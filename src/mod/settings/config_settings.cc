// PLAY's configuration controls, adapted to the foundation's nested Config and existing feature/save owners.
// Shared settings catalog; shortcut and custom-file workflows have their own adapters.
#include "config_settings.h"
#include "config.h"
#include "config_filters.h"
#include "confirmation_settings.h"
#include "features/features.h"
#include "features/galaxy.h"
#include "features/notification_audio.h"
#include "frame.h"
#include "game/notifications.h"
#include "image_file_setting.h"
#include "image_path_entry.h"
#include "native_controls.h"
#include "runtime_config.h"
#include "text_entry.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace mod_settings
{
namespace
{
  using Available = std::function<bool()>;
  using Timing    = ApplyTiming;
  std::vector<std::shared_ptr<void>> models;
  std::vector<ConfigNumberControl>   numbers;
  std::shared_ptr<ImageFileSetting>  loading_image_picker;
  bool                               loading_image_poll_registered = false;

  void PollLoadingImagePicker()
  {
    if (loading_image_picker && loading_image_picker->Poll())
      native::RequestPageRefresh();
  }

  template <class T, class... Args> std::shared_ptr<T> Keep(Args&&... args)
  {
    auto value = std::make_shared<T>(std::forward<Args>(args)...);
    models.push_back(value);
    return value;
  }

  const Config& Defaults()
  {
    static const auto defaults = [] {
      toml::table effective;
      return Config::Parse(toml::table{}, effective, false);
    }();
    return *defaults;
  }

  bool Installed(std::string_view name)
  {
    return std::ranges::any_of(FeatureInstallations(), [name](const FeatureInstallation& feature) {
      return feature.name && name == feature.name && feature.state == InstallState::kInstalled;
    });
  }
  Available Owner(const char* name)
  {
    return [name] { return Installed(name); };
  }
  Available Either(Available first, Available second)
  {
    return [first, second] { return first() || second(); };
  }

  template <class T> config_edit::Value Serialized(const T& value)
  {
    if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::string>)
      return value;
    else if constexpr (std::is_integral_v<T>)
      return static_cast<int64_t>(value);
    else
      return static_cast<double>(value);
  }

  template <class T, class Read, class Write, class Serialize>
  ValueDefinition<T> MakeConfigDefinition(const char* section, const char* key, const char* label, T default_value,
                                          Read read, Write write, Available available, Timing timing, const char* help,
                                          const char* unit, Serialize serialize)
  {
    // Lazy initialization avoids reading Config or the game during catalog construction.
    auto               requested = std::make_shared<std::optional<T>>();
    ValueDefinition<T> definition{std::string("community_mod.") + section + "." + key, label,
                                  [read, requested, available, timing] {
                                    if (!available())
                                      return ValueReadResult<T>{};
                                    if (timing == Timing::Restart && !*requested)
                                      *requested = read();
                                    return ValueReadResult<T>::Known(timing == Timing::Restart ? **requested : read(),
                                                                     1);
                                  },
                                  [write, requested, available, timing](T value, uint64_t generation) {
                                    if (generation != 1 || !available())
                                      return ApplyResult::Rejected;
                                    if (timing == Timing::Restart)
                                      *requested = std::move(value);
                                    else if (!write(value))
                                      return ApplyResult::Rejected;
                                    return ApplyResult::Applied;
                                  }};
    definition.metadata      = {section, key, help, unit, timing};
    definition.default_value = std::move(default_value);
    if (timing == Timing::Restart)
      definition.active_value = [read]() -> std::optional<T> { return read(); };
    runtime_config::RegisterSetting(section, key);
    definition.persist = [section, key, serialize](const T& value) {
      runtime_config::RegisterSetting(section, key);
      runtime_config::SaveSetting(section, key, serialize(value),
                                  std::is_floating_point_v<T> ? std::chrono::milliseconds(250)
                                                              : std::chrono::milliseconds{});
    };
    definition.save_status = [section, key, serialize](const T& value) {
      return runtime_config::GetSaveStatus(section, key, serialize(value));
    };
    return definition;
  }

  template <class T, class Read, class Write>
  ValueDefinition<T> MakeConfigDefinition(const char* section, const char* key, const char* label, T default_value,
                                          Read read, Write write, Available available, Timing timing, const char* help,
                                          const char* unit = "")
  {
    return MakeConfigDefinition<T>(section, key, label, std::move(default_value), read, write, std::move(available),
                                   timing, help, unit, [](const T& value) { return Serialized(value); });
  }

  template <class Access>
  void Boolean(PageCatalog& catalog, const char* page, const char* section, const char* key, const char* label,
               Access access, Available available, Timing timing, const char* help, std::function<void()> changed = {})
  {
    auto definition = MakeConfigDefinition<bool>(
        section, key, label, access(Defaults()), [access] { return access(Config::Get()); },
        [access, changed](bool value) {
          const bool different  = access(Config::Get()) != value;
          access(Config::Get()) = value;
          if (different && changed)
            changed();
          return true;
        },
        available, timing, help);
    catalog.AddBoolean(page, *Keep<BooleanSetting>(std::move(definition)));
  }

  template <class T> std::string ExactNumberText(T value)
  {
    char       text[128];
    const auto written = [&] {
      if constexpr (std::is_floating_point_v<T>)
        return std::to_chars(text, text + sizeof(text), value, std::chars_format::general,
                             std::numeric_limits<T>::max_digits10);
      else
        return std::to_chars(text, text + sizeof(text), value);
    }();
    return written.ec == std::errc{} ? std::string(text, written.ptr) : std::string{};
  }

  // The PLAY ExactEntry workflow, reusing the current host's page custody.
  template <class Parse>
  void ExactText(PageCatalog& catalog, const char* page, const std::shared_ptr<ValueSetting<std::string>>& setting,
                 Parse parse)
  {
    auto action           = Keep<ActionSetting>();
    action->identity      = setting->id() + ".exact";
    action->label         = setting->label();
    action->metadata      = &setting->metadata();
    action->reset         = [setting] { setting->Reset(setting->Observe()); };
    action->retry         = [setting] { setting->RetrySave(setting->Observe()); };
    action->prepare_reset = [setting] { return PrepareReset(*setting); };
    action->status        = [setting] { return setting->status_text(); };
    action->save_status   = [setting] { return setting->save_status(); };
    action->default_label = [setting] { return setting->default_value().value_or(""); };
    action->read          = [setting](size_t) {
      const auto observed = setting->Observe();
      auto       text     = observed.state.known() ? *observed.state.value : "Unavailable";
      if (text.empty())
        text = "(empty)";
      const auto status = setting->status_text();
      if (!status.empty())
        text += "\n" + status;
      return ActionSetting::Presentation{setting->label(), "Edit", std::move(text), observed.state.known()};
    };
    action->invoke = [setting, parse](size_t) {
      const auto observed = setting->Observe();
      const auto visit    = native::CurrentPageVisit();
      if (!observed.state.known() || !native::IsPageVisitCurrent(visit))
        return;
      const auto initial = *observed.state.value;
      auto       help    = setting->metadata().description;
      if (const auto value = setting->default_value())
        help += "\nDefault: " + (value->empty() ? std::string("(empty)") : *value);
      OpenTextEntry({.title = setting->label(),
                     .value = initial,
                     .help  = std::move(help),
                     .validate =
                         [parse](std::string_view text) {
                           return parse(text) ? std::string{} : std::string("Enter a valid value.");
                         },
                     .commit =
                         [setting, observed, visit, parse, initial](std::string_view text) {
                           const auto value = parse(text);
                           if (!value || !native::IsPageVisitCurrent(visit))
                             return false;
                           const auto result =
                               setting->SetFromUser(text == initial ? *observed.state.value : *value, observed).outcome;
                           return result == Outcome::AppliedVerified || result == Outcome::Unchanged;
                         }});
    };
    catalog.AddAction(page, *action);
  }
  template <class T> std::optional<T> ParseExactNumber(std::string_view text, T minimum, T maximum)
  {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == text.npos)
      return {};
    text = text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
    T value{};
    if constexpr (std::is_floating_point_v<T>) {
      // Keep compatibility with the supported macOS runtime's missing floating from_chars implementation.
      if (text.front() == '+' || text.find_first_not_of("0123456789.eE+-") != text.npos)
        return {};
      std::istringstream input{std::string(text)};
      input.imbue(std::locale::classic());
      input >> std::noskipws >> value;
      if (!input || input.peek() != std::char_traits<char>::eof() || !std::isfinite(value))
        return {};
      if (value == 0 && text.substr(0, text.find_first_of("eE")).find_first_not_of("-0.") != text.npos)
        return {};
    } else {
      const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
      if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        return {};
    }
    return value >= minimum && value <= maximum ? std::optional<T>(value) : std::nullopt;
  }
  template <class T>
  std::shared_ptr<ActionSetting> ExactNumber(PageCatalog& catalog, const char* page,
                                             const std::shared_ptr<ValueSetting<T>>& setting, T minimum, T maximum)
  {
    auto action           = Keep<ActionSetting>();
    action->identity      = setting->id() + ".exact";
    action->label         = setting->label();
    action->metadata      = &setting->metadata();
    action->prepare_reset = [setting] { return PrepareReset(*setting); };
    action->reset         = [setting] { setting->Reset(setting->Observe()); };
    action->retry         = [setting] { setting->RetrySave(setting->Observe()); };
    action->status        = [setting] { return setting->status_text(); };
    action->save_status   = [setting] { return setting->save_status(); };
    action->default_label = [setting] {
      return setting->default_value() ? ExactNumberText(*setting->default_value()) : "";
    };
    action->read = [setting](size_t) {
      const auto state = setting->Observe().state;
      return ActionSetting::Presentation{setting->label(), "Edit",
                                         state.known() ? ExactNumberText(*state.value) : "Unavailable", state.known()};
    };
    action->invoke = [setting, minimum, maximum](size_t) {
      const auto snapshot = setting->Observe();
      const auto visit    = native::CurrentPageVisit();
      if (!snapshot.state.known() || !native::IsPageVisitCurrent(visit))
        return;
      const auto initial = ExactNumberText(*snapshot.state.value);
      auto       help    = setting->metadata().description + "\nMinimum: " + ExactNumberText(minimum)
                           + ". Maximum: " + ExactNumberText(maximum) + ".";
      if constexpr (std::is_integral_v<T>)
        help += " Whole numbers only.";
      OpenTextEntry(
          {.title = setting->label(),
           .value = initial,
           .help  = std::move(help),
           .validate =
               [minimum, maximum](std::string_view text) {
                 return ParseExactNumber<T>(text, minimum, maximum)
                            ? std::string{}
                            : std::string("Enter a valid number in the allowed range.");
               },
           .commit =
               [setting, snapshot, visit, initial, minimum, maximum](std::string_view text) {
                 const auto parsed = ParseExactNumber<T>(text, minimum, maximum);
                 if (!parsed || !native::IsPageVisitCurrent(visit))
                   return false;
                 const auto result =
                     setting->SetFromUser(text == initial ? *snapshot.state.value : *parsed, snapshot).outcome;
                 return result == Outcome::AppliedVerified || result == Outcome::Unchanged;
               }});
    };
    catalog.AddAction(page, *action);
    return action;
  }

  template <class T, class Access>
  void Number(PageCatalog& catalog, const char* page, const char* section, const char* key, const char* label,
              Access access, Available available, Timing timing, const char* help, const char* unit, T minimum,
              T maximum, float slider_maximum = 0, float step = 1, std::function<bool(T)> apply = {},
              Available slider_enabled = {})
  {
    auto definition = MakeConfigDefinition<T>(
        section, key, label, access(Defaults()), [access] { return access(Config::Get()); },
        [access, minimum, maximum, apply](T value) {
          if (value < minimum || value > maximum)
            return false;
          if constexpr (std::is_floating_point_v<T>)
            if (!std::isfinite(value))
              return false;
          if (apply)
            return apply(value);
          access(Config::Get()) = value;
          return true;
        },
        available, timing, help, unit);
    if constexpr (std::is_floating_point_v<T>) {
      const auto read = definition.read;
      definition.read = [read] {
        auto value = read();
        if (value.known() && !std::isfinite(*value.value))
          return ValueReadResult<T>{Availability::Unavailable, {}, 0, UnavailableReason::InvalidValue};
        return value;
      };
    }
    const auto write = definition.write;
    definition.write = [write, minimum, maximum](T value, uint64_t generation) {
      if (value < minimum || value > maximum)
        return ApplyResult::Rejected;
      if constexpr (std::is_floating_point_v<T>)
        if (!std::isfinite(value))
          return ApplyResult::Rejected;
      return write(value, generation);
    };
    auto canonical = Keep<ValueSetting<T>>(definition);
    numbers.push_back({page, canonical.get(), double(minimum), double(maximum)});
    auto editor = ExactNumber(catalog, page, canonical, minimum, maximum);
    if constexpr (std::is_same_v<T, float>)
      if (slider_maximum > minimum) {
        auto slider          = Keep<SliderSetting>(std::move(definition), minimum, slider_maximum, step,
                                                   slider_enabled ? slider_enabled : available, SliderLabel::Value, 2);
        slider->exact_editor = editor.get();
        catalog.AddSlider(page, *slider);
      }
  }

  template <class Read, class Write>
  void Choice(PageCatalog& catalog, const char* page, const char* section, const char* key, const char* label,
              int default_value, Read read, Write write, Available available, Timing timing, const char* help,
              std::vector<std::string> labels, std::vector<std::string> values)
  {
    auto definition = MakeConfigDefinition<int>(
        section, key, label, default_value, read,
        [write, count = values.size()](int value) { return value >= 0 && size_t(value) < count && write(value); },
        available, timing, help, "", [values](int value) -> config_edit::Value { return values.at(value); });
    catalog.AddChoice(page, *Keep<ChoiceSetting>(std::move(definition), std::move(labels)));
  }
} // namespace

void RegisterConfigSettings(PageCatalog& catalog)
{
  if (!models.empty())
    return;
  constexpr auto root = "community_mod.settings";
  for (const auto& [id, label] :
       std::initializer_list<std::pair<const char*, const char*>>{{"controls", "Controls"},
                                                                  {"camera", "Camera"},
                                                                  {"interface", "Interface & Scale"},
                                                                  {"travel", "Map & Travel"},
                                                                  {"previews", "Previews & Cargo"},
                                                                  {"claims", "Claims & Confirmations"},
                                                                  {"officers", "Officers & Ships"},
                                                                  {"chat", "Chat"},
                                                                  {"notifications", "Notifications"},
                                                                  {"hud", "HUD Buttons"},
                                                                  {"labels", "Fleet Labels"},
                                                                  {"loading", "Loading Screens"},
                                                                  {"advanced", "Advanced & Configuration"},
                                                                  {"galaxy", "Galaxy Labels"},
                                                                  {"audio", "Audio Alerts"}})
    catalog.AddPage(std::string("community_mod.") + id, label, root);

  const auto hotkeys = Owner("Hotkeys"), zoom = Owner("Zoom"), pan = Owner("Pan"), scale = Owner("UiScale");
  const auto controls = Either(hotkeys, Either(zoom, pan)), extended = Either(hotkeys, zoom);
  const auto protections =
      Either(Owner("ActionQueueRecovery"), Either(Owner("ThinQueueProtection"), Owner("QueueAddressGuard")));
  const auto      queue = Either(hotkeys, Either(protections, Owner("QueueSwitch")));
  const auto      misc = Owner("ShopLimits"), toast = Owner("Toasts");
  const Available incoming = [toast] { return toast() && IncomingPlayerAttackAvailable(); };
#if _WIN32
  const auto donation = Owner("DonationLimit");
#else
  const auto donation = misc;
#endif
  constexpr float positive = std::numeric_limits<float>::min(), largest = std::numeric_limits<float>::max();
  constexpr int   integer = std::numeric_limits<int>::max();

#define ACCESS(group, key) [](auto& value) -> auto& { return value.group.key; }
#define BOOL(page, group, key, label, owner, timing, help)                                                             \
  Boolean(catalog, "community_mod." page, #group, #key, label, ACCESS(group, key), owner, Timing::timing, help)
#define FLOAT(page, group, key, label, owner, timing, help, unit, low, high, slider, step)                             \
  Number<float>(catalog, "community_mod." page, #group, #key, label, ACCESS(group, key), owner, Timing::timing, help,  \
                unit, low, high, slider, step)
#define INT(page, group, key, label, owner, timing, help, unit, low, high)                                             \
  Number<int>(catalog, "community_mod." page, #group, #key, label, ACCESS(group, key), owner, Timing::timing, help,    \
              unit, low, high)

  BOOL("controls", control, hotkeys_enabled, "Enable mod shortcuts", controls, Live,
       "Use configured mod keyboard and mouse shortcuts.");
  BOOL("controls", control, hotkeys_extended, "Extended shortcut actions", extended, Restart,
       "Change extended action availability after restart.");
  BOOL("controls", control, use_scopely_hotkeys, "Use native game shortcuts", controls, Restart,
       "Use the game's shortcuts instead of mod bindings.");
  Boolean(catalog, "community_mod.controls", "control", "queue_enabled", "Enable action queue",
          ACCESS(control, queue_enabled), queue, Timing::Live, "Allow queued actions.", ResetQueueRecoveryTracking);
  Boolean(catalog, "community_mod.controls", "control", "kirshara_fixes", "Kirshara queue protections",
          ACCESS(control, kirshara_fixes), protections, Timing::Live,
          "Protect queued actions when targets or addresses change.", ResetQueueRecoveryTracking);
  BOOL("controls", control, primary_requires_visible_target, "Require visible primary target", hotkeys, Live,
       "Require a visible target for the primary action shortcut.");
  INT("controls", control, select_timer, "Ship selection interval", hotkeys, Live,
      "Maximum interval between repeated ship-selection shortcuts.", "ms", 0, integer);

  FLOAT("camera", graphics, keyboard_zoom_speed, "Keyboard zoom speed", zoom, Live, "Speed of keyboard zoom.",
        "distance/s", 0, largest, 1000, 25);
  FLOAT("camera", graphics, zoom, "System zoom-out distance", zoom, Live, "Maximum system-camera distance.", "distance",
        positive, largest, 10000, 250);
  FLOAT("camera", graphics, haven_zoom, "Haven zoom-out distance", Owner("HavenZoom"), Live,
        "Zero uses the native Haven limit.", "distance", 0, largest, 6000, 250);
  FLOAT("camera", graphics, interior_zoom_scale, "Station interior zoom-out", Owner("InteriorZoom"), Live,
        "A multiplier of one uses the native interior limit.", "multiplier", 1, largest, 4, .1f);
  FLOAT("camera", graphics, default_system_zoom, "Default system zoom", zoom, Live,
        "Distance used by reset and entering a system.", "distance", 0, largest, 6000, 50);
  FLOAT("camera", graphics, system_pan_momentum_falloff, "Pan glide", pan, Live,
        "Retained motion per pan update; higher values glide longer.", "ratio", 0, .999999f, .99f, .01f);
  BOOL("camera", graphics, use_presets_as_default, "Use last preset as default zoom", zoom, Live,
       "Selecting a preset updates the session's default zoom.");
  FLOAT("camera", graphics, system_zoom_preset_1, "Zoom preset 1", zoom, Live, "Distance for zoom preset 1.",
        "distance", 0, largest, 6000, 50);
  FLOAT("camera", graphics, system_zoom_preset_2, "Zoom preset 2", zoom, Live, "Distance for zoom preset 2.",
        "distance", 0, largest, 6000, 50);
  FLOAT("camera", graphics, system_zoom_preset_3, "Zoom preset 3", zoom, Live, "Distance for zoom preset 3.",
        "distance", 0, largest, 6000, 50);
  FLOAT("camera", graphics, system_zoom_preset_4, "Zoom preset 4", zoom, Live, "Distance for zoom preset 4.",
        "distance", 0, largest, 6000, 50);
  FLOAT("camera", graphics, system_zoom_preset_5, "Zoom preset 5", zoom, Live, "Distance for zoom preset 5.",
        "distance", 0, largest, 6000, 50);
  BOOL("camera", graphics, hide_haven_water, "Hide Haven water", Owner("HavenZoom"), Live,
       "Hide water while in Haven.");

  FLOAT("interface", graphics, ui_scale, "Interface scale", scale, Live, "Zero uses native interface scaling.",
        "multiplier", 0, largest, 2, .05f);
  FLOAT("interface", graphics, ui_scale_step, "Scale shortcut step", scale, Live,
        "Amount added or removed by scale shortcuts.", "multiplier", positive, largest, .5f, .01f);
  Number<float>(catalog, "community_mod.interface", "graphics", "ui_scale_ship", "Ship scale",
                ACCESS(graphics, ui_scale_ship), scale, Timing::Live,
                "Multiplier for ship models, including ships already loaded.", "multiplier", positive, largest, 3, .05f,
                SetUiShipScale);
  FLOAT("interface", graphics, ui_scale_viewer, "Viewer scale", scale, ReopenView,
        "Reopen ship and officer viewers after changing their scale.", "multiplier", 0, largest, 3, .05f);
  FLOAT("interface", graphics, fr_scale, "Planet backdrop scale", zoom, Restart,
        "Restart to avoid rescaling already modified backdrops.", "multiplier", positive, largest, 4, .1f);
#if _WIN32
  BOOL("interface", graphics, allow_cursor, "Allow cursor changes", Either(scale, Owner("Cursor")), Live,
       "Allow the game to change the cursor.");
  BOOL("interface", graphics, free_resize, "Free window resizing", Owner("FreeResize"), Restart,
       "Allow arbitrary window sizes on Windows.");
#endif
  const Available aai = [] {
    return std::ranges::any_of(FeatureInstallations(), [](const FeatureInstallation& feature) {
      return feature.name && std::string_view(feature.name) == "BattleLogSummary"
             && feature.state == InstallState::kInstalled && feature.frame_ready;
    });
  };
  BOOL("interface", ui, battle_log_summary_preview, "Battle Analysis (AAI)", aai, Live,
       "Show Battle Analysis on combat reports. Turning this off closes analysis on the next update. "
       "Requires an AAI-enabled build and battle-log hooks; reopen the report after enabling it.");
  BOOL("interface", ui, disable_escape_exit, "Block Escape exit prompt", hotkeys, Live,
       "Block the exit prompt except for an enabled double-tap.");
  INT("interface", ui, disable_escape_exit_timer, "Escape double-tap interval", hotkeys, Live,
      "Zero disables double-tap exit.", "ms", 0, integer);
  BOOL("interface", ui, disable_move_keys, "Disable keyboard movement", pan, Live,
       "Disable configured movement shortcuts.");
  BOOL("interface", ui, only_show_first_popup, "Show only the first startup popup", Owner("SkipRevealSequence"), Live,
       "Suppress subsequent startup offers and interstitials.");
  BOOL("interface", ui, focus_search, "Enable search-focus shortcut", hotkeys, Live,
       "Enable focus search on supported game screens.");
  BOOL("travel", graphics, galactic_anomaly_timer, "Galactic Anomaly timer", Owner("AnomalyTimer"), Live,
       "Show the remaining lifetime on supported anomalies.");

  BOOL("previews", ui, disable_preview_locate, "Block Locate with an open preview", hotkeys, Live,
       "Keep Locate from dismissing a target preview.");
  BOOL("previews", ui, disable_preview_recall, "Block Recall with an open preview", hotkeys, Live,
       "Block Recall while a target preview is open.");
  BOOL("previews", ui, show_cargo_default, "Automatically open cargo", hotkeys, Live,
       "Automatically expand supported preview cargo.");
  BOOL("previews", ui, show_player_cargo, "Player cargo", hotkeys, Live, "Automatically show cargo for player fleets.");
  BOOL("previews", ui, show_station_cargo, "Station cargo", hotkeys, Live, "Automatically show cargo for stations.");
  BOOL("previews", ui, show_hostile_cargo, "Hostile cargo", hotkeys, Live, "Automatically show cargo for hostiles.");
  BOOL("previews", ui, show_armada_cargo, "Armada cargo", hotkeys, Live,
       "Automatically show cargo for Armada targets.");
#if _WIN32
  BOOL("previews", ui, instant_cargo_counter, "Instant ship cargo counter", Owner("InstantCargo"), Live,
       "Remove the animated cargo-counter delay.");
#endif
  BOOL("previews", ui, format_cargo_values, "Format cargo values", Owner("CargoFormat"), Restart,
       "Use configured decimal precision for abbreviated cargo.");
  INT("previews", ui, cargo_significant_decimals, "Cargo decimal places", Owner("CargoFormat"), ReopenView,
      "Decimal places for abbreviated cargo amounts.", "places", 0, 6);
  BOOL("previews", ui, keep_base_card_open, "Keep base card open", Owner("BaseCard"), Live,
       "Keep the base card open after supported actions.");
  BOOL("previews", ui, highlight_opc_fleets, "Highlight unprotected cargo", Owner("OpcIndicators"), Live,
       "Highlight fleets over protected cargo.");
  BOOL("previews", ui, fleet_hud_opc_eta, "Time until unprotected cargo", Owner("OpcIndicators"), Live,
       "Estimate time until cargo exceeds protection.");
  BOOL("previews", ui, show_ship_tech_indicators, "Ship technology indicators", Owner("ShipTechIndicators"), Live,
       "Show equipped ship technology in ship selection.");
  BOOL("previews", ui, show_ship_tech_indicator_backgrounds, "Technology indicator backgrounds",
       Owner("ShipTechIndicators"), Live, "Show backgrounds behind ship technology indicators.");

  BOOL("claims", ui, auto_open_bulk_claim_flyout, "Automatically open bulk claims", Owner("GiftsAutoOpen"), Live,
       "Open the bulk-claim drawer on supported screens.");
  BOOL("claims", ui, daily_bulk_claim_toggle_default_on, "Daily bulk claims on by default", Owner("DailyClaimFilter"),
       Restart, "Initial state of the session's faction Claim All toggle.");
  BOOL("claims", ui, always_skip_reveal_sequence, "Skip reward reveal sequences", Owner("SkipRevealSequence"), Live,
       "Show reward summaries directly.");
  catalog.AddBoolean("community_mod.claims", ForbiddenTechConfirmationSetting());
  BOOL("claims", ui, auto_confirm_reveal_ability, "Automatically confirm Excelsior reveal", Owner("RevealConfirm"),
       Live, "Skip the Excelsior Decloaking Deflector confirmation.");
  BOOL("claims", ui, disable_exchange_all, "Hide artifact Exchange All", Owner("ArtifactExchange"), ReopenView,
       "Reopen the artifact popup to refresh this button.");
  INT("claims", ui, extend_chest_purchase_max, "Maximum chest quantity", misc, Restart,
      "Zero disables extension; the effective extension is capped at 160.", "chests", 0, integer);
  BOOL("claims", ui, extend_donation_slider, "Extend donation slider", donation, ReopenView,
       "Use the configured donation limit in supported dialogs.");
  INT("claims", ui, extend_donation_max, "Maximum donation quantity", donation, ReopenView,
      "Reopen the donation dialog; nonpositive values remove the cap.", "donations", std::numeric_limits<int>::min(),
      integer);

  BOOL("officers", ui, double_click_to_assign_ship, "Double-click to assign ship", Owner("DoubleClickAssign"), Live,
       "Assign the selected ship by double-click or Enter.");
  BOOL("officers", ui, officer_sort, "Below Deck Ability sorting", Owner("OfficerSort"), Restart,
       "Add Below Deck Ability sorting to supported screens.");
  BOOL("officers", ui, arrow_keys_to_select_ship, "Arrow keys select ships", hotkeys, Live,
       "Use configured left and right controls to select ships.");
  BOOL("officers", ui, dock_syncmanagement, "Keep dock selection in sync", hotkeys, Live,
       "Keep dock and Manage Ships selections aligned.");
  BOOL("officers", buffs, use_out_of_dock_power, "Use out-of-dock power", Owner("OutOfDockPower"), Restart,
       "Use deployed-ship power for supported displays.");
  BOOL("officers", ui, reverse_haven_history, "Newest Haven help first", Owner("HavenHistory"), ReopenView,
       "Show the newest Haven help records first.");
  BOOL("officers", ui, allow_officer_preset_reordering, "Reorder officer presets", Owner("OfficerPresetReorder"),
       Restart, "Native preset integration is currently unavailable.");
  BOOL("chat", ui, show_player_title, "Show player titles", Owner("ChatTitles"), ReopenView,
       "Reopen chat to rebuild existing message titles.");
  BOOL("chat", ui, disable_galaxy_chat, "Disable galaxy chat", Owner("Chat"), Restart,
       "Disable galaxy chat tabs and previews.");
  BOOL("chat", ui, disable_veil_chat, "Disable Veil chat", Owner("Chat"), Restart,
       "Disable Veil chat tabs and previews where available.");
  BOOL("labels", graphics, ship_hotkey_badges, "Ship shortcut badges", Owner("ShipShortcutBadges"), Live,
       "Show configured ship shortcut hints.");
  BOOL("loading", graphics, loader_enabled, "Custom loading background", Owner("LoginScreen"), Restart,
       "Replace the login background with the configured image.");
  BOOL("loading", graphics, loader_transition, "Custom transition background", Owner("TransitionScreen"), Restart,
       "Use the configured image during transitions.");
  BOOL("loading", graphics, loader_transition_black, "Black transition background", Owner("TransitionScreen"), Restart,
       "Use black instead of the configured transition image.");
  BOOL("loading", graphics, loader_tip_enabled, "Loading tips", Owner("LoadingTips"), Restart,
       "Show loading tips on supported screens.");
  FLOAT("loading", graphics, loader_logo_scale, "Loading logo scale",
        Either(Owner("LoginScreen"), Owner("TransitionScreen")), Restart, "Multiplier for the loading-screen logo.",
        "multiplier", positive, largest, 3, .1f);
  auto image                  = Keep<ValueSetting<std::string>>(MakeConfigDefinition<std::string>(
      "graphics", "loader_image", "Loading image path", Defaults().graphics.loader_image,
      [] { return Config::Get().graphics.loader_image; },
      [](const std::string& path) {
        Config::Get().graphics.loader_image = path;
        return true;
      },
      Either(Owner("LoginScreen"), Owner("TransitionScreen")), Timing::Restart,
      "Path to an image readable by the game. Empty uses the embedded fallback. This does not move or copy the "
      "image."));
  auto image_action           = Keep<ActionSetting>();
  image_action->identity      = image->id() + ".exact";
  image_action->label         = image->label();
  image_action->metadata      = &image->metadata();
  image_action->prepare_reset = [image] { return PrepareReset(*image); };
  image_action->reset         = [image] { image->Reset(image->Observe()); };
  image_action->retry         = [image] { image->RetrySave(image->Observe()); };
  image_action->status        = [image] { return image->status_text(); };
  image_action->save_status   = [image] { return image->save_status(); };
  image_action->default_label = [image] { return *image->default_value(); };
  image_action->read          = [image](size_t) {
    const auto value = image->Observe();
    return ActionSetting::Presentation{image->label(), "Edit", value.state.known() ? *value.state.value : "Unavailable",
                                       value.state.known()};
  };
  loading_image_picker    = Keep<ImageFileSetting>(image, *image_action);
  const auto choose_image = image_action->invoke;
  image_action->invoke    = [image, choose_image](size_t index) {
    if (!image->Observe().state.known())
      return;
    // Catalog construction stays pure. Reuse the existing anchor only.
    if (!loading_image_poll_registered && FrameDeliveryAvailable())
      loading_image_poll_registered = RegisterFrameCallback(PollLoadingImagePicker, "LoadingImagePicker");
    if (loading_image_poll_registered && FrameDeliveryAvailable())
      choose_image(index);
    else {
      const auto visit   = native::CurrentPageVisit();
      auto       request = ImagePathEntry(image, [visit] { return native::IsPageVisitCurrent(visit); });
      if (!OpenTextEntry(std::move(request)))
        LoadingImagePathValidator().Cancel();
    }
  };
  catalog.AddAction("community_mod.loading", *image_action);
  BOOL("galaxy", graphics, galaxy_extended_selection, "Extended system selection", galaxy_controls::SelectionAvailable,
       Live, "Extend supported Far-tier Galaxy selection.");
  Boolean(catalog, "community_mod.galaxy", "graphics", "galaxy_station_housing", "Station housing",
          ACCESS(graphics, galaxy_station_housing), galaxy_controls::HousingAvailable, Timing::Live,
          "Show housing-capable systems, not free station slots.", galaxy_controls::Refresh);
  Boolean(catalog, "community_mod.galaxy", "graphics", "galaxy_multi_select", "Select multiple overlays",
          ACCESS(graphics, galaxy_multi_select), galaxy_controls::LabelsAvailable, Timing::Live,
          "Select several Galaxy overlays together.", galaxy_controls::Refresh);
#undef BOOL
#undef FLOAT
#undef INT
#undef ACCESS

  Choice(
      catalog, "community_mod.travel", "ui", "auto_confirm_instant_warp", "Instant warp confirmation", 0,
      [] { return int(Config::Get().ui.auto_confirm_instant_warp); },
      [](int value) {
        Config::Get().ui.auto_confirm_instant_warp = static_cast<InstantWarpConfirmation>(value);
        return true;
      },
      Owner("InstantWarp"), Timing::Live, "Choose whether the selected warp or jump route is confirmed automatically.",
      {"Ask", "Automatically warp", "Automatically jump"}, {"none", "warp", "jump"});

  constexpr std::array<const char*, 4> overlay_keys{"galaxy_overlay_default", "galaxy_overlay_mining",
                                                    "galaxy_overlay_hostiles", "galaxy_overlay_hazards"};
  constexpr std::array<const char*, 4> overlay_labels{"Default (system names)", "Mining", "Hostiles", "Hazards"};
  for (const int index : {0, 2, 1, 3}) {
    auto definition = MakeConfigDefinition<bool>(
        "graphics", overlay_keys[index], overlay_labels[index], index == 0,
        [index] { return Config::Get().graphics.galaxy_overlays[index]; },
        [index](bool value) {
          galaxy_controls::SetOverlay(index, value);
          return Config::Get().graphics.galaxy_overlays[index] == value;
        },
        galaxy_controls::LabelsAvailable, Timing::Live, "Removing the last overlay restores Default.");
    // SetOverlay owns all affected flags and their registered-key saves, including the fallback Default flag.
    definition.persist = {};
    catalog.AddBoolean("community_mod.galaxy", *Keep<BooleanSetting>(std::move(definition)),
                       [] { return Config::Get().graphics.galaxy_multi_select; });
  }
  for (const bool minor : {false, true}) {
    auto profile = [minor](auto& config) -> auto& {
      return minor ? config.graphics.galaxy_label_minor : config.graphics.galaxy_label_major;
    };
    Choice(
        catalog, "community_mod.galaxy", "graphics", minor ? "galaxy_label_minor_detail" : "galaxy_label_major_detail",
        minor ? "Minor system labels" : "Major system labels", 0,
        [profile] { return int(profile(Config::Get()).mode); },
        [profile](int value) {
          profile(Config::Get()).mode = static_cast<galaxy_controls::ZoomMode>(value);
          galaxy_controls::Refresh();
          return true;
        },
        galaxy_controls::LabelsAvailable, Timing::Live, "Choose when Galaxy system labels are visible.",
        {"Native", "Always", "Threshold"}, {"native", "always", "threshold"});
    Number<float>(
        catalog, "community_mod.galaxy", "graphics",
        minor ? "galaxy_label_minor_threshold" : "galaxy_label_major_threshold",
        minor ? "Minor label zoom threshold" : "Major label zoom threshold",
        [profile](auto& config) -> auto& { return profile(config).threshold; }, galaxy_controls::LabelsAvailable,
        Timing::Live, "Zoom threshold from zero to one for Galaxy system labels.", "ratio", 0, 1, 1, .01f, {},
        [profile] {
          return galaxy_controls::LabelsAvailable()
                 && profile(Config::Get()).mode == galaxy_controls::ZoomMode::Threshold;
        });
  }
  for (const bool player : {true, false}) {
    auto profile = [player](auto& config) -> auto& {
      return player ? config.graphics.zoom_label_player : config.graphics.zoom_label_non_player;
    };
    Choice(
        catalog, "community_mod.labels", "graphics",
        player ? "zoom_label_player_detail" : "zoom_label_non_player_detail",
        player ? "Player label detail" : "Non-player label detail", 0,
        [profile] { return int(profile(Config::Get()).detail); },
        [profile](int value) {
          profile(Config::Get()).detail = static_cast<FleetLabelDetail>(value);
          RefreshFleetLabelControls();
          return true;
        },
        Owner("FleetLabels"), Timing::Live, "Choose the information shown on system fleet labels.",
        {"Native", "Expanded", "Compact", "Threshold"}, {"native", "expanded", "compact", "threshold"});
    Number<float>(
        catalog, "community_mod.labels", "graphics",
        player ? "zoom_label_player_threshold" : "zoom_label_non_player_threshold",
        player ? "Player detail threshold" : "Non-player detail threshold",
        [profile](auto& config) -> auto& { return profile(config).zoom_threshold; }, Owner("FleetLabels"), Timing::Live,
        "Threshold label-detail ratio from zero to one.", "ratio", 0, 1, 1, .01f,
        [profile](float value) {
          profile(Config::Get()).zoom_threshold = value;
          RefreshFleetLabelControls();
          return true;
        },
        [profile] {
          return Installed("FleetLabels") && profile(Config::Get()).detail == FleetLabelDetail::kThreshold;
        });
  }
  struct Hud {
    const char*          key;
    const char*          label;
    MissionHudVisibility Config::Ui::* value;
  };
  for (const auto& item :
       std::initializer_list<Hud>{{"hud_q_trials", "Q's Trials", &Config::Ui::hud_q_trials},
                                  {"hud_field_training", "Field Training", &Config::Ui::hud_field_training},
                                  {"hud_outposts", "Outposts", &Config::Ui::hud_outposts},
                                  {"hud_missions", "Missions", &Config::Ui::hud_missions}})
    Choice(
        catalog, "community_mod.hud", "ui", item.key, item.label, int(Defaults().ui.*item.value),
        [item] { return int(Config::Get().ui.*item.value); },
        [item](int value) {
          if (!mission_hud::CanChange())
            return false;
          Config::Get().ui.*item.value = static_cast<MissionHudVisibility>(value);
          mission_hud::Refresh();
          return true;
        },
        mission_hud::Available, Timing::Live, "Choose when this HUD button is visible.", {"Auto", "Always", "Never"},
        {"auto", "always", "never"});

  static constexpr std::array faction_names{"federation", "klingon", "romulan"};
  const auto                  faction_text = [](int mask) {
    if (!mask)
      return std::string("none");
    if (mask == 7)
      return std::string{}; // Source blank retains native/all claiming.
    std::string text;
    for (int i = 0; i < 3; ++i)
      if (mask & (1 << i)) {
        if (!text.empty())
          text += ", ";
        text += faction_names[i];
      }
    return text;
  };
  auto factions = MakeConfigDefinition<int>(
      "ui", "daily_bulk_claim_factions", "Daily bulk-claim factions", 7,
      [] {
        if (Config::Get().ui.daily_bulk_claim_factions.empty())
          return 7;
        int mask = 0;
        for (int i = 0; i < 3; ++i)
          if (std::ranges::find(Config::Get().ui.daily_bulk_claim_factions, faction_names[i])
              != Config::Get().ui.daily_bulk_claim_factions.end())
            mask |= 1 << i;
        return mask;
      },
      [faction_text](int mask) {
        std::vector<std::string> values;
        if (!ParseFactionFilter(faction_text(mask), values))
          return false;
        Config::Get().ui.daily_bulk_claim_factions = std::move(values);
        return true;
      },
      Owner("DailyClaimFilter"), Timing::ReopenView, "Select faction dailies to include in bulk claims.", "",
      [faction_text](int mask) -> config_edit::Value { return faction_text(mask); });
  catalog.AddChoice("community_mod.claims",
                    *Keep<ChoiceSetting>(std::move(factions),
                                         std::vector<std::string>{"Federation", "Klingon", "Romulan"},
                                         ChoiceMode::Multiple));

  const auto filter_page = "community_mod.travel.filters";
  catalog.AddPage(filter_page, "Ship route filters", "community_mod.travel");
  struct RouteFilter {
    const char* key;
    const char* label;
    ShipFilter Config::Ui::* member;
  };
  for (const auto& filter :
       {RouteFilter{"instant_warp_auto_jump", "Always jump with these ships", &Config::Ui::instant_warp_auto_jump},
        RouteFilter{"instant_warp_auto_warp", "Always warp with these ships", &Config::Ui::instant_warp_auto_warp},
        RouteFilter{"instant_warp_always_ask", "Always ask for these ships", &Config::Ui::instant_warp_always_ask}}) {
    auto setting = Keep<ValueSetting<std::string>>(MakeConfigDefinition<std::string>(
        "ui", filter.key, filter.label, FormatShipFilter(Defaults().ui.*filter.member),
        [filter] { return FormatShipFilter(Config::Get().ui.*filter.member); },
        [filter](const std::string& text) {
          ShipFilter value;
          if (!ParseShipFilter(text, value))
            return false;
          Config::Get().ui.*filter.member = std::move(value);
          return true;
        },
        Owner("InstantWarp"), Timing::ReopenView,
        "Comma-separated ship names, or * for every ship. Always ask wins, then jump, then warp, then the default. "
        "Reopen fleet management after changing them."));
    ExactText(catalog, filter_page, setting, [](std::string_view text) -> std::optional<std::string> {
      ShipFilter value;
      return ParseShipFilter(text, value) ? std::optional{FormatShipFilter(value)} : std::nullopt;
    });
  }
  for (const bool notify : {false, true}) {
#if !_WIN32
    if (notify)
      continue; // Preserve the source control's platform scope.
#endif
    auto setting = Keep<ValueSetting<std::string>>(MakeConfigDefinition<std::string>(
        "ui", notify ? "notify_banner_types" : "disabled_banner_types",
        notify ? "Banners allowed to notify" : "Hidden banner types", "",
        [notify] {
          return FormatBannerFilter(notify ? Config::Get().ui.notify_banner_types
                                           : Config::Get().ui.disabled_banner_types);
        },
        [notify](const std::string& text) {
          std::vector<int> types;
          if (!ParseBannerFilter(text, types))
            return false;
          (notify ? Config::Get().ui.notify_banner_types : Config::Get().ui.disabled_banner_types) = std::move(types);
          return true;
        },
        Owner("Toasts"), Timing::Live,
        "Comma-separated banner names from the English example configuration, or All. Unknown names are rejected. "
        "Hiding a banner does not independently disable sound or desktop notifications."));
    ExactText(catalog, "community_mod.notifications", setting, [](std::string_view text) -> std::optional<std::string> {
      std::vector<int> types;
      return ParseBannerFilter(text, types) ? std::optional{FormatBannerFilter(types)} : std::nullopt;
    });
  }
}

std::span<const ConfigNumberControl> ConfigNumberControls()
{ return numbers; }
} // namespace mod_settings
