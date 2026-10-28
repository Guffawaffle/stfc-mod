// Owns reading the settings file into Config: defaults, validation, renamed and retired keys, the runtime vars
// file, and reloading when the file changes.

#include "config.h"

#include "config_filters.h"
#include "config_save.h"
#include "features/action_queue.h"
#include "features/notification_audio.h"
#include "file.h"
#include "keyboard_layout.h"
#include "runtime_config.h"
#include "shortcuts.h"
#include "text.h"

#include "game/ui.h"

#include <spdlog/spdlog.h>
#include <toml++/toml.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <sstream>
#include <utility>

namespace
{
// --- Catalogs ---

constexpr std::pair<std::string_view, bool Config::Patches::*> kPatches[] = {
    {"abilitybuttonrecoveryhooks", &Config::Patches::abilitybuttonrecoveryhooks},
    {"actionqueuerecoveryhooks", &Config::Patches::actionqueuerecoveryhooks},
    {"artifactexchangehooks", &Config::Patches::artifactexchangehooks},
    {"audioeventhooks", &Config::Patches::audioeventhooks},
    {"basecardhooks", &Config::Patches::basecardhooks},
    {"battlelogsummaryhooks", &Config::Patches::battlelogsummaryhooks},
    {"bufffixhooks", &Config::Patches::bufffixhooks},
    {"cargoformathooks", &Config::Patches::cargoformathooks},
    {"chatpatches", &Config::Patches::chatpatches},
    {"chattitledisplayhooks", &Config::Patches::chattitledisplayhooks},
    {"dailyfactionbulkclaimhooks", &Config::Patches::dailyfactionbulkclaimhooks},
    {"doubleclickassignshiphooks", &Config::Patches::doubleclickassignshiphooks},
    {"forbiddentechconfirmhooks", &Config::Patches::forbiddentechconfirmhooks},
    {"forbiddentechshipnamehooks", &Config::Patches::forbiddentechshipnamehooks},
    {"freeresizehooks", &Config::Patches::freeresizehooks},
    {"galacticanomalytimerhooks", &Config::Patches::galacticanomalytimerhooks},
    {"game_version", &Config::Patches::game_version},
    {"giftsbulkclaimhooks", &Config::Patches::giftsbulkclaimhooks},
    {"havenhistoryhooks", &Config::Patches::havenhistoryhooks},
    {"havenzoomhooks", &Config::Patches::havenzoomhooks},
    {"hotkeyhooks", &Config::Patches::hotkeyhooks},
    {"instantcargocounterhooks", &Config::Patches::instantcargocounterhooks},
    {"interiorzoomhooks", &Config::Patches::interiorzoomhooks},
    {"loadingscreenhooks", &Config::Patches::loadingscreenhooks},
    {"loadingtiphooks", &Config::Patches::loadingtiphooks},
    {"miscpatches", &Config::Patches::miscpatches},
    {"missionhudtweakshooks", &Config::Patches::missionhudtweakshooks},
    {"navigationorbithooks", &Config::Patches::navigationorbithooks},
    {"objecttracker", &Config::Patches::objecttracker},
    {"officerpresetreorderhooks", &Config::Patches::officerpresetreorderhooks},
    {"officersorthooks", &Config::Patches::officersorthooks},
    {"opcindicatorhooks", &Config::Patches::opcindicatorhooks},
    {"panhooks", &Config::Patches::panhooks},
    {"pinnedshipclicktoggle", &Config::Patches::pinnedshipclicktoggle},
    {"pinnedshiphooks", &Config::Patches::pinnedshiphooks},
    {"queueaddressguardhooks", &Config::Patches::queueaddressguardhooks},
    {"revealconfirmhooks", &Config::Patches::revealconfirmhooks},
    {"runtimeconfighooks", &Config::Patches::runtimeconfighooks},
    {"shiptechindicatorhooks", &Config::Patches::shiptechindicatorhooks},
    {"syncpatches", &Config::Patches::syncpatches},
    {"tempcrashfixes", &Config::Patches::tempcrashfixes},
    {"testpatches", &Config::Patches::testpatches},
    {"thinqueueprotectionhooks", &Config::Patches::thinqueueprotectionhooks},
    {"toastbannerhooks", &Config::Patches::toastbannerhooks},
    {"fleetnotificationhooks", &Config::Patches::fleetnotificationhooks},
    {"nativesettingshooks", &Config::Patches::nativesettingshooks},
    {"transitionscreenhooks", &Config::Patches::transitionscreenhooks},
    {"uiscalehooks", &Config::Patches::uiscalehooks},
    {"zoomhooks", &Config::Patches::zoomhooks},
};

template <typename E> struct Choice {
  std::string_view name;
  E                value;
};

// Names of ToastState values as written in disabled_banner_types and notify_banner_types.
constexpr Choice<ToastState> kBannerTypes[] = {
    {"Standard", ToastState::Standard},
    {"FactionWarning", ToastState::FactionWarning},
    {"FactionLevelUp", ToastState::FactionLevelUp},
    {"FactionLevelDown", ToastState::FactionLevelDown},
    {"FactionDiscovered", ToastState::FactionDiscovered},
    {"IncomingAttackFaction", ToastState::IncomingAttackFaction},
    {"FleetBattle", ToastState::FleetBattle},
    {"Victory", ToastState::Victory},
    {"Defeat", ToastState::Defeat},
    {"Event", ToastState::Tournament},
    {"ArmadaCreated", ToastState::ArmadaCreated},
    {"ArmadaCanceled", ToastState::ArmadaCanceled},
    {"ArmadaIncomingAttack", ToastState::ArmadaIncomingAttack},
    {"ArmadaBattleWon", ToastState::ArmadaBattleWon},
    {"ArmadaBattleLost", ToastState::ArmadaBattleLost},
    {"DiplomacyUpdated", ToastState::DiplomacyUpdated},
    {"JoinedTakeover", ToastState::JoinedTakeover},
    {"CompetitorJoinedTakeover", ToastState::CompetitorJoinedTakeover},
    {"AbandonedTerritory", ToastState::AbandonedTerritory},
    {"TakeoverVictory", ToastState::TakeoverVictory},
    {"TakeoverDefeat", ToastState::TakeoverDefeat},
    {"TreasuryProgress", ToastState::TreasuryProgress},
    {"TreasuryFull", ToastState::TreasuryFull},
    {"Achievement", ToastState::Achievement},
    {"AssaultVictory", ToastState::AssaultVictory},
    {"AssaultDefeat", ToastState::AssaultDefeat},
    {"ChallengeComplete", ToastState::ChallengeComplete},
    {"ChallengeFailed", ToastState::ChallengeFailed},
    {"StrikeHit", ToastState::StrikeHit},
    {"StrikeDefeat", ToastState::StrikeDefeat},
    {"WarchestProgress", ToastState::WarchestProgress},
    {"WarchestFull", ToastState::WarchestFull},
    {"PartialVictory", ToastState::PartialVictory},
    {"ArenaTimeLeft", ToastState::ArenaTimeLeft},
    {"ChainedEventScored", ToastState::ChainedEventScored},
    {"FleetPresetApplied", ToastState::FleetPresetApplied},
    {"SurgeWarmUpEnded", ToastState::SurgeWarmUpEnded},
    {"SurgeHostileGroupDefeated", ToastState::SurgeHostileGroupDefeated},
    {"SurgeTimeLeft", ToastState::SurgeTimeLeft},
};

// For the enum settings, the first name listed for a value is the one written back; later ones are accepted
// synonyms. The empty name keeps a blank setting at the default without a warning.
constexpr Choice<MissionHudVisibility> kHudVisibility[] = {
    {"auto", MissionHudVisibility::kAuto},
    {"always", MissionHudVisibility::kAlways},
    {"never", MissionHudVisibility::kNever},
    {"", MissionHudVisibility::kAuto},
};

constexpr Choice<InstantWarpConfirmation> kInstantWarp[] = {
    {"none", InstantWarpConfirmation::kNone},  {"warp", InstantWarpConfirmation::kWarp},
    {"jump", InstantWarpConfirmation::kJump},  {"left", InstantWarpConfirmation::kWarp},
    {"right", InstantWarpConfirmation::kJump}, {"", InstantWarpConfirmation::kNone},
};

constexpr Choice<FleetLabelDetail> kFleetLabelDetail[] = {
    {"native", FleetLabelDetail::kNative},    {"expanded", FleetLabelDetail::kExpanded},
    {"compact", FleetLabelDetail::kCompact},  {"threshold", FleetLabelDetail::kThreshold},
    {"always", FleetLabelDetail::kExpanded},  {"never", FleetLabelDetail::kCompact},
    {"custom", FleetLabelDetail::kThreshold}, {"auto", FleetLabelDetail::kNative},
    {"none", FleetLabelDetail::kNative},      {"off", FleetLabelDetail::kNative},
    {"", FleetLabelDetail::kNative},
};

constexpr Choice<AudioCoalescing> kAudioCoalescing[] = {
    {"none", AudioCoalescing::None},
    {"same", AudioCoalescing::Same},
    {"all", AudioCoalescing::All},
};
constexpr std::pair<std::string_view, NotificationAudioCue Config::Audio::*> kAudioAlerts[] = {
    {"alert_victory", &Config::Audio::alert_victory},
    {"alert_defeat", &Config::Audio::alert_defeat},
    {"alert_incoming_player_attack", &Config::Audio::alert_incoming_player_attack},
    {"alert_armada_created", &Config::Audio::alert_armada_created},
    {"alert_armada_battle_won", &Config::Audio::alert_armada_battle_won},
    {"alert_armada_battle_lost", &Config::Audio::alert_armada_battle_lost},
    {"alert_fleet_arrived_in_system", &Config::Audio::alert_fleet_arrived_in_system},
    {"alert_fleet_arrived_at_destination", &Config::Audio::alert_fleet_arrived_at_destination},
    {"alert_fleet_started_mining", &Config::Audio::alert_fleet_started_mining},
    {"alert_fleet_node_depleted", &Config::Audio::alert_fleet_node_depleted},
    {"alert_fleet_docked", &Config::Audio::alert_fleet_docked},
    {"alert_fleet_repair_complete", &Config::Audio::alert_fleet_repair_complete},
    {"alert_fleet_miner_opc", &Config::Audio::alert_fleet_miner_opc},
};

constexpr std::string_view kKnownFactions[] = {"federation", "klingon", "romulan"};

// --- Reading ---

// The text of a setting as written, for messages about values that were rejected.
std::string Raw(toml::node_view<const toml::node> node)
{
  if (auto text = node.value<std::string>())
    return *text;
  std::ostringstream out;
  out << node;
  return out.str();
}

// Tokens are shown with their first 9 and last 12 characters, enough to tell two apart in a log.
std::string MaskToken(std::string token)
{
  if (token.size() > 21)
    std::ranges::replace_if(token.begin() + 9, token.end() - 12, [](char c) { return c != '-'; }, '*');
  return token;
}

// Reads settings from the user's file and records every value used in `effective`, which is written out as the
// runtime vars file so users can see what the mod actually used. A missing or rejected value falls back to the
// default, which is the member's current value; every rejection logs one warning.
class Reader
{
public:
  Reader(const toml::table& file, toml::table& effective, bool log)
      : file_(file)
      , effective_(effective)
      , log_(log)
  {
  }

  // The value as written, empty when the file does not set it.
  toml::node_view<const toml::node> Node(std::string_view section, std::string_view key) const
  { return file_[section][key]; }

  // The section of the vars file, created on first use.
  toml::table& EffectiveSection(std::string_view section)
  { return *effective_.emplace<toml::table>(section).first->second.as_table(); }

  // The configured value converted to T, or `fallback` when missing or of the wrong type. TOML integers read fine
  // into float settings; fractions into int settings do not.
  template <typename T> T Get(std::string_view section, std::string_view key, T fallback)
  {
    auto value = Convert(section, key, fallback);
    Record(section, key, value);
    return value;
  }

  template <typename T> void Read(std::string_view section, std::string_view key, T& value)
  { value = Get(section, key, value); }

  // Read, keeping the default when `valid` rejects the configured value.
  template <typename T, typename Valid>
  void ReadValid(std::string_view section, std::string_view key, T& value, Valid valid)
  {
    const auto fallback = value;
    Read(section, key, value);
    if (valid(value))
      return;
    Invalid(section, key, value, fallback);
    value = fallback;
    Record(section, key, value);
  }

  // A renamed setting: `key`, or `old_key` when only the old name is set. The vars file records `key`.
  template <typename T>
  void ReadRenamed(std::string_view section, std::string_view key, std::string_view old_key, T& value)
  {
    if (Node(section, key) || !Node(section, old_key))
      return Read(section, key, value);
    spdlog::warn("deprecated config value {}.{}; use {}.{}", section, old_key, section, key);
    value = Convert(section, old_key, value);
    Record(section, key, value);
  }

  // A case-insensitive choice from `choices`. The vars file records the canonical name.
  template <typename E, size_t N>
  void Choose(std::string_view section, std::string_view key, E& value, const Choice<E> (&choices)[N])
  {
    const auto fallback = CanonicalName(value, choices);
    const auto raw      = Get<std::string>(section, key, std::string(fallback));
    const auto found    = std::ranges::find(choices, AsciiStrToLower(StripAsciiWhitespace(raw)), &Choice<E>::name);
    if (found == std::end(choices))
      Invalid(section, key, raw, fallback);
    else
      value = found->value;
    Record(section, key, std::string(CanonicalName(value, choices)));
  }

  // A setting that no longer exists, at a dotted path. Says what replaced it when the file still sets it.
  void Retired(std::string_view path, std::string_view replacement)
  {
    if (file_.at_path(path))
      spdlog::warn("retired config value {}; {}", path, replacement);
  }

  // Sets the value the vars file shows. A later Record of the same key replaces it.
  template <typename T> void Record(std::string_view section, std::string_view key, const T& value)
  {
    EffectiveSection(section).insert_or_assign(key, value);
    if (log_)
      spdlog::debug("config {}.{} = {}", section, key, value);
  }

private:
  // The value converted to T, or `fallback` when missing or, after a warning, of the wrong type.
  template <typename T> T Convert(std::string_view section, std::string_view key, const T& fallback) const
  {
    const auto node = Node(section, key);
    if (auto read = node.template value<T>())
      return *read;
    if (node)
      Invalid(section, key, Raw(node), fallback);
    return fallback;
  }

  template <typename R, typename F>
  static void Invalid(std::string_view section, std::string_view key, const R& raw, const F& fallback)
  { spdlog::warn("invalid config value {}.{}: '{}'; using {}", section, key, raw, fallback); }

  template <typename E, size_t N> static std::string_view CanonicalName(E value, const Choice<E> (&choices)[N])
  { return std::ranges::find(choices, value, &Choice<E>::value)->name; }

  const toml::table& file_;
  toml::table&       effective_;
  bool               log_;
};

// "*" matches every ship; otherwise a comma list of ship names, each split into words by ShipNameWords.
ShipFilter ReadShipFilter(Reader& reader, std::string_view key)
{
  const auto value = reader.Get<std::string>("ui", key, "");
  ShipFilter filter;
  // Startup remains tolerant of malformed manual lists. The native editor
  // rejects the same parser's false result instead of silently accepting it.
  ParseShipFilter(value, filter);
  return filter;
}

// A comma list of kKnownFactions, or "none", which wins over the others. Blank means all of them.
std::vector<std::string> ReadFactions(Reader& reader)
{
  const auto all   = kKnownFactions | std::views::join_with(',') | std::ranges::to<std::string>();
  auto       value = reader.Get<std::string>("ui", "daily_bulk_claim_factions", all);
  if (StripAsciiWhitespace(value).empty()) {
    value = all;
    // Replaces the blank that Get recorded, without a second debug line.
    reader.EffectiveSection("ui").insert_or_assign("daily_bulk_claim_factions", value);
  }

  std::vector<std::string> factions;
  bool                     none = false;
  for (const auto& token : StrSplit(value, ',')) {
    auto faction = AsciiStrToLower(token);
    if (faction == "none")
      none = true;
    else if (!std::ranges::contains(kKnownFactions, faction))
      spdlog::warn("invalid config value ui.daily_bulk_claim_factions: '{}'; ignoring it (expected {} or none)", token,
                   all);
    else if (!std::ranges::contains(factions, faction))
      factions.push_back(std::move(faction));
  }
  if (!none)
    return factions;
  if (!factions.empty())
    spdlog::warn("invalid config value ui.daily_bulk_claim_factions: '{}'; using none, which overrides the others",
                 value);
  return {"none"};
}

// A case-insensitive comma list of kBannerTypes names, or "All". The vars file gets the names that matched, in
// catalog order.
std::vector<int> ReadBannerTypes(Reader& reader, std::string_view key)
{
  auto wanted = StrSplit(reader.Get<std::string>("ui", key, ""), ',') | std::views::transform(AsciiStrToUpper)
                | std::ranges::to<std::vector>();
  // With "All", names after it are still reported as unknown.
  const bool all = std::erase(wanted, "ALL") > 0;

  std::vector<int> types;
  std::string      names;
  for (const auto& [name, state] : kBannerTypes) {
    if (!all && std::erase(wanted, AsciiStrToUpper(name)) == 0)
      continue;
    types.push_back(std::to_underlying(state));
    names += names.empty() ? "" : ", ";
    names += name;
  }
  for (const auto& unknown : wanted)
    spdlog::warn("invalid config value ui.{}: '{}'; ignoring it", key, unknown);

  reader.Record("ui", key, names);
  return types;
}

// [sync.targets.<name>] tables. Each target takes its proxy, verify_ssl and categories from [sync] unless it sets
// its own. Unlike [sync], a target's renamed category keys are read without a deprecation warning.
void ReadSyncTargets(Reader& reader, std::map<std::string, SyncTargetConfig>& sync_targets, const SyncConfig& defaults)
{
  // The built-in targets are listed in the vars file as examples; they only sync once given a token.
  auto& effective_targets =
      *reader.EffectiveSection("sync")
           .insert_or_assign(
               "targets",
               toml::table{
                   {"stfcdata", toml::table{{"token", ""}, {"url", "https://sync.stfcdata.de"}}},
                   {"nextspocksclub", toml::table{{"token", ""}, {"url", "https://next.spocks.club/sync/ingress/"}}},
                   {"spocksclub", toml::table{{"token", ""}, {"url", "https://spocks.club/sync/ingress/"}}},
               })
           .first->second.as_table();

  const auto* targets = reader.Node("sync", "targets").as_table();
  if (targets)
    for (const auto& [name, node] : *targets) {
      const auto* values = node.as_table();
      if (!values)
        continue;

      const auto section = "sync.targets." + std::string(name.str());
      const auto url     = (*values)["url"].value<std::string>();
      const auto token   = (*values)["token"].value<std::string>();
      if (!url || !token) {
        spdlog::warn("invalid config value {}: url and token are required; skipping the target", section);
        continue;
      }
      // Blank values are the example file's placeholders for a target the user has not set up.
      if (url->empty() || token->empty())
        continue;

      SyncTargetConfig target{defaults, *url, *token};
      target.proxy      = (*values)["proxy"].value<std::string>().value_or(target.proxy);
      target.verify_ssl = (*values)["verify_ssl"].value<bool>().value_or(target.verify_ssl);

      toml::table effective{
          {"url", target.url}, {"token", target.token}, {"proxy", target.proxy}, {"verify_ssl", target.verify_ssl}};
      for (const auto& [key, member, old_name] : SyncOptions) {
        const auto read_as = values->contains(key) || old_name.empty() ? key : old_name;
        target.*member     = (*values)[read_as].value<bool>().value_or(target.*member);
        effective.insert(key, target.*member);
      }

      // TOML table keys are unique, so every target is new here.
      effective_targets.insert_or_assign(name.str(), std::move(effective));
      spdlog::debug("config {} = url {}, token {}, proxy '{}', verify_ssl {}", section, target.url,
                    MaskToken(target.token), target.proxy, target.verify_ssl);
      sync_targets.emplace(name.str(), std::move(target));
    }

  // Preserve downstream's legacy pair without rewriting the user's primary
  // TOML. A valid explicit default target wins; examples/invalid targets do not.
  const auto legacy_url   = reader.Node("sync", "url").value<std::string>();
  const auto legacy_token = reader.Node("sync", "token").value<std::string>();
  if (legacy_url && legacy_token && !legacy_url->empty() && !legacy_token->empty()) {
    SyncTargetConfig target{defaults, *legacy_url, *legacy_token};
    if (sync_targets.emplace("default", target).second) {
      toml::table effective{
          {"url", target.url}, {"token", target.token}, {"proxy", target.proxy}, {"verify_ssl", target.verify_ssl}};
      for (const auto& [key, member, old_name] : SyncOptions)
        effective.insert(key, target.*member);
      effective_targets.insert_or_assign("default", std::move(effective));
      spdlog::info("[Sync] legacy sync.url/token interpreted as sync.targets.default");
    } else {
      spdlog::warn("[Sync] explicit sync.targets.default takes precedence over legacy sync.url/token");
    }
  }
}

// --- Files and activation ---

std::unique_ptr<Config> current_config;
// The vars document follows accepted settings, never a rejected parse or the primary editor's write queue.
std::optional<toml::table> accepted_effective;
std::optional<uint64_t>    saved_keyboard_generation;
bool                       reported_keyboard_save_failure = false;
// Detached sync workers can still read during static teardown, so the publication slot lives until process exit.
auto& sync_config = *new std::shared_ptr<const Config::Sync>{std::make_shared<const Config::Sync>()};

// The settings file, or nullopt after a warning that says why it could not be read and what happens `instead`.
std::optional<toml::table> ReadSettingsFile(std::string_view instead)
{
  try {
    // The toml++ library is built without its char8_t overloads, so the UTF-8 path goes in as chars.
    const auto path = File::Path(File::Config()).u8string();
    return toml::parse_file(std::string_view(reinterpret_cast<const char*>(path.data()), path.size()));
  } catch (const toml::parse_error& e) {
    spdlog::warn("could not parse {}: {}; {}", File::Config(), e.description(), instead);
  } catch (const std::exception& e) {
    spdlog::warn("could not read {}: {}; {}", File::Config(), e.what(), instead);
  }
  return std::nullopt;
}

// Heads the runtime vars file; the placeholders are File::DefaultConfig() and File::Config().
constexpr std::string_view kVarsHeader = R"(#######################################################################
#######################################################################
####                                                               ####
#### NOTE: This file is not the configuration file that is used    ####
####       by the STFC Community Mod.  It is provided to help      ####
####       see what configuration is being used by the runtime     ####
####       and any desired settings should be copied to the same   ####
####       section in: {}
####                                                               ####
####        Config in: {}
####                                                               ####
#######################################################################
#######################################################################

)";

void ReportStartupSaveFailure(std::string_view name, std::string_view error) noexcept
{
  try {
    spdlog::warn("could not write startup configuration output '{}': {}; "
                 "loaded settings remain available",
                 name, error);
  } catch (...) {
    // Reporting an optional generated-file failure cannot prevent activation.
  }
}

bool Save(const toml::table& table, const std::string& name, bool warning_header, bool startup)
{
  try {
    const auto header =
        warning_header ? std::format(kVarsHeader, File::DefaultConfig(), File::Config()) : std::string{};
    const auto path = File::Path(name, true);
    if (startup) {
      std::string error;
      if (!SaveStartupConfigDocument(table, path, header, error)) {
        ReportStartupSaveFailure(name, error);
        return false;
      }
    } else {
      // Preserve Reload's existing transactional activation behavior: an output
      // failure rejects the candidate without changing live settings/bindings.
      SaveConfigDocument(table, path, header);
    }
    return true;
  } catch (const std::exception& error) {
    if (!startup)
      throw;
    ReportStartupSaveFailure(name, error.what());
  } catch (...) {
    if (!startup)
      throw;
    ReportStartupSaveFailure(name, "unknown output preparation failure");
  }
  return false;
}

bool SaveRuntimeVars(const toml::table& effective, bool startup)
{
  try {
    const auto diagnostics = keyboard_layout::ReadDiagnostics();
    auto       vars        = effective;
    vars.insert_or_assign("keyboard_mapping", toml::table{{"layout", diagnostics.layout},
                                                          {"status", diagnostics.status},
                                                          {"reason", diagnostics.reason},
                                                          {"generation", std::to_string(diagnostics.generation)}});
    if (!Save(vars, File::Vars(), true, startup))
      return false;
    saved_keyboard_generation      = diagnostics.generation;
    reported_keyboard_save_failure = false;
    return true;
  } catch (const std::exception& error) {
    if (!startup)
      throw;
    ReportStartupSaveFailure(File::Vars(), error.what());
  } catch (...) {
    if (!startup)
      throw;
    ReportStartupSaveFailure(File::Vars(), "unknown diagnostics preparation failure");
  }
  return false;
}

void ReportKeyboardSaveFailure(std::string_view error) noexcept
{
  if (std::exchange(reported_keyboard_save_failure, true))
    return;
  try {
    spdlog::warn("could not update keyboard diagnostics in {}: {}; the accepted settings remain active; will retry",
                 File::Vars(), error);
  } catch (...) {
  }
}

// Native input only publishes scalar diagnostics. The existing once-per-second settings check owns their I/O.
void RefreshKeyboardDiagnostics() noexcept
{
  if (!accepted_effective
      || (saved_keyboard_generation && !keyboard_layout::DiagnosticsChanged(*saved_keyboard_generation)))
    return;
  try {
    SaveRuntimeVars(*accepted_effective, false);
  } catch (const std::exception& error) {
    ReportKeyboardSaveFailure(error.what());
  } catch (...) {
    ReportKeyboardSaveFailure("unknown output failure");
  }
}

// Writes `effective` as the runtime vars file. At startup it also becomes the
// settings file when there is none, so a new user starts from a complete list
// of settings.
bool SaveEffective(const toml::table& effective, bool startup)
{
  bool created_primary = false;
  if (startup) {
    try {
      if (!std::filesystem::exists(File::Path(File::Config()))) {
        spdlog::info("creating {} with the default settings", File::Config());
        created_primary = Save(effective, File::Config(), false, true);
      }
    } catch (const std::exception& error) {
      // An existence/path failure is not permission to replace the primary
      // file.
      ReportStartupSaveFailure(File::Config(), error.what());
    } catch (...) {
      ReportStartupSaveFailure(File::Config(), "could not establish whether primary settings exist");
    }
  }
  spdlog::info("writing the settings in effect to {}", File::Vars());
  SaveRuntimeVars(effective, startup);
  return created_primary;
}
} // namespace

// PLAY config_filters.cc, adapted to the existing typed ShipFilter and shared
// banner catalog. No native objects or filesystem operations are involved.
bool ParseShipFilter(std::string_view text, ShipFilter& filter)
{
  filter = {};
  if (StripAsciiWhitespace(text) == "*") {
    filter.all = true;
    return true;
  }
  bool valid = true;
  for (const auto& token : StrSplit(text, ',')) {
    const auto normalized = NormalizeShipName(token);
    if (normalized.empty())
      valid = false;
    else {
      if (token == "*")
        valid = false;
      // Preserve tolerant startup's literal mixed wildcard pattern; an editor
      // rejects the entire false result. Normalize only once for matching.
      filter.names.push_back(ShipNameWords(token));
    }
  }
  return valid;
}

std::string FormatShipFilter(const ShipFilter& filter)
{
  if (filter.all)
    return "*";
  std::string text;
  for (const auto& words : filter.names) {
    if (!text.empty())
      text += ", ";
    bool first = true;
    for (const auto& word : words) {
      if (!std::exchange(first, false))
        text += ' ';
      text += word;
    }
  }
  return text;
}

bool ParseFactionFilter(std::string_view text, std::vector<std::string>& factions)
{
  factions.clear();
  bool valid = true, none = false;
  for (const auto& token : StrSplit(text, ',')) {
    auto name = AsciiStrToLower(token);
    if (name == "none")
      none = true;
    else if (!std::ranges::contains(kKnownFactions, name))
      valid = false;
    else if (!std::ranges::contains(factions, name))
      factions.push_back(std::move(name));
  }
  if (none) {
    valid    = valid && factions.empty();
    factions = {"none"}; // Empty means all/native; none must remain nonempty.
  }
  return valid;
}

bool ParseBannerFilter(std::string_view text, std::vector<int>& types)
{
  types.clear();
  bool valid = true;
  for (const auto& token : StrSplit(text, ',')) {
    const auto name  = AsciiStrToUpper(token);
    bool       found = false;
    for (const auto& entry : kBannerTypes)
      if (name == "ALL" || name == AsciiStrToUpper(entry.name)) {
        if (!std::ranges::contains(types, std::to_underlying(entry.value)))
          types.push_back(std::to_underlying(entry.value));
        found = true;
      }
    valid = valid && found;
  }
  return valid;
}

std::string FormatBannerFilter(const std::vector<int>& types)
{
  std::string text;
  for (const auto& entry : kBannerTypes)
    if (std::ranges::contains(types, std::to_underlying(entry.value))) {
      if (!text.empty())
        text += ", ";
      text += entry.name;
    }
  return text;
}

std::unique_ptr<Config> Config::Parse(const toml::table& file, toml::table& effective, bool log_values)
{
  std::unique_ptr<Config> config(new Config);
  config->Read(file, effective, log_values);
  return config;
}

std::shared_ptr<const Config::Sync> Config::Sync::Get()
{ return std::atomic_load_explicit(&sync_config, std::memory_order_acquire); }

void Config::Activate(std::unique_ptr<Config> next)
{
  // Prepare only an accepted candidate. Parsing, effective-vars inspection and settings inventories perform no I/O.
  // Relative custom clips follow the admitted settings location, never an independently inferred account path.
  const auto directory = File::Path(File::Config()).parent_path();
  for (const auto& [key, member] : kAudioAlerts) {
    auto& cue = next->audio.*member;
    if (!NotificationSoundFromName(cue.source))
      cue = LoadNotificationAudio(cue.source, directory);
  }
  SetNotificationAudioCoalescing(next->audio.coalescing);
  keyboard_layout::Configure(next->control.keyboard_layout_mode);
  Shortcut::Set(std::move(next->shortcuts_));
  std::atomic_store_explicit(&sync_config, next->sync_, std::memory_order_release);
  // A live file reload can turn the protections off/on without a queue callback in between. Retire the old
  // correlation at acceptance, just as the Settings and shortcut paths do; parsing/rejected reloads do not act.
  if (current_config
      && (current_config->control.queue_enabled != next->control.queue_enabled
          || current_config->control.kirshara_fixes != next->control.kirshara_fixes))
    ResetQueueRecoveryTracking();
  current_config = std::move(next);
}

Config& Config::Get()
{
  // First use is inside il2cpp_init on the game thread. A file that cannot be read gives the defaults; values are
  // only logged when they came from the file.
  if (!current_config) {
    File::MigrateLegacyFiles();
    auto        file   = ReadSettingsFile("using the default settings");
    const bool  loaded = file.has_value();
    toml::table effective;
    auto        config  = Parse(file.value_or(toml::table{}), effective, loaded);
    const bool  created = SaveEffective(effective, true);
    // Preserve original semantic disk values. A missing key is not its default;
    // only a successfully created complete primary supplies effective defaults.
    runtime_config::Configure(file ? *file : created ? effective : toml::table{});
    Activate(std::move(config));
    accepted_effective = std::move(effective);
    RefreshKeyboardDiagnostics();
  }
  return *current_config;
}

bool Config::Reload()
{
  std::unique_ptr<Config> next;
  toml::table             effective;
  try {
    // Capture before opening/parsing: acknowledgement crossing the read must
    // not let older file bytes replace a pending/session-only live intent.
    const auto snapshot = runtime_config::CaptureReload();
    auto       file     = ReadSettingsFile("keeping the current settings");
    if (!file)
      return false;
    runtime_config::ApplyReloadSnapshot(*file, snapshot);
    spdlog::info("reloading settings from {}", File::Config());
    next = Parse(*file, effective, true);
    if (next->patches != Get().patches)
      spdlog::warn("changes to [patches] take effect after a restart");
    next->patches = Get().patches;
    // Runtime vars describe installed hook groups, not a future launch's request.
    auto* active_patches = effective["patches"].as_table();
    for (const auto& [key, member] : kPatches)
      active_patches->insert_or_assign(key, next->patches.*member);
    SaveEffective(effective, false);
  } catch (const std::exception& e) {
    spdlog::error("could not apply {}: {}; keeping the current settings", File::Config(), e.what());
    return false;
  }
  Activate(std::move(next));
  accepted_effective = std::move(effective);
  RefreshKeyboardDiagnostics();
  return true;
}

void Config::ReloadIfChanged()
{
  // Editors save in steps (truncate, then write) and an empty file is valid TOML, so a change is only applied once
  // the file has kept the same modification time for a whole check.
  using Clock = std::chrono::steady_clock;
  static Clock::time_point                              next_check{};
  static std::optional<std::filesystem::file_time_type> applied, pending;

  const auto now = Clock::now();
  if (now < next_check)
    return;
  next_check = now + std::chrono::seconds(1);
  RefreshKeyboardDiagnostics();

  std::error_code error;
  const auto      time = std::filesystem::last_write_time(File::Path(File::Config()), error);
  if (error)
    return;
  // The first check only learns the time of the file Get already read.
  if (!applied || time == *applied) {
    applied = time;
    pending.reset();
    return;
  }
  if (pending != time) {
    pending = time;
    return;
  }
  applied = time;
  pending.reset();
  Reload();
}

void Config::Read(const toml::table& file, toml::table& effective, bool log_values)
{
  Reader r(file, effective, log_values);

  // [patches]. Reload keeps the startup values, since hooks are only installed once.

  for (const auto& [key, member] : kPatches)
    r.Read("patches", key, patches.*member);

  // [control] comes next: enable_experimental and hotkeys_extended decide which other settings are read.
  r.Read("control", "kirshara_fixes", control.kirshara_fixes);
  // Once kirshara_fixes is set, leftover queue keys are not worth a warning.
  if (!r.Node("control", "kirshara_fixes"))
    for (const auto path : {"control.faster_queue_recovery", "control.thin_queue_protection",
                            "control.queue_address_guard", "advanced.queue.thin_queue_protection"})
      r.Retired(path, "control.kirshara_fixes switches all the queue fixes");
  r.Read("control", "primary_requires_visible_target", control.primary_requires_visible_target);
  r.Read("control", "queue_enabled", control.queue_enabled);
  r.Read("control", "hotkeys_enabled", control.hotkeys_enabled);
  r.Read("control", "hotkeys_extended", control.hotkeys_extended);
  r.Read("control", "use_scopely_hotkeys", control.use_scopely_hotkeys);
  r.ReadValid("control", "keyboard_layout_mode", control.keyboard_layout_mode,
              [](const std::string& mode) { return mode == "physical" || mode == "layout"; });
  r.Read("control", "select_timer", control.select_timer);
  r.Read("control", "enable_experimental", control.enable_experimental);

  // [graphics]
  auto& g = graphics;
  r.Read("graphics", "ui_scale", g.ui_scale);
  r.ReadRenamed("graphics", "ui_scale_step", "ui_scale_adjust", g.ui_scale_step);
  r.Read("graphics", "ui_scale_ship", g.ui_scale_ship);
  r.Read("graphics", "ui_scale_viewer", g.ui_scale_viewer);
  r.Read("graphics", "ship_hotkey_badges", g.ship_hotkey_badges);
  r.Read("graphics", "galaxy_extended_selection", g.galaxy_extended_selection);
  r.Read("graphics", "galaxy_multi_select", g.galaxy_multi_select);
  r.Read("graphics", "galaxy_station_housing", g.galaxy_station_housing);
  constexpr std::array galaxy_layers{"galaxy_overlay_default", "galaxy_overlay_mining", "galaxy_overlay_hostiles",
                                     "galaxy_overlay_hazards"};
  for (size_t i = 0; i < galaxy_layers.size(); ++i)
    r.Read("graphics", galaxy_layers[i], g.galaxy_overlays[i]);
  if (std::ranges::none_of(g.galaxy_overlays, [](bool value) { return value; })) {
    g.galaxy_overlays[0] = true;
    r.Record("graphics", "galaxy_overlay_default", true);
  }
  const Choice<galaxy_controls::ZoomMode> galaxy_modes[]{{"native", galaxy_controls::ZoomMode::Native},
                                                         {"always", galaxy_controls::ZoomMode::Always},
                                                         {"threshold", galaxy_controls::ZoomMode::Threshold}};
  const auto galaxy_profile = [&r, &galaxy_modes](std::string prefix, galaxy_controls::ZoomProfile& profile) {
    r.Choose("graphics", prefix + "_detail", profile.mode, galaxy_modes);
    r.ReadValid("graphics", prefix + "_threshold", profile.threshold,
                [](float value) { return std::isfinite(value) && value >= 0.f && value <= 1.f; });
  };
  galaxy_profile("galaxy_label_major", g.galaxy_label_major);
  galaxy_profile("galaxy_label_minor", g.galaxy_label_minor);
  r.Read("graphics", "zoom", g.zoom);
  r.ReadValid("graphics", "haven_zoom", g.haven_zoom, [](float v) { return std::isfinite(v) && v >= 0.0f; });
  r.Read("graphics", "hide_haven_water", g.hide_haven_water);
  r.ReadValid("graphics", "interior_zoom_scale", g.interior_zoom_scale,
              [](float v) { return std::isfinite(v) && v >= 1.0f; });
  r.Read("graphics", "fr_scale", g.fr_scale);
  const auto read_label = [&r](const std::string& prefix, FleetLabelProfile& profile) {
    r.Choose("graphics", prefix + "_detail", profile.detail, kFleetLabelDetail);
    r.ReadValid("graphics", prefix + "_threshold", profile.zoom_threshold,
                [](float v) { return std::isfinite(v) && v >= 0.0f && v <= 1.0f; });
  };
  read_label("zoom_label_player", g.zoom_label_player);
  read_label("zoom_label_non_player", g.zoom_label_non_player);
  r.Read("graphics", "free_resize", g.free_resize);
  r.Read("graphics", "allow_cursor", g.allow_cursor);
  r.Read("graphics", "keyboard_zoom_speed", g.keyboard_zoom_speed);
  if (control.enable_experimental)
    r.Read("graphics", "system_pan_momentum", g.system_pan_momentum);
  r.Read("graphics", "system_pan_momentum_falloff", g.system_pan_momentum_falloff);
  r.Read("graphics", "disable_target_camera_lock", g.disable_target_camera_lock);
  r.Read("graphics", "borderless_fullscreen", g.borderless_fullscreen);
  r.Read("graphics", "transition_time", g.transition_time);
  r.Read("graphics", "default_system_zoom", g.default_system_zoom);
  r.Read("graphics", "system_zoom_preset_1", g.system_zoom_preset_1);
  r.Read("graphics", "system_zoom_preset_2", g.system_zoom_preset_2);
  r.Read("graphics", "system_zoom_preset_3", g.system_zoom_preset_3);
  r.Read("graphics", "system_zoom_preset_4", g.system_zoom_preset_4);
  r.Read("graphics", "system_zoom_preset_5", g.system_zoom_preset_5);
  r.Read("graphics", "use_presets_as_default", g.use_presets_as_default);
  r.Read("graphics", "loader_enabled", g.loader_enabled);
  r.Read("graphics", "loader_transition", g.loader_transition);
  r.Read("graphics", "loader_transition_black", g.loader_transition_black);
  // Without transition customization (or in a build that keeps the original background) the game's own black
  // background stays. The vars file records what is actually used.
#ifdef _USE_ORIGINAL_BG
  constexpr bool kOriginalBackground = true;
#else
  constexpr bool kOriginalBackground = false;
#endif
  if (kOriginalBackground || !g.loader_transition) {
    g.loader_transition_black = true;
    r.Record("graphics", "loader_transition_black", true);
  }
  r.Read("graphics", "loader_image", g.loader_image);
  r.Read("graphics", "loader_logo_scale", g.loader_logo_scale);
  r.Read("graphics", "loader_tip_enabled", g.loader_tip_enabled);
  r.Read("graphics", "galactic_anomaly_timer", g.galactic_anomaly_timer);

  // [buffs]
  r.Read("buffs", "use_out_of_dock_power", buffs.use_out_of_dock_power);

  // [audio]
  r.Read("audio", "trace_events", audio.trace_events);
  for (auto& event : StrSplit(r.Get<std::string>("audio", "disabled_events", ""), ',')) {
    if (AsciiStrToLower(event) == "all")
      audio.disable_all_events = true;
    else
      audio.disabled_events.push_back(std::move(event));
  }
  r.Choose("audio", "coalescing", audio.coalescing, kAudioCoalescing);
  for (const auto& [key, member] : kAudioAlerts) {
    auto& cue = audio.*member;
    cue       = NotificationAudioCueFromSource(r.Get<std::string>("audio", key, cue.source));
    r.Record("audio", key, cue.source);
  }
  const bool has_audio_alert = audio.alert_victory.source != "none" || audio.alert_defeat.source != "none"
                               || audio.alert_armada_created.source != "none"
                               || audio.alert_armada_battle_won.source != "none"
                               || audio.alert_armada_battle_lost.source != "none";
  // The alerts play from the toast hooks.
  if (has_audio_alert && !patches.toastbannerhooks)
    spdlog::warn("audio alerts need patches.toastbannerhooks = true; they stay silent");

  // [ui]
  auto& u = ui;
  r.Read("ui", "disable_escape_exit", u.disable_escape_exit);
  r.Read("ui", "disable_escape_exit_timer", u.disable_escape_exit_timer);
  r.Read("ui", "disable_preview_locate", u.disable_preview_locate);
  r.Read("ui", "disable_preview_recall", u.disable_preview_recall);
  r.Read("ui", "keep_base_card_open", u.keep_base_card_open);
  r.Read("ui", "only_show_first_popup", u.only_show_first_popup);
  r.Read("ui", "reverse_haven_history", u.reverse_haven_history);
  r.Read("ui", "disable_move_keys", u.disable_move_keys);
  r.Read("ui", "disable_toast_banners", u.disable_toast_banners);
  r.Read("ui", "disable_galaxy_chat", u.disable_galaxy_chat);
  r.Read("ui", "disable_veil_chat", u.disable_veil_chat);
  r.Read("ui", "show_player_title", u.show_player_title);
  r.Read("ui", "allow_officer_preset_reordering", u.allow_officer_preset_reordering);
  r.Read("ui", "battle_log_summary_preview", u.battle_log_summary_preview);
  r.Read("ui", "auto_open_bulk_claim_flyout", u.auto_open_bulk_claim_flyout);
  u.daily_bulk_claim_factions = ReadFactions(r);
  r.Read("ui", "daily_bulk_claim_toggle_default_on", u.daily_bulk_claim_toggle_default_on);
  r.Choose("ui", "auto_confirm_instant_warp", u.auto_confirm_instant_warp, kInstantWarp);
  u.instant_warp_auto_jump  = ReadShipFilter(r, "instant_warp_auto_jump");
  u.instant_warp_auto_warp  = ReadShipFilter(r, "instant_warp_auto_warp");
  u.instant_warp_always_ask = ReadShipFilter(r, "instant_warp_always_ask");
  // "*" pins nothing.
  u.pinned_ships = ReadShipFilter(r, "pinned_ships").names;
  r.Read("ui", "double_click_to_assign_ship", u.double_click_to_assign_ship);
  r.Read("ui", "dock_syncmanagement", u.dock_syncmanagement);
  // PLAY accepted the older cross-section key. A present canonical key wins even when malformed.
  if (!r.Node("ui", "focus_search"))
    u.focus_search = r.Get<bool>("patches", "focussearch", u.focus_search);
  r.Read("ui", "focus_search", u.focus_search);
  r.ReadRenamed("ui", "format_cargo_values", "cargo_format", u.format_cargo_values);
  r.Read("ui", "highlight_opc_fleets", u.highlight_opc_fleets);
  r.Read("ui", "fleet_hud_opc_eta", u.fleet_hud_opc_eta);
  r.Read("ui", "officer_sort", u.officer_sort);
  r.Read("ui", "show_ship_tech_indicators", u.show_ship_tech_indicators);
  r.Read("ui", "show_ship_tech_indicator_backgrounds", u.show_ship_tech_indicator_backgrounds);
  r.Read("ui", "arrow_keys_to_select_ship", u.arrow_keys_to_select_ship);
  r.Read("ui", "auto_confirm_ft_upgrade", u.auto_confirm_ft_upgrade);
  r.Read("ui", "forbidden_tech_ship_name", u.forbidden_tech_ship_name);
  r.Read("ui", "auto_confirm_alliance_contribution", u.auto_confirm_alliance_contribution);
  r.Read("ui", "auto_confirm_reveal_ability", u.auto_confirm_reveal_ability);
  r.Read("ui", "extend_chest_purchase_max", u.extend_chest_purchase_max);
  r.Read("ui", "extend_donation_slider", u.extend_donation_slider);
  r.Read("ui", "extend_donation_max", u.extend_donation_max);
  r.Read("ui", "show_cargo_default", u.show_cargo_default);
  r.Read("ui", "show_player_cargo", u.show_player_cargo);
  r.Read("ui", "show_station_cargo", u.show_station_cargo);
  r.Read("ui", "show_hostile_cargo", u.show_hostile_cargo);
  r.Read("ui", "show_armada_cargo", u.show_armada_cargo);
  r.Read("ui", "instant_cargo_counter", u.instant_cargo_counter);
  r.Read("ui", "cargo_significant_decimals", u.cargo_significant_decimals);
  r.Read("ui", "always_skip_reveal_sequence", u.always_skip_reveal_sequence);
  r.ReadRenamed("ui", "disable_exchange_all", "hide_artifact_exchange_all", u.disable_exchange_all);
  r.Choose("ui", "hud_q_trials", u.hud_q_trials, kHudVisibility);
  r.Choose("ui", "hud_field_training", u.hud_field_training, kHudVisibility);
  r.Choose("ui", "hud_outposts", u.hud_outposts, kHudVisibility);
  r.Choose("ui", "hud_missions", u.hud_missions, kHudVisibility);
  u.disabled_banner_types = ReadBannerTypes(r, "disabled_banner_types");
  u.notify_banner_types   = ReadBannerTypes(r, "notify_banner_types");
  r.Read("ui", "notify_incoming_player_attack", u.notify_incoming_player_attack);
  u.notify_fleet_events = ParseFleetNotifications(r.Get<std::string>("ui", "notify_fleet_events", ""));
  r.Record("ui", "notify_fleet_events", FormatFleetNotifications(u.notify_fleet_events));

  // [sync]
  auto sync = std::make_shared<Sync>();
  r.Read("sync", "debug", sync->debug);
  r.Read("sync", "logging", sync->logging);
  r.Read("sync", "resolver_cache_ttl", sync->resolver_cache_ttl);
  SyncConfig sync_defaults;
  r.Read("sync", "proxy", sync_defaults.proxy);
  r.Read("sync", "verify_ssl", sync_defaults.verify_ssl);
  for (const auto& [key, member, old_name] : SyncOptions)
    if (old_name.empty())
      r.Read("sync", key, sync_defaults.*member);
    else
      r.ReadRenamed("sync", key, old_name, sync_defaults.*member);
  ReadSyncTargets(r, sync->targets, sync_defaults);

  r.Retired("sync.file", "capture sync output with a local HTTP server instead");

  // The global options carry [sync]'s proxy and verify_ssl, and for each category whether any target wants it.
  sync->options = {.proxy = sync_defaults.proxy, .verify_ssl = sync_defaults.verify_ssl};
  for (const auto& [key, member, old_name] : SyncOptions)
    sync->options.*member =
        std::ranges::any_of(sync->targets | std::views::values, [&](const auto& target) { return target.*member; });

  // [config]
  r.Read("config", "settings_url", game.settings_url);
  r.Read("config", "assets_url_override", game.assets_url_override);

  shortcuts_ = Shortcut::Parse(file, effective, control.hotkeys_extended, control.enable_experimental);
  sync_      = std::move(sync);
}
