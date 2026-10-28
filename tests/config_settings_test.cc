// Production catalog/models with synthetic feature readiness, registered-key saves and filesystem paths.
#include "config.h"
#include "config_filters.h"
#include "config_save_fixture.h"
#include "features/features.h"
#include "features/fleet_notification_settings.h"
#include "features/notifications.h"
#include "file.h"
#include "frame.h"
#include "runtime_config.h"
#include "settings/config_settings.h"
#include "settings/confirmation_settings.h"
#include "settings/image_file_picker.h"
#include "settings/mod_pages.h"
#include "settings/native_controls.h"
#include "settings/shortcut_capture.h"
#include "settings/shortcut_catalog.h"
#include "settings/shortcut_settings.h"
#include "settings/text_entry.h"
#include "text.h"

#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <spdlog/spdlog.h>

namespace
{
const ConfigSaveFixtureDirectory              directory;
const std::string                             config_name = "settings.toml", vars_name = "runtime.vars";
size_t                                        path_reads = 0, ship_rescales = 0, queue_resets = 0;
size_t                                        fleet_label_refreshes = 0;
std::vector<FeatureInstallation>              features;
std::set<std::string>                         registered;
std::map<std::string, config_edit::Value>     saved;
bool                                          conflict = false;
std::optional<mod_settings::TextEntryRequest> editor;
mod_settings::native::PageVisitToken          visit = 1;
mod_settings::ShortcutCapture::Keys           raw_held{}, raw_down{};
size_t                                        raw_queries    = 0;
ScreenManagerUpdateCallback                   image_poll     = nullptr;
bool                                          frame_delivery = false, admit_image_poll = true;
size_t                                        frame_queries = 0, frame_registrations = 0, picker_opens = 0;
std::promise<std::string>                     image_picked;

void Check(bool value, const char* message)
{
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
void Enable(const char* name)
{ features.push_back({name, InstallState::kInstalled, true, true}); }

template <class T> T* Find(const mod_settings::PageCatalog& catalog, std::string_view key)
{
  for (const auto& page : catalog.RegisteredPages())
    for (const auto& item : page.items)
      if (const auto* value = std::get_if<T*>(&item)) {
        const auto& metadata = [&]() -> const auto& {
          if constexpr (std::is_same_v<T, mod_settings::BooleanSetting>)
            return (*value)->metadata();
          else
            return (*value)->state().metadata();
        }();
        if (metadata.TomlKey() == key)
          return *value;
      }
  return nullptr;
}
template <class T> mod_settings::ValueSetting<T>* Number(std::string_view key)
{
  for (const auto& entry : mod_settings::ConfigNumberControls())
    if (auto* value = std::get_if<mod_settings::ValueSetting<T>*>(&entry.value);
        value && (*value)->metadata().TomlKey() == key)
      return *value;
  return nullptr;
}
mod_settings::ActionSetting* Exact(std::string_view key)
{
  for (const auto& page : mod_settings::BuildModPages().RegisteredPages())
    for (const auto& item : page.items)
      if (auto* action = std::get_if<mod_settings::ActionSetting*>(&item);
          action && (*action)->metadata && (*action)->metadata->TomlKey() == key)
        return *action;
  return nullptr;
}
mod_settings::ActionSetting& ShortcutRow(Action action, std::string_view suffix)
{
  const auto* setting = mod_settings::ShortcutEditorSetting(action);
  Check(setting != nullptr, "registered shortcut has an editor model");
  const auto id = setting->id() + "." + std::string(suffix);
  for (const auto& page : mod_settings::BuildModPages().RegisteredPages())
    for (auto* row : page.Controls<mod_settings::ActionSetting>())
      if (row->id() == id)
        return *row;
  Check(false, "registered shortcut has its concrete command row");
  std::abort();
}
} // namespace

bool FrameDeliveryAvailable() noexcept
{
  ++frame_queries;
  return frame_delivery;
}
bool RegisterFrameCallback(ScreenManagerUpdateCallback callback, const char*) noexcept
{
  ++frame_registrations;
  if (!admit_image_poll)
    return false;
  image_poll = callback;
  return true;
}
std::future<std::string> OpenImageFilePicker()
{
  ++picker_opens;
  image_picked = {};
  return image_picked.get_future();
}
std::future<std::string> OpenAudioFilePicker()
{
  std::promise<std::string> cancelled;
  cancelled.set_value("");
  return cancelled.get_future();
}

namespace File
{
const std::string& Config()
{ return config_name; }
const std::string& DefaultConfig()
{ return config_name; }
const std::string& Vars()
{ return vars_name; }
std::filesystem::path Path(std::string_view name, bool)
{
  ++path_reads;
  return directory.Root() / name;
}
void MigrateLegacyFiles() {}
} // namespace File
namespace unity::Key
{
bool Pressed(KeyCode key)
{
  ++raw_queries;
  return raw_held[static_cast<int>(key)];
}
bool Down(KeyCode key)
{
  ++raw_queries;
  return raw_down[static_cast<int>(key)];
}
bool IsModified()
{ return false; }
} // namespace unity::Key
std::span<const FeatureInstallation> FeatureInstallations()
{ return features; }
bool IncomingPlayerAttackAvailable()
{ return false; }
#if __APPLE__
// Notification authorization belongs to the native host, outside this catalog fixture.
void InitializeDesktopNotifications() {}
#endif
bool ToastAudioAvailable()
{
  return std::ranges::any_of(features, [](const auto& f) { return std::string_view(f.name) == "Toasts"; });
}
bool FleetNotificationAudioAvailable(FleetNotificationKind)
{ return false; }
void RefreshFleetNotificationAudio() {}
void ResetQueueRecoveryTracking()
{ ++queue_resets; }
void RefreshFleetLabelControls()
{ ++fleet_label_refreshes; }
bool SetUiShipScale(float value)
{
  ++ship_rescales;
  Config::Get().graphics.ui_scale_ship = value;
  return true;
}
namespace galaxy_controls
{
bool LabelsAvailable()
{ return false; }
bool SelectionAvailable()
{ return false; }
bool HousingAvailable()
{ return false; }
void Refresh() {}
void SetOverlay(int32_t, bool) {}
} // namespace galaxy_controls
namespace mission_hud
{
bool Available() noexcept
{ return false; }
bool CanChange() noexcept
{ return false; }
void Refresh() noexcept {}
} // namespace mission_hud
namespace runtime_config
{
void           Configure(const toml::table&) {}
ReloadSnapshot CaptureReload()
{ return {}; }
bool RegisterSetting(const char* section, const char* key) noexcept
{
  registered.insert(std::string(section) + "." + key);
  return true;
}
void SaveSetting(const char* section, const char* key, config_edit::Value value, std::chrono::milliseconds) noexcept
{ saved.insert_or_assign(std::string(section) + "." + key, std::move(value)); }
mod_settings::SaveStatus GetSaveStatus(const char*, const char*, const config_edit::Value&) noexcept
{ return {conflict ? mod_settings::SaveState::Conflict : mod_settings::SaveState::Saved}; }
bool HasSaveFailures() noexcept
{ return conflict; }
} // namespace runtime_config
namespace mod_settings
{
#if defined(STFC_SETTINGS_TEXT_ENTRY_FIXTURE) || (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
bool OpenTextEntry(TextEntryRequest request)
{
  editor = std::move(request);
  return true;
}
void CloseTextEntry()
{
  auto closing = std::move(editor);
  editor.reset();
  if (closing && closing->cancel)
    closing->cancel();
}
bool TextEntryActive()
{ return editor.has_value(); }
#endif
// The native confirmation backend is independently qualified; this fixture never asks it to access the game.
BooleanSetting& ForbiddenTechConfirmationSetting()
{
  static BooleanSetting setting(
      {"community_mod.ft_confirmation",
       "Confirm Forbidden Tech upgrades",
       [] { return ReadResult{}; },
       [](bool, uint64_t) { return ApplyResult::Rejected; },
       {"ui", "auto_confirm_ft_upgrade", "Ask before upgrading Forbidden Tech.", "", ApplyTiming::Live},
       true});
  return setting;
}
} // namespace mod_settings
namespace mod_settings::native
{
PageVisitToken CurrentPageVisit()
{ return visit; }
bool IsPageVisitCurrent(PageVisitToken expected)
{ return expected != 0 && expected == visit; }
void RequestPageRefresh() {}
bool SettingsSearchAvailable()
{ return false; }
bool FocusSettingsSearch()
{ return false; }
bool QueueSettingsPage(std::string_view, std::string_view, bool)
{ return false; }
} // namespace mod_settings::native

int main()
{
  using namespace mod_settings;
  spdlog::set_level(spdlog::level::off);
  {
    std::ofstream settings(directory.Root() / config_name);
    settings << "# SYNTHETIC catalog fixture\n[graphics]\nui_scale = 3.25\n"
                "[audio]\nalert_victory = 'missing-custom.wav'\n";
  }
  auto& catalog = BuildModPages();
  Check(&BuildModPages() == &catalog && path_reads == 0 && saved.empty(),
        "catalog registration is stable and never loads, mutates or saves live configuration");
  Check(frame_queries == 0 && frame_registrations == 0 && picker_opens == 0,
        "loading image catalog registration does not acquire native or worker services");
  Check(registered.contains("control.hotkeys_extended") && registered.contains("audio.alert_fleet_miner_opc")
            && registered.contains("graphics.galaxy_label_minor_threshold"),
        "original control keys register with the existing writer");
  auto* cargo = Find<BooleanSetting>(catalog, "ui.show_player_cargo");
  Check(cargo && !cargo->Observe().state.known() && path_reads == 0,
        "unavailable owners stay unavailable without initializing Config");
  Enable("Hotkeys");
  Enable("UiScale");
  Enable("Zoom");
  Enable("Toasts");
  Enable("DailyClaimFilter");
  Enable("FleetLabels");
  auto* image = Exact("graphics.loader_image");
  Check(image && !image->Read(0).actionable(), "loading image remains unavailable without a renderer owner");
  Enable("LoginScreen");
  const auto active_image = Config::Get().graphics.loader_image;
  const auto image_path   = (directory.Root() / "loading.png").string();
  std::ofstream(image_path) << "SYNTHETIC regular-file fixture; not a decoded image";
  image->invoke(0); // No polling delivery: use the source's exact-entry fallback.
  Check(editor && picker_opens == 0, "missing polling delivery selects exact path entry");
  Check(!editor->validate("  ").size(), "empty exact path uses the embedded fallback");
  const auto await_validation = [&] {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!editor->validate(image_path).empty() && std::chrono::steady_clock::now() < end)
      std::this_thread::yield();
    Check(editor->validate(image_path).empty(), "owned asynchronous path validation completed");
  };
  await_validation();
  Check(editor->validation_changed && editor->validation_changed(), "completion notifies an unchanged editor draft");
  auto stale_image_commit = editor->commit;
  ++visit;
  Check(!stale_image_commit(image_path) && !saved.contains("graphics.loader_image"),
        "departed-page exact intent cannot save its delayed validation result");
  CloseTextEntry();
  frame_delivery   = true;
  admit_image_poll = false;
  image->invoke(0);
  Check(editor && frame_registrations == 1 && picker_opens == 0, "rejected polling admission also uses exact entry");
  await_validation();
  Check(editor->commit("  " + image_path + "  ")
            && std::get<std::string>(saved.at("graphics.loader_image")) == image_path
            && Config::Get().graphics.loader_image == active_image,
        "exact image intent trims and persists the registered key without changing active restart state");
  CloseTextEntry();
  image->reset();
  Check(std::get<std::string>(saved.at("graphics.loader_image")).empty(), "image reset persists the original default");
  admit_image_poll = true;
  image->invoke(0);
  image->invoke(0);
  Check(image_poll && picker_opens == 1 && !image->Read(0).actionable(), "picker admission is lazy and bounded");
  image_picked.set_value(image_path);
  const auto image_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  do {
    image_poll();
    std::this_thread::yield();
  } while (!image->Read(0).actionable() && std::chrono::steady_clock::now() < image_deadline);
  Check(image->Read(0).actionable() && std::get<std::string>(saved.at("graphics.loader_image")) == image_path
            && Config::Get().graphics.loader_image == active_image,
        "the production catalog polling adapter applies the source picker model's verified restart intent");
  auto before = cargo->Observe();
  Check(before.state.value == true, "live boolean reads active Config");
  Check(cargo->SetFromUser(false, before).outcome == Outcome::AppliedVerified && !Config::Get().ui.show_player_cargo
            && std::get<bool>(saved.at("ui.show_player_cargo")) == false,
        "verified live writes use the actual nested field and registered-key serializer");
  conflict = true;
  Check(cargo->status_text().find("file changed externally") != std::string::npos,
        "writer conflicts remain visible through the model");
  conflict = false;

  auto* extended = Find<BooleanSetting>(catalog, "control.hotkeys_extended");
  Check(extended && extended->SetFromUser(false, extended->Observe()).outcome == Outcome::AppliedVerified
            && Config::Get().control.hotkeys_extended && extended->restart_pending(),
        "restart intent saves the requested value without changing the active field");
  auto* preset = Find<BooleanSetting>(catalog, "ui.allow_officer_preset_reordering");
  Check(preset && !preset->Observe().state.known(), "unqualified officer preset native owner stays disabled");
  auto* aai = Find<BooleanSetting>(catalog, "ui.battle_log_summary_preview");
  Check(aai && !aai->Observe().state.known(), "AAI control stays unavailable without its compiled/runtime owner");
  Enable("BattleLogSummary");
  features.back().frame_ready = false;
  Check(!aai->Observe().state.known(), "AAI registration alone does not establish frame delivery");
  features.back().frame_ready = true;
  Check(aai->Observe().state.known() && Config::Get().ui.battle_log_summary_preview,
        "ready native AAI retains the existing enabled default");
  Check(aai->SetFromUser(false, aai->Observe()).outcome == Outcome::AppliedVerified
            && !Config::Get().ui.battle_log_summary_preview
            && !std::get<bool>(saved.at("ui.battle_log_summary_preview")),
        "AAI live toggle retains the registered TOML owner and exact key");
  Check(!Find<BooleanSetting>(catalog, "graphics.borderless_fullscreen"), "dormant borderless behavior is not revived");

  for (const bool player : {true, false}) {
    auto* detail = Find<ChoiceSetting>(catalog, player ? "graphics.zoom_label_player_detail"
                                                       : "graphics.zoom_label_non_player_detail");
    auto* threshold =
        Number<float>(player ? "graphics.zoom_label_player_threshold" : "graphics.zoom_label_non_player_threshold");
    Check(detail && threshold, "both fleet-label profiles have their production controls");
    const auto refreshes = fleet_label_refreshes;
    Check(detail->state().SetFromUser(int(FleetLabelDetail::kExpanded), detail->state().Observe()).outcome
                  == Outcome::AppliedVerified
              && fleet_label_refreshes == refreshes + 1,
          "changing fleet-label detail requests its installed gameplay owner refresh");
    Check(detail->state().SetFromUser(int(FleetLabelDetail::kExpanded), detail->state().Observe()).outcome
                  == Outcome::Unchanged
              && fleet_label_refreshes == refreshes + 1,
          "an unchanged fleet-label preference does not enqueue more owner work");
    Check(detail->state().SetFromUser(int(FleetLabelDetail::kNative), detail->state().Observe()).outcome
                  == Outcome::AppliedVerified
              && fleet_label_refreshes == refreshes + 2,
          "returning the final fleet-label override to Native still requests restoration");
    Check(threshold->SetFromUser(.75f, threshold->Observe()).outcome == Outcome::AppliedVerified
              && fleet_label_refreshes == refreshes + 3,
          "exact threshold writes share the gameplay refresh path");
    Check(threshold->SetFromUser(std::numeric_limits<float>::quiet_NaN(), threshold->Observe()).outcome
                  == Outcome::Rejected
              && fleet_label_refreshes == refreshes + 3,
          "rejected threshold writes never request native refresh work");
  }

  auto* canonical = Number<float>("graphics.ui_scale");
  auto* slider    = Find<SliderSetting>(catalog, "graphics.ui_scale");
  Check(canonical && slider && canonical->Observe().state.value == 3.25f
            && slider->state().Observe().state.reason == UnavailableReason::OutsideRange,
        "full-range canonical values survive convenience slider limits");
  Check(canonical->SetFromUser(1.175f, canonical->Observe()).outcome == Outcome::AppliedVerified
            && std::get<double>(saved.at("graphics.ui_scale")) == double(1.175f),
        "numeric writes retain exact float values rather than display rounding");
  auto* ship = Number<float>("graphics.ui_scale_ship");
  Check(ship && ship->SetFromUser(1.5f, ship->Observe()).outcome == Outcome::AppliedVerified && ship_rescales == 1,
        "ship scale calls its loaded-model owner");
  auto* backdrop = Number<float>("graphics.fr_scale");
  Check(backdrop
            && backdrop->SetFromUser(std::numeric_limits<float>::quiet_NaN(), backdrop->Observe()).outcome
                   == Outcome::Rejected
            && backdrop->SetFromUser(0, backdrop->Observe()).outcome == Outcome::Rejected,
        "restart numeric requests validate finite values and the canonical allowed range");
  auto* interval = Number<int>("control.select_timer");
  Check(interval && !Find<SliderSetting>(catalog, "control.select_timer")
            && interval->SetFromUser(321, interval->Observe()).outcome == Outcome::AppliedVerified
            && std::get<int64_t>(saved.at("control.select_timer")) == 321,
        "integer exact-entry models retain integer saves without inventing sliders");
#if defined(STFC_SETTINGS_TEXT_ENTRY_FIXTURE) || (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  auto* exact = Exact("control.select_timer");
  Check(exact && exact->Read(0).actionable(), "canonical integer has a real Edit command");
  exact->invoke(0);
  Check(editor && editor->value == "321" && !editor->validate("12.5").empty() && !editor->validate("2147483648").empty()
            && editor->validate(" 1000 ").empty(),
        "integer editing validates whole tokens, overflow and allowed range without mutation");
  Check(Config::Get().control.select_timer == 321 && editor->commit("1000")
            && Config::Get().control.select_timer == 1000,
        "accepted integer edit mutates and persists its canonical model");
  exact->invoke(0);
  Config::Get().control.select_timer = 1001;
  Check(!editor->commit("2000") && Config::Get().control.select_timer == 1001,
        "stale exact editor cannot replace changed live settings");
  exact->invoke(0);
  ++visit;
  Check(!editor->commit("2000"), "page departure revokes deferred numeric commits");
  auto* float_editor = Exact("graphics.ui_scale");
  float_editor->invoke(0);
  Check(editor && !editor->validate("nan").empty() && !editor->validate("inf").empty()
            && !editor->validate("1e-1000").empty() && editor->validate("3.25").empty(),
        "float exact editing rejects nonfinite/underflow values while accepting values beyond slider range");
  const auto exact_before = Config::Get().graphics.ui_scale;
  Check(editor->commit(editor->value) && Config::Get().graphics.ui_scale == exact_before,
        "accepting unchanged initial text preserves the exact float");
  editor.reset();
#endif

  auto* factions = Find<ChoiceSetting>(catalog, "ui.daily_bulk_claim_factions");
  Config::Get().ui.daily_bulk_claim_factions.clear();
  Check(factions && *factions->state().Observe().state.value == 7,
        "blank/native faction context displays all selected");
  for (int mask = 0; mask < 8; ++mask) {
    const auto outcome = factions->state().SetFromUser(mask, factions->state().Observe()).outcome;
    Check((outcome == Outcome::AppliedVerified || outcome == Outcome::Unchanged)
              && *factions->state().Observe().state.value == mask,
          "each source faction combination applies and reads back exactly");
    toml::table input{{"ui", toml::table{{"daily_bulk_claim_factions",
                                          std::get<std::string>(saved.at("ui.daily_bulk_claim_factions"))}}}},
        parsed;
    auto        reopened = Config::Parse(input, parsed, false);
    const auto& values   = reopened->ui.daily_bulk_claim_factions;
    for (int i = 0; i < 3; ++i) {
      constexpr std::array names{"federation", "klingon", "romulan"};
      Check(bool(mask & (1 << i)) == (values.empty() || std::ranges::contains(values, names[i])),
            "each persisted faction mask survives the actual startup parser");
    }
  }
  Check(std::get<std::string>(saved.at("ui.daily_bulk_claim_factions")).empty(),
        "all factions uses source blank encoding");
  std::vector<std::string> parsed_factions;
  Check(!ParseFactionFilter("none, federation", parsed_factions) && parsed_factions == std::vector<std::string>{"none"},
        "contradictory none is invalid to strict editors and preserves a nonempty sentinel");
  Check(factions && factions->multiple()
            && factions->state().SetFromUser(0, factions->state().Observe()).outcome == Outcome::AppliedVerified
            && Config::Get().ui.daily_bulk_claim_factions == std::vector<std::string>{"none"}
            && std::get<std::string>(saved.at("ui.daily_bulk_claim_factions")) == "none",
        "empty faction selection preserves explicit none semantics");
  auto* sound = Find<ChoiceSetting>(catalog, "audio.alert_victory");
  Check(sound && sound->state().Observe().state.value == int(NotificationSound::kCount),
        "custom audio remains represented");
  Check(sound->state().SetFromUser(int(NotificationSound::kInfo), sound->state().Observe()).outcome
                == Outcome::AppliedVerified
            && sound->state().SetFromUser(int(NotificationSound::kCount), sound->state().Observe()).outcome
                   == Outcome::AppliedVerified
            && Config::Get().audio.alert_victory.source == "missing-custom.wav",
        "choosing a builtin does not discard the retained custom source");

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  auto* routes = Exact("ui.instant_warp_auto_jump");
  Check(routes && !routes->Read(0).actionable(), "route editor requires the actual installed consumer");
  Enable("InstantWarp");
  routes->invoke(0);
  Check(editor && !editor->validate("*, Enterprise").empty()
            && editor->validate(" U.S.S. Enterprise-D, D'Vor ").empty(),
        "route editor normalizes names and rejects mixed wildcard intent");
  Check(editor->commit(" U.S.S. Enterprise-D, D'Vor ")
            && std::get<std::string>(saved.at("ui.instant_warp_auto_jump")) == "ENTERPRISE D, DVOR",
        "per-ship route edits save canonical names through the existing key");
  Check(ShipNameMatches(ShipNameWords("USS Enterprise-D"), Config::Get().ui.instant_warp_auto_jump.names[0]),
        "saved route editor words feed the existing matching implementation");
  routes->invoke(0);
  auto delayed_route = editor->commit;
  routes->reset();
  Check(!delayed_route("Relativity"), "reset invalidates an older route draft");
  routes->invoke(0);
  ++visit;
  Check(!editor->commit("*"), "departed route editor cannot publish");
  routes->invoke(0);
  Check(editor->commit("* ") && Config::Get().ui.instant_warp_auto_jump.all, "standalone wildcard remains supported");
  auto* warp = Exact("ui.instant_warp_auto_warp");
  auto* ask  = Exact("ui.instant_warp_always_ask");
  Check(warp && ask, "all three independently owned source filters are registered");
  for (auto* route : {routes, warp, ask})
    route->reset();
  auto* banners = Exact("ui.disabled_banner_types");
  Check(banners && banners->Read(0).actionable(), "existing toast owner exposes the banner editor");
  banners->invoke(0);
  Check(!editor->validate("Victory, MADE_UP_BANNER").empty() && editor->validate(" victory, Victory, defeat ").empty(),
        "banner editor rejects unknown labels and accepts source casing/duplicates");
  Check(editor->commit(" victory, Victory, defeat ")
            && std::get<std::string>(saved.at("ui.disabled_banner_types")) == "Victory, Defeat",
        "banner editor writes catalog-ordered distinct names");
  banners->invoke(0);
  Check(editor->commit("All"), "All expands existing known banner types");
  {
    toml::table input{{"ui", toml::table{{"disabled_banner_types", "All"}}}}, parsed;
    Check(Config::Parse(input, parsed, false)->ui.disabled_banner_types == Config::Get().ui.disabled_banner_types,
          "editor All and existing startup All have identical concrete consumer sets");
  }
  banners->invoke(0);
  Config::Get().ui.disabled_banner_types.clear();
  Check(!editor->commit("Defeat"), "newer external banner state invalidates a draft");
#if _WIN32
  auto* forwarding = Exact("ui.notify_banner_types");
  Check(forwarding && forwarding != banners, "desktop allowlist has its own canonical model");
  forwarding->invoke(0);
  Check(editor->commit("Victory") && Config::Get().ui.disabled_banner_types.empty(),
        "desktop forwarding preference never changes banner visibility");
  forwarding->reset();
#endif
  banners->reset();
  CloseTextEntry();
#endif
  Check(registered.contains("shortcuts.move_left") && registered.contains("shortcuts.show_lookup") && raw_queries == 0,
        "all shortcut keys register without sampling input or initializing recording");
  constexpr auto action    = Action::kMoveLeft;
  auto*          shortcuts = ShortcutEditorSetting(action);
  const auto     replace   = [](Action selected, ShortcutList list) {
    return Shortcut::Replace(selected, list, Shortcut::Read(selected).generation);
  };
  Check(shortcuts && replace(action, {"LCTRL-G", "F8"}), "shortcut model shares the live dispatcher's binding owner");
  auto first = shortcuts->Observe();
  Check(first.state.value == ShortcutList{"LCTRL-G", "F8"},
        "shortcut observation preserves alternative order and sides");
  const auto save_key = std::string("shortcuts.move_left");
#if defined(STFC_SETTINGS_TEXT_ENTRY_FIXTURE) || (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  ShortcutRow(action, "type").invoke(0);
  Check(editor && !editor->read_only && !editor->validate("F8 | F9").empty() && editor->validate("SHIFT-F14").empty(),
        "exact shortcut entry uses the strict dispatch parser");
  Check(editor->commit("SHIFT-F14") && !saved.contains(save_key)
            && Shortcut::Read(action).alternatives == ShortcutList{"LCTRL-G", "F8"},
        "staging a shortcut proposal never mutates bindings or saves");
  editor.reset();
  ShortcutRow(action, "confirm").invoke(0);
  Check(Shortcut::Read(action).alternatives == ShortcutList{"LCTRL-G", "F8", "SHIFT-F14"}
            && std::get<std::string>(saved.at(save_key)) == "LCTRL-G | F8 | SHIFT-F14",
        "explicit confirmation verifies the live replacement before registering the canonical save");

  ShortcutRow(action, "type").invoke(0);
  Check(editor && editor->commit("F15"), "a second proposal stages");
  editor.reset();
  const auto accepted = Shortcut::Read(action).alternatives;
  Check(replace(Action::kMoveRight, {"F12"}), "unrelated action advances the global replacement generation");
  ShortcutRow(action, "confirm").invoke(0);
  Check(Shortcut::Read(action).alternatives == accepted,
        "global generation changes invalidate a pending shortcut proposal");

  ShortcutRow(action, "binding").invoke(0);
  Check(editor && editor->read_only && editor->record_frame && editor->record_again,
        "Change opens the actual native recording request with an explicit reset");
  raw_held[static_cast<int>(unity::KeyCode::Mouse0)] = raw_down[static_cast<int>(unity::KeyCode::Mouse0)] = true;
  Check(!editor->record_frame(), "the opening click cannot become a recorded shortcut");
  raw_held = raw_down = {};
  Check(!editor->record_frame(), "recording waits for the opening input release");
  raw_held[static_cast<int>(unity::KeyCode::Return)] = raw_down[static_cast<int>(unity::KeyCode::Return)] = true;
  const auto recorded = editor->record_frame();
  Check(recorded == "RETURN" && editor->validate(*recorded).empty(),
        "Return records as a chord rather than applying itself");
  raw_held = raw_down                                = {};
  raw_held[static_cast<int>(unity::KeyCode::Mouse0)] = raw_down[static_cast<int>(unity::KeyCode::Mouse0)] = true;
  Check(!editor->record_frame(), "a recorded candidate is latched before the confirmation click");
  editor->record_again();
  Check(!editor->record_frame(), "Record again drains its own opening click");
  raw_held = raw_down = {};
  editor->record_frame();
  raw_held[static_cast<int>(unity::KeyCode::LeftControl)] = true;
  raw_held[static_cast<int>(unity::KeyCode::LeftShift)]   = true;
  raw_held[static_cast<int>(unity::KeyCode::I)] = raw_down[static_cast<int>(unity::KeyCode::I)] = true;
  const auto modified = editor->record_frame();
  Check(modified == "CTRL-SHIFT-I" && Shortcut::Read(action).alternatives == accepted,
        "raw recording uses canonical generic modifiers and still stages without mutation");
  auto        delayed = editor->commit;
  const auto* page    = &*std::ranges::find(catalog.RegisteredPages(), shortcuts->id(), &PageCatalog::Page::id);
  page->leave();
  Check(!editor && !delayed(*modified) && Shortcut::Read(action).alternatives == accepted,
        "page departure cancels recording and revokes its delayed commit");
  raw_held = raw_down = {};
#endif
  Check(replace(action, {"F8", "F9"}), "prepare removal fixture");
  ShortcutRow(action, "remove").invoke(0);
  Check(Shortcut::Read(action).alternatives == ShortcutList{"F9"} && ShortcutRow(action, "status").Read(0).actionable(),
        "per-binding removal applies immediately and offers Undo");
  ShortcutRow(action, "status").invoke(0);
  Check(Shortcut::Read(action).alternatives == ShortcutList{"F8", "F9"}, "Undo restores the exact observed list");
  ShortcutRow(action, "unbind").invoke(0);
  Check(Shortcut::Read(action).alternatives.empty() && std::get<std::string>(saved.at(save_key)) == "NONE",
        "unbinding the final alternative persists explicit NONE");
  ShortcutRow(action, "status").invoke(0);
  ShortcutRow(action, "default").invoke(0);
  Check(Shortcut::Read(action).alternatives == *shortcuts->default_value(),
        "Restore default uses the registered action default");
  ShortcutRow(action, "status").invoke(0);
  Check(Shortcut::Read(action).alternatives == ShortcutList{"F8", "F9"}, "default restoration has exact Undo");
  ShortcutRow(action, "remove").invoke(0);
  Check(replace(Action::kMoveRight, {"F11"}), "prepare unrelated edit during Undo");
  ShortcutRow(action, "status").invoke(0);
  Check(Shortcut::Read(action).alternatives == ShortcutList{"F9"},
        "Undo cannot overwrite an unrelated newer global replacement");
  {
    toml::table configured{{"shortcuts", toml::table{{"move_left", "LCTRL-G | F8 | LCTRL-G"}}}}, parsed;
    Shortcut::Set(Shortcut::Parse(configured, parsed, true, true));
    ShortcutRow(action, "remove").invoke(1);
    Check(Shortcut::Read(action).alternatives == ShortcutList{"LCTRL-G", "LCTRL-G"},
          "deletion retains an exact ordered subset including existing TOML duplicates");
    ShortcutRow(action, "status").invoke(0);
    Check(Shortcut::Read(action).alternatives == ShortcutList{"LCTRL-G", "F8", "LCTRL-G"},
          "Undo preserves existing duplicate alternatives and their original order");
    std::string oversized = "F8";
    for (size_t count = 1; count < ShortcutBindingDisplayLimit + 2; ++count)
      oversized += " | F8";
    configured = toml::table{{"shortcuts", toml::table{{"move_left", oversized}}}};
    parsed     = {};
    Shortcut::Set(Shortcut::Parse(configured, parsed, true, true));
    Check(ShortcutRow(action, "binding").Count() == ShortcutBindingDisplayLimit,
          "oversized source lists retain their live values while presenting a bounded prefix");
    ShortcutRow(action, "remove").invoke(0);
    Check(Shortcut::Read(action).alternatives.size() == ShortcutBindingDisplayLimit + 1,
          "oversized observed duplicate lists allow deletion");
    ShortcutRow(action, "status").invoke(0);
    Check(Shortcut::Read(action).alternatives.size() == ShortcutBindingDisplayLimit + 2,
          "exact removal Undo may recover the original oversized list without granting ordinary growth");
  }
  Config::Get().control.hotkeys_extended = false;
  Check(!ShortcutEditorSetting(Action::kShowAlliance)->Observe().state.known(),
        "extended action availability follows the active tier");
  Config::Get().control.hotkeys_extended = true;
  Check(!ShortcutEditorSetting(Action::kNavigationOrbitDrag)->Observe().state.known(),
        "parked system orbit remains unavailable");
  Config::Get().control.use_scopely_hotkeys = true;
  Check(!shortcuts->Observe().state.known(), "native shortcut mode disables mod shortcut editing");
  Config::Get().control.use_scopely_hotkeys = false;
  for (const auto& page : catalog.Build()) {
    size_t rows = page.ControlRows();
    for (auto* action : page.Controls<ActionSetting>())
      rows += action->Count() ? action->Count() - 1 : 0;
    Check(rows <= PageCatalog::NativeChildLimit,
          "catalog pages including repeated shortcut actions fit the native child budget");
  }
  std::cout << "Production configuration catalog ownership, range, restart and persistence checks passed\n";
}
