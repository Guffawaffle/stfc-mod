/**
 * @file config.cc
 * @brief Configuration loading, saving, and migration for the Community Mod.
 *
 * Parses community_patch_settings.toml via toml++, applies DefaultConfig
 * fallbacks for every key, writes a merged "runtime vars" snapshot, handles
 * macOS config-path migration, and converts legacy sync options.
 */
#include "config.h"
#include "config_metadata.h"
#include "config_redaction.h"
#include "config_schema.h"
#include "config_sidecar.h"
#include "file.h"
#include "patches/action_queue_repair_config.h"
#include "patches/input_binding/input_config_bridge.h"
#include "patches/input_binding/input_runtime_bindings.h"
#include "patches/mapkey.h"
#include "patches/mod_impact_monitor.h"
#include "patches/notification_policy.h"
#include "prime/KeyCode.h"
#include "str_utils.h"
#include "testable_functions.h"
#include "version.h"
#include <prime/Toast.h>

#include <EASTL/tuple.h>
#include <spdlog/spdlog.h>

#include "defaultconfig.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>

namespace DCP   = DefaultConfig::Patches;
namespace DCG   = DefaultConfig::Graphics;
namespace DCD   = DefaultConfig::Debug;
namespace DCAD  = DefaultConfig::Advanced::Diagnostics;
namespace DCAQ  = DefaultConfig::Advanced::Queue;
namespace DCAKQ = DefaultConfig::Advanced::KirsharaQueue;
namespace DCN   = DefaultConfig::Notifications;
namespace DCC   = DefaultConfig::Control;
namespace DCU   = DefaultConfig::UI;
namespace DCMH  = DefaultConfig::UI::MissionHud;
namespace DCBS  = DefaultConfig::Buffs;
namespace DCS   = DefaultConfig::Sync;
namespace DCSL  = DefaultConfig::Sidecar::Logging;
namespace DCSC  = DefaultConfig::SystemConfig;
namespace DCSH  = DefaultConfig::Shortcuts;
namespace DCBLD = DefaultConfig::BattleLogDecoder;

using config_metadata::BoolConfigSpec;
using config_metadata::kAllowKeyFallthroughConfig;
using config_metadata::kDisableHotkeysShortcutConfig;
using config_metadata::kHotkeysEnabledConfig;
using config_metadata::kHotkeysExtendedConfig;
using config_metadata::kUseScopelyHotkeysConfig;
using config_metadata::NotificationBoolConfigSpec;
using config_metadata::notificationBoolConfigSpecs;
using config_metadata::NotificationToggleSpec;
using config_metadata::notificationToggleSpecs;

static_assert(!DCS::allow_unsafe_tls_without_certificate_validation, "Unsafe TLS override default must remain false.");

// Standalone flag — NOT in Config struct to avoid struct layout sensitivity.
// See: fix/lto-and-sync-crashes for context on why Config struct changes crash.
static bool                      g_allow_key_fallthrough    = false;
static ScopelyShortcutPolicy     g_scopely_shortcuts_policy = ScopelyShortcutPolicy::Off;
static OriginalFramePolicy       g_original_frame_policy    = OriginalFramePolicy::Mod;
static bool                      g_live_debug_channel       = DCAD::live_query;
static bool                      g_queue_repair_enabled     = DCAQ::queue_repair_enabled;
static KirsharaQueueRepairConfig g_kirshara_queue_repair_config{};
static bool                      g_queue_add_direct_handler    = DCAQ::queue_add_direct_handler;
static bool                      g_battle_log_decoder_enabled  = false;
static bool                      g_battle_log_decoder_segments = true;
static bool                      g_battle_log_decoder_feed     = true;
static SidecarConfig             g_sidecar_config{};
static AdvancedConfig            g_advanced_config{};
static int                       g_sidecar_logging_jsonl_replay_seconds = DCSL::jsonl_replay_seconds;
static int                       g_sidecar_logging_jsonl_recent_logs    = DCSL::jsonl_recent_logs;
static bool                      g_refinery_diagnostics                 = DCAD::refinery_diagnostics;
static bool                      g_auto_open_bulk_claim_gifts           = DCU::auto_open_bulk_claim_flyout;

static std::map<std::string, MissionHudVisibility> g_mission_hud_buttons;

static bool              g_mod_impact_monitor               = DCAD::mod_impact_monitor;
static RuntimeTraceLevel g_runtime_trace_level              = RuntimeTraceLevel::Off;
static bool              g_runtime_trace_track_overhead     = DCAD::runtime_trace_track_overhead;
static int               g_runtime_trace_report_interval_ms = DCAD::runtime_trace_report_interval_ms;

/** @brief Accessor for the file-scope allow_key_fallthrough flag. */
bool AllowKeyFallthrough()
{ return g_allow_key_fallthrough; }

ScopelyShortcutPolicy ScopelyShortcutsPolicy()
{ return g_scopely_shortcuts_policy; }

OriginalFramePolicy OriginalFramePolicySetting()
{ return g_original_frame_policy; }

bool LiveDebugChannelEnabled()
{ return g_live_debug_channel; }

bool QueueRepairEnabled()
{ return g_queue_repair_enabled; }

bool KirsharaQueueRepairEnabled()
{ return g_kirshara_queue_repair_config.enabled; }

const KirsharaQueueRepairConfig& KirsharaQueueRepairSettings()
{ return g_kirshara_queue_repair_config; }

bool QueueAddDirectHandlerEnabled()
{ return g_queue_repair_enabled && g_queue_add_direct_handler; }

bool BattleLogDecoderEnabled()
{ return g_battle_log_decoder_enabled; }

const SidecarConfig& SidecarSettings()
{ return g_sidecar_config; }

const AdvancedConfig& AdvancedSettings()
{ return g_advanced_config; }

const SidecarSyncConfig& SidecarSyncSettings()
{ return g_sidecar_config.sync; }

const SidecarProbesConfig& SidecarProbesSettings()
{ return g_sidecar_config.probes; }

const SidecarLoggingConfig& SidecarLoggingSettings()
{ return g_sidecar_config.logging; }

const SidecarDiagnosticsConfig& SidecarDiagnosticsSettings()
{ return g_sidecar_config.diagnostics; }

const AdvancedDiagnosticsConfig& AdvancedDiagnosticsSettings()
{ return g_advanced_config.diagnostics; }

const AdvancedDiagnosticsConfig::FilesConfig& AdvancedDiagnosticsFileSettings()
{ return g_advanced_config.diagnostics.files; }

const AdvancedQueueConfig& AdvancedQueueSettings()
{ return g_advanced_config.queue; }

bool BattleLogDecoderEmitSegments()
{ return g_battle_log_decoder_segments; }

bool BattleLogDecoderEmitFeed()
{ return g_battle_log_decoder_feed; }

int SidecarLoggingJsonlReplaySeconds()
{ return g_sidecar_logging_jsonl_replay_seconds; }

int SidecarLoggingJsonlRecentLogs()
{ return g_sidecar_logging_jsonl_recent_logs; }

bool RefineryDiagnosticsEnabled()
{ return g_refinery_diagnostics; }

bool RepairActionStatusProbeEnabled()
{ return LiveDebugChannelEnabled() && g_advanced_config.diagnostics.ship_state_probe == "repair_action_status"; }

int RepairActionStatusProbeStackBudget()
{ return g_advanced_config.diagnostics.ship_state_probe_stack_budget; }

bool AutoOpenBulkClaimGiftsEnabled()
{ return g_auto_open_bulk_claim_gifts; }

MissionHudVisibility MissionHudButtonVisibility(std::string_view button_name)
{
  const auto it = g_mission_hud_buttons.find(std::string(button_name));
  return it == g_mission_hud_buttons.end() ? MissionHudVisibility::Auto : it->second;
}

bool MissionHudTweaksEnabled()
{
  return std::ranges::any_of(g_mission_hud_buttons,
                             [](const auto& button) { return button.second != MissionHudVisibility::Auto; });
}

bool ModImpactMonitorEnabled()
{ return g_runtime_trace_level != RuntimeTraceLevel::Off; }

RuntimeTraceLevel RuntimeTraceLevelSetting()
{ return g_runtime_trace_level; }

bool RuntimeTraceTrackOverhead()
{ return g_runtime_trace_track_overhead; }

int RuntimeTraceReportIntervalMs()
{ return g_runtime_trace_report_interval_ms; }

/// Human-readable names → ToastState enum values.
/// Used for [ui].disabled_banner_types and legacy [ui] notification allowlists.
static const eastl::tuple<const char*, int> bannerTypes[] = {
    {"Standard", ToastState::Standard},
    {"FactionWarning", ToastState::FactionWarning},
    {"FactionLevelUp", ToastState::FactionLevelUp},
    {"FactionLevelDown", ToastState::FactionLevelDown},
    {"FactionDiscovered", ToastState::FactionDiscovered},
    {"IncomingAttack", ToastState::IncomingAttack},
    {"IncomingAttackFaction", ToastState::IncomingAttackFaction},
    {"FleetBattle", ToastState::FleetBattle},
    {"StationBattle", ToastState::StationBattle},
    {"StationVictory", ToastState::StationVictory},
    {"Victory", ToastState::Victory},
    {"Defeat", ToastState::Defeat},
    {"StationDefeat", ToastState::StationDefeat},
    {"Event", ToastState::Tournament},
    {"Tournament", ToastState::Tournament},
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

bool SyncConfig::enabled(SyncConfig::Type type) const
{
  for (const auto& opt : SyncOptions) {
    if (opt.type == type) {
      return this->*opt.option;
    }
  }

  return false;
}

Config::Config()
{ Load(); }

void Config::Save(const toml::table& config, const std::string_view filename, bool apply_warning)
{
  std::ofstream config_file;

  auto config_path = File::MakePath(filename, true);
  config_file.open(config_path);

  if (apply_warning) {
    char defaultFile[255], configFile[255];
    snprintf(defaultFile, 255, "%s", File::Default());
    snprintf(configFile, 255, "%s", File::Config());

    config_file << "#######################################################################\n";
    config_file << "#######################################################################\n";
    config_file << "####                                                               ####\n";
    config_file << "#### NOTE: This file is not the configuration file that is used    ####\n";
    config_file << "####       by the STFC Community Mod.  It is provided to help      ####\n";
    config_file << "####       see what configuration is being used by the runtime     ####\n";
    config_file << "####       and any desired settings should be copied to the same   ####\n";
    config_file << "####       section in: " << defaultFile << "\n";
    config_file << "####                                                               ####\n";
    config_file << "####        Config in: " << configFile << "\n";
    config_file << "####                                                               ####\n";
    config_file << "#######################################################################\n";
    config_file << "#######################################################################\n\n";
  }

  config_file << config;
  config_file.close();
}

Config& Config::Get()
{
  static Config config;
  return config;
}

#if _WIN32
static HMONITOR lastMonitor = (HMONITOR)-1;
static float    dpi         = 1.0f;

HWND Config::WindowHandle()
{
  static HWND hwnd = nullptr;

  if (hwnd == nullptr) {
    DWORD processId = GetCurrentProcessId();
    hwnd            = GetTopWindow(nullptr); // Start with the first top-level window

    while (hwnd != nullptr) {
      DWORD windowProcessId;
      GetWindowThreadProcessId(hwnd, &windowProcessId);

      // Check if the window belongs to the current process and is the main window
      if (windowProcessId == processId && GetWindow(hwnd, GW_OWNER) == nullptr && IsWindowVisible(hwnd)) {
        break;
      }

      hwnd = GetNextWindow(hwnd, GW_HWNDNEXT); // Move to the next top-level window
    }
  }

  return hwnd;
}

float Config::RefreshDPI()
{
  lastMonitor = (HMONITOR)-1;

  return Config::GetDPI();
}

float Config::GetDPI()
{
  auto     activeWindow = GetActiveWindow();
  HMONITOR monitor      = MonitorFromWindow(activeWindow, MONITOR_DEFAULTTONEAREST);

  if (monitor != lastMonitor) {
    // Get the logical width and height of the monitor
    MONITORINFOEX monitorInfoEx;
    monitorInfoEx.cbSize = sizeof(monitorInfoEx);
    GetMonitorInfo(monitor, &monitorInfoEx);
    auto cxLogical = monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
    auto cyLogical = monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;

    // Get the physical width and height of the monitor
    DEVMODE devMode;
    devMode.dmSize        = sizeof(devMode);
    devMode.dmDriverExtra = 0;
    EnumDisplaySettings(monitorInfoEx.szDevice, ENUM_CURRENT_SETTINGS, &devMode);
    auto cxPhysical = devMode.dmPelsWidth;
    auto cyPhysical = devMode.dmPelsHeight;

    // Calculate the scaling factor
    auto horizontalScale = ((double)cxPhysical / (double)cxLogical);
    auto verticalScale   = ((double)cyPhysical / (double)cyLogical);

    spdlog::trace("Horizontal scaling: {}", horizontalScale);
    spdlog::trace("Vertical scaling: {}", verticalScale);

    dpi         = horizontalScale;
    lastMonitor = monitor;
  }

  return dpi;
}
#else
float Config::RefreshDPI()
{ return Config::GetDPI(); }

float Config::GetDPI()
{ return 1.0f; }
#endif

void Config::AdjustUiScale(bool scaleUp)
{
  if (this->ui_scale != 0.0f) {
    auto old_scale    = this->ui_scale;
    auto scale_factor = (scaleUp ? 1.0f : -1.0f) * this->ui_scale_adjust;
    auto new_scale    = this->ui_scale + scale_factor;
    this->ui_scale    = std::clamp(new_scale, 0.1f, 2.0f);

    auto dpi = Config::RefreshDPI();
    spdlog::info("UI has been scaled {}, was {}, now {} (unclamped {}) @ {} DPI Scaling", (scaleUp ? "UP" : "DOWN"),
                 old_scale, this->ui_scale, new_scale, dpi);
  }
}

void Config::AdjustUiViewerScale(bool scaleUp)
{
  if (this->ui_scale_viewer != 0.0f) {
    auto old_scale        = this->ui_scale_viewer;
    auto scale_factor     = (scaleUp ? 1.0f : -1.0f) * this->ui_scale_adjust;
    auto new_scale        = this->ui_scale_viewer + (scale_factor * 0.25f);
    this->ui_scale_viewer = std::clamp(new_scale, 0.1f, 2.0f);

    spdlog::info("UI Viewer has been scaled {}, was {}, now {} (unclamped {})", (scaleUp ? "UP" : "DOWN"), old_scale,
                 this->ui_scale_viewer, new_scale);
  }
}

/**
 * @brief Mask the middle portion of a bearer token for safe logging.
 * @return The token with interior characters replaced by '*' (preserving dashes).
 */
inline std::string mask_token(const std::string& token)
{ return config_redaction::mask_token_for_log(token); }

inline std::string mask_proxy_for_log(const std::string& proxy)
{ return config_redaction::mask_proxy_userinfo(proxy); }

/** @brief Convert a toml::node_type to a human-readable string for diagnostics. */
std::string get_config_type_as_string(const toml::node_type type)
{
  switch (type) {
    case toml::node_type::none:
      return "Not-a-node.";
    case toml::node_type::table:
      return "toml::table.";
    case toml::node_type::array:
      return "toml::array.";
    case toml::node_type::string:
      return "toml::value<std::string>.";
    case toml::node_type::integer:
      return "toml::value<int64_t>.";
    case toml::node_type::floating_point:
      return "toml::value<double>.";
    case toml::node_type::boolean:
      return "toml::value<bool>.";
    case toml::node_type::date:
      return "toml::value<date>.";
    case toml::node_type::time:
      return "toml::value<time>.";
    case toml::node_type::date_time:
      return "toml::value<date_time>.";
  };

  return "The node type is unknown";
}

bool config_key_exists(toml::table& config, std::string_view section, std::string_view key)
{
  auto* section_table = config[section].as_table();
  return section_table && section_table->contains(key);
}

std::optional<bool> read_bool_config_value_if_present(toml::table& config, std::string_view section,
                                                      std::string_view key, std::string_view docs)
{
  auto* section_table = config[section].as_table();
  if (!section_table || !section_table->contains(key)) {
    return std::nullopt;
  }

  auto* node = section_table->get(key);
  if (!node) {
    return std::nullopt;
  }

  if (auto value = node->value<bool>(); value.has_value()) {
    return value.value();
  }

  spdlog::warn("Invalid boolean config [{}].{} ({}). Found {}; using default.", section, key,
               docs.empty() ? "boolean toggle" : docs, get_config_type_as_string(node->type()));
  return std::nullopt;
}

std::optional<std::string> read_string_config_value_if_present(toml::table& config, std::string_view section,
                                                               std::string_view key, std::string_view docs)
{
  auto* section_table = config[section].as_table();
  if (!section_table || !section_table->contains(key)) {
    return std::nullopt;
  }

  auto* node = section_table->get(key);
  if (!node) {
    return std::nullopt;
  }

  if (auto value = node->value<std::string>(); value.has_value()) {
    return value.value();
  }

  spdlog::warn("Invalid string config [{}].{} ({}). Found {}; using default.", section, key,
               docs.empty() ? "string setting" : docs, get_config_type_as_string(node->type()));
  return std::nullopt;
}

std::optional<ScopelyShortcutPolicy> parse_scopely_shortcut_policy(std::string_view value)
{
  const auto normalized = AsciiStrToUpper(StripAsciiWhitespace(value));

  if (normalized == "OFF") {
    return ScopelyShortcutPolicy::Off;
  }

  if (normalized == "NATIVE") {
    return ScopelyShortcutPolicy::Native;
  }

  if (normalized == "FALLBACK") {
    return ScopelyShortcutPolicy::Fallback;
  }

  return std::nullopt;
}

std::optional<OriginalFramePolicy> parse_original_frame_policy(std::string_view value)
{
  const auto normalized = AsciiStrToUpper(StripAsciiWhitespace(value));

  if (normalized == "MOD") {
    return OriginalFramePolicy::Mod;
  }

  if (normalized == "FALLTHROUGH_UNHANDLED") {
    return OriginalFramePolicy::FallthroughUnhandled;
  }

  if (normalized == "FALLTHROUGH_ALL") {
    return OriginalFramePolicy::FallthroughAll;
  }

  return std::nullopt;
}

struct ScopelyHotkeysConfigValue {
  bool                                 use_scopely_hotkeys = DCC::use_scopely_hotkeys;
  std::optional<ScopelyShortcutPolicy> policy_override;
};

ScopelyHotkeysConfigValue read_scopely_hotkeys_config(toml::table& config, toml::table& new_config, bool write_log)
{
  ScopelyHotkeysConfigValue result;
  auto                      section = kUseScopelyHotkeysConfig.section;
  auto                      key     = kUseScopelyHotkeysConfig.key;

  new_config.emplace<toml::table>(section, toml::table());
  auto* new_section = new_config[section].as_table();

  if (auto* section_table = config[section].as_table(); section_table && section_table->contains(key)) {
    auto* node = section_table->get(key);
    if (auto bool_value = node->value<bool>(); bool_value.has_value()) {
      result.use_scopely_hotkeys = *bool_value;
    } else if (auto string_value = node->value<std::string>(); string_value.has_value()) {
      if (auto policy = parse_scopely_shortcut_policy(*string_value)) {
        result.policy_override     = *policy;
        result.use_scopely_hotkeys = *policy == ScopelyShortcutPolicy::Native;
        spdlog::warn("Config [control].use_scopely_hotkeys='{}' is a legacy policy string. Prefer "
                     "[input].scopely_shortcuts='{}' with [control].use_scopely_hotkeys={}.",
                     *string_value, scopely_shortcut_policy_name(*policy),
                     result.use_scopely_hotkeys ? "true" : "false");
      } else {
        spdlog::warn("Invalid string config [control].use_scopely_hotkeys value='{}'; expected boolean true/false or "
                     "legacy policy off/native/fallback. Using default {}.",
                     *string_value, result.use_scopely_hotkeys);
      }
    } else if (node) {
      spdlog::warn("Invalid config [control].use_scopely_hotkeys ({}). Found {}; using default.",
                   kUseScopelyHotkeysConfig.docs, get_config_type_as_string(node->type()));
    }
  }

  new_section->insert_or_assign(kUseScopelyHotkeysConfig.runtime_key, result.use_scopely_hotkeys);

  if (write_log) {
    spdlog::debug("config value {}.{} value: {}", section, kUseScopelyHotkeysConfig.runtime_key,
                  result.use_scopely_hotkeys);
  }

  return result;
}

void write_input_policy_config(toml::table& new_config, const ScopelyShortcutPolicy scopely_shortcuts,
                               const OriginalFramePolicy original_frame_policy)
{
  new_config.emplace<toml::table>("input", toml::table());
  auto* input = new_config["input"].as_table();
  input->insert_or_assign("scopely_shortcuts", scopely_shortcut_policy_name(scopely_shortcuts));
  input->insert_or_assign("original_frame_policy", original_frame_policy_name(original_frame_policy));
}

void write_runtime_trace_config(toml::table& new_config, const RuntimeTraceLevel level, const bool track_overhead,
                                const bool mod_impact_monitor, const int report_interval_ms)
{
  new_config.emplace<toml::table>("advanced", toml::table());
  auto* advanced = new_config["advanced"].as_table();
  advanced->emplace<toml::table>("diagnostics", toml::table());
  auto* diagnostics = (*advanced)["diagnostics"].as_table();
  diagnostics->insert_or_assign("runtime_trace", RuntimeTraceLevelName(level));
  diagnostics->insert_or_assign("runtime_trace_track_overhead", track_overhead);
  diagnostics->insert_or_assign("mod_impact_monitor", mod_impact_monitor);
  diagnostics->insert_or_assign("runtime_trace_report_interval_ms", report_interval_ms);
}

bool read_bool_config_entry(toml::table& config, toml::table& new_config, std::string_view section,
                            std::string_view key, std::string_view runtime_key, bool default_value,
                            std::string_view docs, bool write_log)
{
  new_config.emplace<toml::table>(section, toml::table());
  auto sectionTable = new_config[section];

  auto final_value = default_value;
  if (auto parsed_value = read_bool_config_value_if_present(config, section, key, docs); parsed_value.has_value()) {
    final_value = parsed_value.value();
  }

  sectionTable.as_table()->insert_or_assign(runtime_key, final_value);

  if (write_log) {
    spdlog::debug("config value {}.{} value: {}", section, runtime_key, final_value);
  }

  return final_value;
}

bool read_bool_config_entry(toml::table& config, toml::table& new_config, const BoolConfigSpec& spec, bool write_log)
{
  return read_bool_config_entry(config, new_config, spec.section, spec.key, spec.runtime_key, spec.default_value,
                                spec.docs, write_log);
}

bool read_bool_config_entry(toml::table& config, toml::table& new_config, const NotificationBoolConfigSpec& spec,
                            bool write_log)
{
  const auto                            legacy_path = config_schema::make_path(spec.section, spec.key);
  const std::array<std::string_view, 1> aliases{legacy_path};

  const config_schema::BoolSetting setting{
      spec.canonical_path,
      spec.default_value,
      aliases,
      spec.docs,
  };

  auto result = config_schema::read_bool(config, setting);
  for (const auto& diagnostic : result.diagnostics) {
    switch (diagnostic.severity) {
      case config_schema::DiagnosticSeverity::Info:
        spdlog::info("[ConfigSchema] info path={} source={} message='{}'", diagnostic.path, diagnostic.source_path,
                     diagnostic.message);
        break;
      case config_schema::DiagnosticSeverity::Warning:
        spdlog::warn("[ConfigSchema] warning path={} source={} message='{}'", diagnostic.path, diagnostic.source_path,
                     diagnostic.message);
        break;
      case config_schema::DiagnosticSeverity::Error:
        spdlog::error("[ConfigSchema] error path={} source={} message='{}'", diagnostic.path, diagnostic.source_path,
                      diagnostic.message);
        break;
    }
  }

  config_schema::write_bool(new_config, spec.canonical_path, result.value);

  new_config.emplace<toml::table>(spec.section, toml::table());
  new_config[spec.section].as_table()->insert_or_assign(spec.runtime_key, result.value);

  if (write_log) {
    if (!result.source_path.empty()) {
      spdlog::debug("config value {} value: {} source: {}", spec.canonical_path, result.value, result.source_path);
    } else {
      spdlog::debug("config value {} value: {} source: default", spec.canonical_path, result.value);
    }
  }

  return result.value;
}

void log_config_diagnostic(const config_schema::Diagnostic& diagnostic)
{
  switch (diagnostic.severity) {
    case config_schema::DiagnosticSeverity::Info:
      spdlog::info("[ConfigSchema] info path={} source={} message='{}'", diagnostic.path, diagnostic.source_path,
                   diagnostic.message);
      break;
    case config_schema::DiagnosticSeverity::Warning:
      spdlog::warn("[ConfigSchema] warning path={} source={} message='{}'", diagnostic.path, diagnostic.source_path,
                   diagnostic.message);
      break;
    case config_schema::DiagnosticSeverity::Error:
      spdlog::error("[ConfigSchema] error path={} source={} message='{}'", diagnostic.path, diagnostic.source_path,
                    diagnostic.message);
      break;
  }
}

bool notification_toggle_key_exists(toml::table& config, const NotificationToggleSpec& spec)
{
  return config_key_exists(config, spec.section, spec.key)
         || (!spec.deprecated_key.empty() && config_key_exists(config, spec.section, spec.deprecated_key));
}

/**
 * @brief Read a single config value from the parsed TOML, falling back to default.
 *
 * Also writes the resolved value into @p new_config so the merged "runtime vars"
 * snapshot reflects what the mod is actually using.
 *
 * @tparam T        Expected value type (bool, int, float, std::string, etc.).
 * @param config     The user-parsed toml::table.
 * @param new_config The output table that accumulates resolved values.
 * @param section    TOML section name (e.g. "graphics").
 * @param item       Key within the section (e.g. "ui_scale").
 * @param default_value  Fallback if the key is missing or invalid.
 * @param write_log  Whether to log the resolved value at debug level.
 * @return The resolved value.
 */
template <typename T>
T get_config_or_default(toml::table& config, toml::table& new_config, std::string_view section, std::string_view item,
                        T default_value, bool write_log)
{
  new_config.emplace<toml::table>(section, toml::table());

  auto sectionTable = new_config[section];
  T    final_value  = default_value;

  try {
    if (config.contains(section)) {
      auto parsed_value = (T)config[section][item].value_or(default_value);
      final_value       = parsed_value;
    }
  } catch (...) {
    spdlog::warn("invalid config value {}.{}", section, item);
  }

  sectionTable.as_table()->insert_or_assign(item, final_value);

  if (write_log) {
    spdlog::debug("config value {}.{} value: {}", section, item, final_value);
  }

  return (T)final_value;
}

struct MissionHudVisibilityConfigSpec {
  std::string_view key;
  std::string_view default_value;
};

constexpr std::array<MissionHudVisibilityConfigSpec, 5> kMissionHudVisibilityConfigSpecs{{
    {"q_trials", DCMH::q_trials},
    {"field_training", DCMH::field_training},
    {"outposts", DCMH::outposts},
    {"daily_goals", DCMH::daily_goals},
    {"missions", DCMH::missions},
}};

std::string_view MissionHudVisibilityName(MissionHudVisibility visibility)
{
  switch (visibility) {
    case MissionHudVisibility::Always:
      return "always";
    case MissionHudVisibility::Never:
      return "never";
    case MissionHudVisibility::Auto:
    default:
      return "auto";
  }
}

MissionHudVisibility ParseMissionHudVisibility(std::string_view item, std::string_view value)
{
  const auto normalized = AsciiStrToLower(StripAsciiWhitespace(value));

  if (normalized == "always") {
    return MissionHudVisibility::Always;
  }
  if (normalized == "never") {
    return MissionHudVisibility::Never;
  }
  if (normalized == "auto" || normalized.empty()) {
    return MissionHudVisibility::Auto;
  }

  spdlog::warn("invalid config value ui.mission_hud.{}: '{}'; using auto", item, value);
  return MissionHudVisibility::Auto;
}

std::string MissionHudVisibilityValue(toml::table& config, std::string_view item, std::string_view default_value)
{
  auto* ui_table = config["ui"].as_table();
  if (!ui_table) {
    return std::string(default_value);
  }

  auto* mission_hud_table = (*ui_table)["mission_hud"].as_table();
  if (!mission_hud_table) {
    if (ui_table->contains("mission_hud")) {
      spdlog::warn("invalid config value ui.mission_hud; expected table");
    }
    return std::string(default_value);
  }

  auto* node = mission_hud_table->get(item);
  if (!node) {
    return std::string(default_value);
  }

  if (auto value = node->value<std::string>(); value.has_value()) {
    return *value;
  }

  spdlog::warn("invalid config value ui.mission_hud.{}; found {}; using auto", item,
               get_config_type_as_string(node->type()));
  return std::string(default_value);
}

toml::table& EnsureUiMissionHudTable(toml::table& config)
{
  config.emplace<toml::table>("ui", toml::table());
  auto* ui_table = config["ui"].as_table();

  if (!ui_table->contains("mission_hud") || !(*ui_table)["mission_hud"].is_table()) {
    ui_table->insert_or_assign("mission_hud", toml::table());
  }

  return *(*ui_table)["mission_hud"].as_table();
}

MissionHudVisibility GetMissionHudVisibility(toml::table& config, toml::table& new_config,
                                             const MissionHudVisibilityConfigSpec& spec, bool write_log)
{
  const auto value = MissionHudVisibilityValue(config, spec.key, spec.default_value);
  const auto mode  = ParseMissionHudVisibility(spec.key, value);

  auto& mission_hud = EnsureUiMissionHudTable(new_config);
  mission_hud.insert_or_assign(spec.key, std::string(MissionHudVisibilityName(mode)));

  if (write_log) {
    spdlog::debug("config value ui.mission_hud.{} value: {}", spec.key, MissionHudVisibilityName(mode));
  }

  return mode;
}

/**
 * @brief Parse all [sync.targets.<name>] tables and populate the targets map.
 *
 * Each target inherits the top-level [sync] defaults for any per-category
 * toggle not explicitly overridden.
 */
void read_sync_targets(toml::table& config, toml::table& new_config,
                       std::map<std::string, SyncTargetConfig>& sync_targets, const SyncConfig& defaults)
{
  if (!config.contains("sync")) {
    return;
  }

  const auto sync = config["sync"].as_table();
  if (!sync || !sync->contains("targets")) {
    return;
  }

  const auto targets = config["sync"]["targets"].as_table();
  if (!targets) {
    return;
  }

  const auto parse_mode = [](const std::string_view            target_section,
                             const std::optional<std::string>& value) -> SyncTargetConfig::Mode {
    if (!value.has_value() || value->empty() || *value == "legacy") {
      return SyncTargetConfig::Mode::Legacy;
    }
    if (*value == "majel") {
      return SyncTargetConfig::Mode::Majel;
    }

    spdlog::warn("Invalid target [{}] mode '{}'; using legacy.", target_section, *value);
    return SyncTargetConfig::Mode::Legacy;
  };

  for (const auto& [target_key, target_config] : *targets) {
    if (!target_config.is_table()) {
      continue;
    }

    const std::string target_section = "sync.targets." + std::string(target_key.str());

    SyncTargetConfig target;
    toml::table      parsed_target;

    const auto& values = *target_config.as_table();
    if (values.contains("url") && values.contains("token")) {
      auto url   = values["url"].value<std::string>();
      auto token = values["token"].value<std::string>();
      auto proxy = values["proxy"].value<std::string>();

      if (!url.has_value() || !token.has_value()) {
        continue;
      }

      target.url        = url.value();
      target.token      = token.value();
      target.proxy      = proxy.value_or(defaults.proxy);
      target.mode       = parse_mode(target_section, values["mode"].value<std::string>());
      target.verify_ssl = values["verify_ssl"].value<bool>().value_or(defaults.verify_ssl);
      target.allow_unsafe_tls_without_certificate_validation =
          values["allow_unsafe_tls_without_certificate_validation"].value<bool>().value_or(
              defaults.allow_unsafe_tls_without_certificate_validation);

      parsed_target.insert("url", target.url);
      parsed_target.insert("token", config_redaction::redact_secret_for_runtime_snapshot(target.token));
      parsed_target.insert("mode", std::string(to_string(target.mode)));
      parsed_target.insert("proxy", config_redaction::mask_proxy_userinfo(target.proxy));
      parsed_target.insert("verify_ssl", target.verify_ssl);
      parsed_target.insert("allow_unsafe_tls_without_certificate_validation",
                           target.allow_unsafe_tls_without_certificate_validation);
    } else {
      spdlog::warn("Skipping invalid target [{}]. Missing url or token.", target_section);
      continue;
    }

    for (const auto& opt : SyncOptions) {
      target.*opt.option = values[opt.option_str].value<bool>().value_or(defaults.*opt.option);
      if (opt.type == SyncConfig::Type::FleetRuntime && target.*opt.option) {
        spdlog::warn("Ignoring unsupported {}.fleet_runtime=true. Fleet runtime delivery is sidecar-only; configure "
                     "[sidecar.sync].fleet_runtime instead.",
                     target_section);
        target.*opt.option = false;
      }
      parsed_target.insert(opt.option_str, target.*opt.option);
    }

    if (sync_targets.emplace(target_key.str(), target).second) {
      new_config["sync"]["targets"].as_table()->emplace<toml::table>(target_key.str(), parsed_target);
      spdlog::debug("config value {} url: {}, token: {}, mode: {}", target_section, target.url,
                    mask_token(target.token), to_string(target.mode));
      spdlog::info(
          "target [{}] mode: {}, proxy: '{}', verify_ssl: {}, allow_unsafe_tls_without_certificate_validation: {}",
          target_section, to_string(target.mode), mask_proxy_for_log(target.proxy), target.verify_ssl,
          target.allow_unsafe_tls_without_certificate_validation);
    }
  }
}

/**
 * @brief Parse a single shortcut config entry and register it with MapKey.
 *
 * Supports pipe-delimited multi-bind strings (e.g. "SPACE|MOUSE1").
 * "NONE" explicitly unbinds the shortcut.
 */
void parse_config_shortcut_value(toml::table& new_config, std::string_view item, GameFunction gameFunction,
                                 std::string_view config_value, std::string_view default_value, bool explicit_value)
{
  auto section = "shortcuts";

  new_config.emplace<toml::table>(section, toml::table());

  auto sectionTable = new_config[section];

  auto valueTrimmed = StripAsciiWhitespace(config_value);
  auto valueLowered = AsciiStrToUpper(valueTrimmed);

  // "NONE" — or an empty/whitespace-only value — explicitly unbinds this shortcut.
  // Empty-string was previously treated as "missing" and silently fell back to the default
  // key (e.g. show_bookmarks="" still bound B). That contradicts user intent: people set
  // show_bookmarks="" precisely to disable the binding. Treat empty the same as "NONE".
  if (valueLowered == "NONE" || valueTrimmed.empty()) {
    sectionTable.as_table()->insert_or_assign(item, "NONE");
    spdlog::debug("shortcut value {}.{} value: NONE (unbound; empty treated as NONE)", section, item);
    return;
  }

  auto wantedKeys = StrSplit(valueLowered, '|');

  bool keyAdded = false;
  for (std::string_view wantedKeyRaw : wantedKeys) {
    const auto wantedKey = StripAsciiWhitespace(wantedKeyRaw);
    MapKey     mapKey    = MapKey::Parse(wantedKey);

    if (mapKey.Key != KeyCode::None) {
      keyAdded = true;
    } else if (!wantedKey.empty()) {
      spdlog::warn("Invalid shortcut token [shortcuts].{} token='{}' value='{}'; ignoring token.", item, wantedKey,
                   config_value);
    }

    if (mapKey.Key != KeyCode::None) {
      MapKey::AddMappedKey(gameFunction, mapKey);
    }
  }

  if (!keyAdded) {
    if (explicit_value) {
      spdlog::warn("No valid shortcut tokens for explicit [shortcuts].{} value='{}'; disabling shortcut.", item,
                   config_value);
      sectionTable.as_table()->insert_or_assign(item, "NONE");
      return;
    }

    spdlog::warn("No valid shortcut tokens for [shortcuts].{} value='{}'; using default '{}'.", item, config_value,
                 default_value);
    MapKey mapKey = MapKey::Parse(default_value);
    MapKey::AddMappedKey(gameFunction, mapKey);
  }

  auto shortcut = MapKey::GetShortcuts(gameFunction);
  sectionTable.as_table()->insert_or_assign(item, shortcut);

  spdlog::debug("shortcut value {}.{} value: {}", section, item, shortcut);
}

void parse_config_shortcut(toml::table& config, toml::table& new_config, std::string_view item,
                           GameFunction gameFunction, std::string_view default_value)
{
  auto section = "shortcuts";

  config.emplace<toml::table>(section, toml::table());

  auto*      shortcuts      = config[section].as_table();
  const auto explicit_value = shortcuts && shortcuts->contains(item);
  auto       config_value   = std::string(default_value);

  if (explicit_value) {
    if (auto value = config[section][item].value<std::string>(); value.has_value()) {
      config_value = *value;
    } else if (auto* node = shortcuts->get(item); node) {
      spdlog::warn("Invalid shortcut config [shortcuts].{} ({}). Found {}; disabling shortcut.", item,
                   "string shortcut binding", get_config_type_as_string(node->type()));
      config_value = "NONE";
    }
  }

  parse_config_shortcut_value(new_config, item, gameFunction, config_value, default_value, explicit_value);
}

bool shortcut_key_exists(toml::table& config, std::string_view item)
{
  auto* shortcuts = config["shortcuts"].as_table();
  return shortcuts && shortcuts->contains(item);
}

std::string shortcut_value_or_default(toml::table& config, std::string_view item, std::string_view default_value)
{
  auto* shortcuts = config["shortcuts"].as_table();
  if (!shortcuts || !shortcuts->contains(item)) {
    return std::string(default_value);
  }

  if (auto value = config["shortcuts"][item].value<std::string>(); value.has_value()) {
    return *value;
  }

  if (auto* node = shortcuts->get(item); node) {
    spdlog::warn("Invalid shortcut config [shortcuts].{} ({}). Found {}; disabling shortcut.", item,
                 "string shortcut binding", get_config_type_as_string(node->type()));
  }
  return "NONE";
}

void parse_disable_hotkeys_shortcut(toml::table& config, toml::table& new_config)
{
  auto section = kDisableHotkeysShortcutConfig.section;
  config.emplace<toml::table>(section, toml::table());

  HotkeyDisableShortcutAliasInput input;
  input.has_canonical = shortcut_key_exists(config, kDisableHotkeysShortcutConfig.key);
  input.canonical =
      shortcut_value_or_default(config, kDisableHotkeysShortcutConfig.key, kDisableHotkeysShortcutConfig.default_value);
  input.has_deprecated_typo = shortcut_key_exists(config, kDisableHotkeysShortcutConfig.deprecated_typo_key);
  input.deprecated_typo     = shortcut_value_or_default(config, kDisableHotkeysShortcutConfig.deprecated_typo_key,
                                                        kDisableHotkeysShortcutConfig.default_value);
  input.has_legacy_disabled = shortcut_key_exists(config, kDisableHotkeysShortcutConfig.legacy_key);
  input.legacy_disabled     = shortcut_value_or_default(config, kDisableHotkeysShortcutConfig.legacy_key,
                                                        kDisableHotkeysShortcutConfig.default_value);
  input.default_value       = kDisableHotkeysShortcutConfig.default_value;

  const auto decision = resolve_hotkey_disable_shortcut_alias(input);
  if (decision.used_deprecated_alias) {
    spdlog::warn("Deprecation Warning: [shortcuts].{} is deprecated. Use {} instead.", decision.source_key,
                 kDisableHotkeysShortcutConfig.key);
  } else if (decision.saw_deprecated_alias) {
    spdlog::warn(
        "Deprecation Warning: deprecated disable-hotkeys shortcut aliases are ignored because [shortcuts].{} is set.",
        kDisableHotkeysShortcutConfig.key);
  }

  if (decision.has_conflicting_alias) {
    spdlog::warn("Conflicting disable-hotkeys shortcut aliases detected. Using [shortcuts].{} value '{}'.",
                 decision.source_key, decision.value);
  }

  const auto has_explicit_shortcut = input.has_canonical || input.has_deprecated_typo || input.has_legacy_disabled;
  parse_config_shortcut_value(new_config, kDisableHotkeysShortcutConfig.runtime_key, GameFunction::DisableHotKeys,
                              decision.value, kDisableHotkeysShortcutConfig.default_value, has_explicit_shortcut);
}

/**
 * @brief Migrate macOS config from the old bundle-id directory to the new one.
 *
 * Moves ~/Library/Preferences/com.tashcan.startrekpatch/ to
 * com.stfcmod.startrekpatch/ and leaves a symlink + info file at the old location.
 */
void migrate_mac_config_if_needed(const char* filename)
{
#if !_WIN32
  namespace fs = std::filesystem;

  fs::path file_path = File::MakePath(filename);
  auto     new_dir   = file_path.parent_path();
  if (fs::exists(file_path) || fs::exists(new_dir))
    return;

  spdlog::info("mac config migration: config dir does not exist, checking for migration...");

  fs::path old_path = File::MakePath(filename, false, true);
  if (!fs::exists(old_path)) {
    spdlog::info("mac config migration: old config does not exist. nothing to migrate.");
    return;
  }

  auto stat = fs::status(old_path);
  if (stat.type() == fs::file_type::regular) {
    spdlog::info("mac config migration: old config found. attempting to migrate...");

    try {
      // move
      auto old_dir = old_path.parent_path();
      fs::rename(old_dir, new_dir);

      // re-create old dir, create symlink
      fs::create_directories(old_dir);
      fs::create_symlink(file_path, old_path);

      // drop update-info.txt
      std::ofstream info;
      info.open(old_dir / "update-info.txt");
      info << "Your config has been moved!\n\n";
      info << "You can now find your config at " << file_path << "\n\n";
      info << "A symlink has been placed for your convenience, but it is generally recommended, that you use the new "
              "path and delete this directory going forward.";
      info.close();

      spdlog::info("mac config migration: config migration done.");
    } catch (std::exception& ex) {
      spdlog::warn("mac config migration: migration failed = {}", ex.what());
    }
  }
#endif
}

/** @brief Remove the old misspelled vars file (community_path_runtime.vars). */
void delete_old_vars()
{
  namespace fs = std::filesystem;

  fs::path        old_vars = fs::path(File::MakePath(File::Vars())).parent_path() / FILE_DEF_VARS_OLD;
  std::error_code ignore;
  fs::remove(old_vars, ignore);
}

void Config::Load()
{
  auto filename = File::Config();

  migrate_mac_config_if_needed(filename);
  delete_old_vars();

  toml::table config;
  toml::table parsed;
  bool        write_config = false;
  bool        write_log    = true;
  try {
    config       = std::move(toml::parse_file(File::MakePath(filename)));
    write_config = true;
  } catch (const toml::parse_error& e) {
    spdlog::warn("Failed to load config file, falling back to default settings: {}", e.description());
    spdlog::debug("");
    write_config = false;
    write_log    = false;
  } catch (...) {
    spdlog::warn("Failed to load config file, falling back to default settings");
    spdlog::debug("");
    write_config = false;
    write_log    = false;
  }

  this->installUiScaleHooks =
      get_config_or_default(config, parsed, "patches", "uiscalehooks", DCP::uiscalehooks, write_config);
  this->installZoomHooks = get_config_or_default(config, parsed, "patches", "zoomhooks", DCP::zoomhooks, write_config);
  this->installBuffFixHooks =
      get_config_or_default(config, parsed, "patches", "bufffixhooks", DCP::bufffixhooks, write_config);
  this->installToastBannerHooks =
      get_config_or_default(config, parsed, "patches", "toastbannerhooks", DCP::toastbannerhooks, write_config);
  this->installPanHooks = get_config_or_default(config, parsed, "patches", "panhooks", DCP::panhooks, write_config);
  this->installImproveResponsivenessHooks = get_config_or_default(
      config, parsed, "patches", "improveresponsivenesshooks", DCP::improveresponsivenesshooks, write_config);
  this->installHotkeyHooks =
      get_config_or_default(config, parsed, "patches", "hotkeyhooks", DCP::hotkeyhooks, write_config);
  this->installFreeResizeHooks =
      get_config_or_default(config, parsed, "patches", "freeresizehooks", DCP::freeresizehooks, write_config);
  this->installTempCrashFixes =
      get_config_or_default(config, parsed, "patches", "tempcrashfixes", DCP::tempcrashfixes, write_config);
  this->installTestPatches =
      get_config_or_default(config, parsed, "patches", "testpatches", DCP::testpatches, write_config);
  this->installMiscPatches =
      get_config_or_default(config, parsed, "patches", "miscpatches", DCP::miscpatches, write_config);
  this->installChatPatches =
      get_config_or_default(config, parsed, "patches", "chatpatches", DCP::chatpatches, write_config);
  this->installResolutionListFix =
      get_config_or_default(config, parsed, "patches", "resolutionlistfix", DCP::resolutionlistfix, write_config);
  this->installSyncPatches =
      get_config_or_default(config, parsed, "patches", "syncpatches", DCP::syncpatches, write_config);
  this->installGameVersionHook =
      get_config_or_default(config, parsed, "patches", "game_version", DCP::game_version, write_config);
  this->installObjectTracker =
      get_config_or_default(config, parsed, "patches", "objecttracker", DCP::objecttracker, write_config);
  this->installFleetArrivalHooks =
      get_config_or_default(config, parsed, "patches", "fleetarrivalhooks", DCP::fleetarrivalhooks, write_config);
  this->installLoadingScreenHooks =
      get_config_or_default(config, parsed, "patches", "loadingscreenhooks", DCP::loadingscreenhooks, write_config);
  this->installTransitionScreenHooks = get_config_or_default(config, parsed, "patches", "transitionscreenhooks",
                                                             DCP::transitionscreenhooks, write_config);
  spdlog::debug("");

  this->queue_enabled =
      get_config_or_default(config, parsed, "control", "queue_enabled", DCC::queue_enabled, write_config);
  this->hotkeys_enabled  = read_bool_config_entry(config, parsed, kHotkeysEnabledConfig, write_config);
  this->hotkeys_extended = read_bool_config_entry(config, parsed, kHotkeysExtendedConfig, write_config);
  const auto legacy_scopely_hotkeys_config = read_scopely_hotkeys_config(config, parsed, write_config);
  this->use_scopely_hotkeys                = legacy_scopely_hotkeys_config.use_scopely_hotkeys;
  this->select_timer =
      get_config_or_default(config, parsed, "control", "select_timer", DCC::select_timer, write_config);
  this->enable_experimental =
      get_config_or_default(config, parsed, "control", "enable_experimental", DCC::enable_experimental, write_config);

  g_allow_key_fallthrough    = read_bool_config_entry(config, parsed, kAllowKeyFallthroughConfig, write_config);
  g_scopely_shortcuts_policy = resolve_scopely_shortcut_policy(this->use_scopely_hotkeys, g_allow_key_fallthrough);
  g_original_frame_policy    = resolve_original_frame_policy(g_allow_key_fallthrough);

  if (legacy_scopely_hotkeys_config.policy_override) {
    g_scopely_shortcuts_policy = *legacy_scopely_hotkeys_config.policy_override;
  }

  const auto explicit_scopely_policy = config_key_exists(config, "input", "scopely_shortcuts");
  if (auto policy_value = read_string_config_value_if_present(config, "input", "scopely_shortcuts",
                                                              "Scopely shortcut initialization policy.")) {
    if (auto policy = parse_scopely_shortcut_policy(*policy_value)) {
      g_scopely_shortcuts_policy = *policy;
    } else {
      spdlog::warn("Invalid string config [input].scopely_shortcuts value='{}'; expected off, native, or fallback. "
                   "Using {}.",
                   *policy_value, scopely_shortcut_policy_name(g_scopely_shortcuts_policy));
    }
  }

  const auto explicit_frame_policy = config_key_exists(config, "input", "original_frame_policy");
  if (auto policy_value = read_string_config_value_if_present(config, "input", "original_frame_policy",
                                                              "Original ScreenManager::Update call policy.")) {
    if (auto policy = parse_original_frame_policy(*policy_value)) {
      g_original_frame_policy = *policy;
    } else {
      spdlog::warn("Invalid string config [input].original_frame_policy value='{}'; expected mod, "
                   "fallthrough_unhandled, or fallthrough_all. Using {}.",
                   *policy_value, original_frame_policy_name(g_original_frame_policy));
    }
  }

  const auto legacy_control_frame_policy = config_key_exists(config, "control", "original_frame_policy");
  if (!explicit_frame_policy) {
    if (auto policy_value = read_string_config_value_if_present(config, "control", "original_frame_policy",
                                                                "Original ScreenManager::Update call policy.")) {
      if (auto policy = parse_original_frame_policy(*policy_value)) {
        g_original_frame_policy = *policy;
        spdlog::warn("Config [control].original_frame_policy='{}' is a compatibility alias. Prefer "
                     "[input].original_frame_policy='{}'.",
                     *policy_value, original_frame_policy_name(*policy));
      } else {
        spdlog::warn("Invalid string config [control].original_frame_policy value='{}'; expected mod, "
                     "fallthrough_unhandled, or fallthrough_all. Using {}.",
                     *policy_value, original_frame_policy_name(g_original_frame_policy));
      }
    }
  } else if (legacy_control_frame_policy) {
    spdlog::warn("Ignoring [control].original_frame_policy because [input].original_frame_policy is set.");
  }

  write_input_policy_config(parsed, g_scopely_shortcuts_policy, g_original_frame_policy);

  spdlog::info(
      "[Hotkeys] config installHotkeyHooks={} hotkeys_enabled={} use_scopely_hotkeys={} allow_key_fallthrough={} "
      "scopely_shortcuts={} original_frame_policy={}",
      this->installHotkeyHooks, this->hotkeys_enabled, this->use_scopely_hotkeys, g_allow_key_fallthrough,
      scopely_shortcut_policy_name(g_scopely_shortcuts_policy), original_frame_policy_name(g_original_frame_policy));

  if (g_allow_key_fallthrough && !this->use_scopely_hotkeys && !explicit_scopely_policy && !explicit_frame_policy
      && !legacy_scopely_hotkeys_config.policy_override && !legacy_control_frame_policy) {
    spdlog::warn("[Hotkeys] legacy allow_key_fallthrough=true now leaves scopely_shortcuts={} and maps "
                 "original_frame_policy={}. Native shortcut execution must be explicit or configured with "
                 "[input].scopely_shortcuts.",
                 scopely_shortcut_policy_name(g_scopely_shortcuts_policy),
                 original_frame_policy_name(g_original_frame_policy));
  }

  spdlog::debug("");

  this->ui_scale = get_config_or_default(config, parsed, "graphics", "ui_scale", DCG::ui_scale, write_config);
  this->ui_scale_adjust =
      get_config_or_default(config, parsed, "graphics", "ui_scale_adjust", DCG::ui_scale_adjust, write_config);
  this->ui_scale_viewer =
      get_config_or_default(config, parsed, "graphics", "ui_scale_viewer", DCG::ui_scale_viewer, write_config);
  this->zoom        = get_config_or_default(config, parsed, "graphics", "zoom", DCG::zoom, write_config);
  this->fr_scale    = get_config_or_default(config, parsed, "graphics", "fr_scale", DCG::fr_scale, write_config);
  this->free_resize = get_config_or_default(config, parsed, "graphics", "free_resize", DCG::free_resize, write_config);
  this->allow_cursor =
      get_config_or_default(config, parsed, "graphics", "allow_cursor", DCG::allow_cursor, write_config);
  this->keyboard_zoom_speed =
      get_config_or_default(config, parsed, "graphics", "keyboard_zoom_speed", DCG::keyboard_zoom_speed, write_config);

  if (this->enable_experimental) {
    this->system_pan_momentum = get_config_or_default(config, parsed, "graphics", "system_pan_momentum",
                                                      DCG::system_pan_momentum, write_config);
  }

  spdlog::debug("");

  this->system_pan_momentum_falloff = get_config_or_default(config, parsed, "graphics", "system_pan_momentum_falloff",
                                                            DCG::system_pan_momentum_falloff, write_log);
  this->borderless_fullscreen =
      get_config_or_default(config, parsed, "graphics", "borderless_fullscreen", DCG::borderless_fullscreen, write_log);
  this->transition_time =
      get_config_or_default(config, parsed, "graphics", "transition_time", DCG::transition_time, write_config);
  this->show_all_resolutions = get_config_or_default(config, parsed, "graphics", "show_all_resolutions",
                                                     DCG::show_all_resolutions, write_config);
  this->default_system_zoom =
      get_config_or_default(config, parsed, "graphics", "default_system_zoom", DCG::default_system_zoom, write_config);

  spdlog::debug("");

  this->system_zoom_preset_1   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_1",
                                                       DCG::system_zoom_preset_1, write_config);
  this->system_zoom_preset_2   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_2",
                                                       DCG::system_zoom_preset_2, write_config);
  this->system_zoom_preset_3   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_3",
                                                       DCG::system_zoom_preset_3, write_config);
  this->system_zoom_preset_4   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_4",
                                                       DCG::system_zoom_preset_4, write_config);
  this->system_zoom_preset_5   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_5",
                                                       DCG::system_zoom_preset_5, write_config);
  this->use_presets_as_default = get_config_or_default(config, parsed, "graphics", "use_presets_as_default",
                                                       DCG::use_presets_as_default, write_config);

  spdlog::debug("");

  this->use_out_of_dock_power = get_config_or_default(config, parsed, "buffs", "use_out_of_dock_power",
                                                      DCBS::use_out_of_dock_power, write_config);

  spdlog::debug("");

  this->disable_escape_exit =
      get_config_or_default(config, parsed, "ui", "disable_escape_exit", DCU::disable_escape_exit, write_config);
  this->escape_exit_timer =
      get_config_or_default(config, parsed, "ui", "escape_exit_timer", DCU::escape_exit_timer, write_config);
  this->disable_preview_locate =
      get_config_or_default(config, parsed, "ui", "disable_preview_locate", DCU::disable_preview_locate, write_config);
  this->disable_preview_recall =
      get_config_or_default(config, parsed, "ui", "disable_preview_recall", DCU::disable_preview_recall, write_config);
  this->disable_first_popup =
      get_config_or_default(config, parsed, "ui", "disable_first_popup", DCU::disable_first_popup, write_config);
  this->disable_move_keys =
      get_config_or_default(config, parsed, "ui", "disable_move_keys", DCU::disable_move_keys, write_config);
  this->disable_toast_banners =
      get_config_or_default(config, parsed, "ui", "disable_toast_banners", DCU::disable_toast_banners, write_config);
  g_auto_open_bulk_claim_gifts = get_config_or_default(config, parsed, "ui", "auto_open_bulk_claim_flyout",
                                                       DCU::auto_open_bulk_claim_flyout, write_config);

#if _WIN32
  this->extend_donation_slider =
      get_config_or_default(config, parsed, "ui", "extend_donation_slider", DCU::extend_donation_slider, write_config);
  this->extend_donation_max =
      get_config_or_default(config, parsed, "ui", "extend_donation_max", DCU::extend_donation_max, write_config);
#endif

  this->disable_galaxy_chat =
      get_config_or_default(config, parsed, "ui", "disable_galaxy_chat", DCU::disable_galaxy_chat, write_config);
  this->disable_veil_chat =
      get_config_or_default(config, parsed, "ui", "disable_veil_chat", DCU::disable_veil_chat, write_config);
  this->show_cargo_default =
      get_config_or_default(config, parsed, "ui", "show_cargo_default", DCU::show_cargo_default, write_config);
  this->show_player_cargo =
      get_config_or_default(config, parsed, "ui", "show_player_cargo", DCU::show_player_cargo, write_config);
  this->show_station_cargo =
      get_config_or_default(config, parsed, "ui", "show_station_cargo", DCU::show_station_cargo, write_config);
  this->show_hostile_cargo =
      get_config_or_default(config, parsed, "ui", "show_hostile_cargo", DCU::show_hostile_cargo, write_config);
  this->show_armada_cargo =
      get_config_or_default(config, parsed, "ui", "show_armada_cargo", DCU::show_armada_cargo, write_config);

  this->always_skip_reveal_sequence = get_config_or_default(config, parsed, "ui", "always_skip_reveal_sequence",
                                                            DCU::always_skip_reveal_sequence, write_config);
  g_mission_hud_buttons.clear();
  for (const auto& spec : kMissionHudVisibilityConfigSpecs) {
    g_mission_hud_buttons.emplace(std::string(spec.key), GetMissionHudVisibility(config, parsed, spec, write_config));
  }

  spdlog::debug("");

  this->sync_debug   = get_config_or_default(config, parsed, "sync", "debug", DCS::debug, write_config);
  this->sync_logging = get_config_or_default(config, parsed, "sync", "logging", DCS::logging, write_config);
  const auto sidecar_config_result = ParseSidecarConfig(config);
  g_sidecar_config                 = sidecar_config_result.config;
  g_advanced_config                = sidecar_config_result.advanced;
  for (const auto& diagnostic : sidecar_config_result.diagnostics) {
    log_config_diagnostic(diagnostic);
  }
  const auto kirshara_queue_repair_config_result = ParseKirsharaQueueRepairConfig(config);
  for (const auto& diagnostic : kirshara_queue_repair_config_result.diagnostics) {
    log_config_diagnostic(diagnostic);
  }
  this->sidecar_logging_jsonl            = g_sidecar_config.logging.jsonl;
  g_sidecar_logging_jsonl_replay_seconds = std::max(0, g_sidecar_config.logging.jsonl_replay_seconds);
  g_sidecar_logging_jsonl_recent_logs    = std::max(0, g_sidecar_config.logging.jsonl_recent_logs);
  WriteSidecarConfigRuntimeSnapshot(parsed, g_sidecar_config);
  WriteAdvancedConfigRuntimeSnapshot(parsed, g_advanced_config);
  WriteKirsharaQueueRepairRuntimeSnapshot(parsed, kirshara_queue_repair_config_result.config);
  g_live_debug_channel           = g_advanced_config.diagnostics.live_query;
  g_kirshara_queue_repair_config = kirshara_queue_repair_config_result.config;
  g_queue_repair_enabled         = g_advanced_config.queue.queue_repair_enabled;
  g_queue_add_direct_handler     = g_advanced_config.queue.queue_add_direct_handler;
  g_refinery_diagnostics         = g_advanced_config.diagnostics.refinery_diagnostics;

  const auto* advanced_table             = config["advanced"].as_table();
  const auto* advanced_diagnostics_table = advanced_table ? (*advanced_table)["diagnostics"].as_table() : nullptr;
  const auto  explicit_runtime_trace =
      advanced_diagnostics_table && advanced_diagnostics_table->contains("runtime_trace");
  g_mod_impact_monitor  = g_advanced_config.diagnostics.mod_impact_monitor;
  g_runtime_trace_level = g_mod_impact_monitor ? RuntimeTraceLevel::Summary : RuntimeTraceLevel::Off;
  if (explicit_runtime_trace) {
    const auto normalized_trace_level = AsciiStrToLower(g_advanced_config.diagnostics.runtime_trace);
    if (auto level = ParseRuntimeTraceLevel(normalized_trace_level)) {
      g_runtime_trace_level = *level;
    } else {
      spdlog::warn(
          "Invalid string config [advanced.diagnostics].runtime_trace value='{}'; expected off, summary, detailed, "
          "or verbose. Using {}.",
          g_advanced_config.diagnostics.runtime_trace, RuntimeTraceLevelName(g_runtime_trace_level));
    }
  }

  g_runtime_trace_track_overhead     = g_advanced_config.diagnostics.runtime_trace_track_overhead;
  g_runtime_trace_report_interval_ms = g_advanced_config.diagnostics.runtime_trace_report_interval_ms;
  g_runtime_trace_report_interval_ms = std::clamp(g_runtime_trace_report_interval_ms, 1000, 60000);
  write_runtime_trace_config(parsed, g_runtime_trace_level, g_runtime_trace_track_overhead, g_mod_impact_monitor,
                             g_runtime_trace_report_interval_ms);
  ConfigureModImpactRuntimeTrace(g_runtime_trace_level, g_runtime_trace_track_overhead,
                                 g_runtime_trace_report_interval_ms);
  g_battle_log_decoder_enabled = g_sidecar_config.sync.battlelog_enrichment;
  config_schema::write_bool(parsed, "battle_log_decoder.enabled", g_battle_log_decoder_enabled);
  if (g_sidecar_config.sync.enabled && g_sidecar_config.sync.battlelogs_realtime && !g_battle_log_decoder_enabled) {
    spdlog::info(
        "sidecar.sync.battlelogs_realtime is enabled without sidecar.sync.battlelog_enrichment; local sidecar battle "
        "feeds will remain capture-only.");
  }
  g_battle_log_decoder_segments =
      get_config_or_default(config, parsed, "battle_log_decoder", "emit_segments", DCBLD::emit_segments, write_config);
  g_battle_log_decoder_feed =
      get_config_or_default(config, parsed, "battle_log_decoder", "emit_feed", DCBLD::emit_feed, write_config);
  this->sync_resolver_cache_ttl =
      get_config_or_default(config, parsed, "sync", "resolver_cache_ttl", DCS::resolver_cache_ttl, write_config);

  SyncConfig sync_defaults;
  sync_defaults.proxy = get_config_or_default<std::string>(config, parsed, "sync", "proxy", DCS::proxy, false);
  parsed["sync"].as_table()->insert_or_assign("proxy", config_redaction::mask_proxy_userinfo(sync_defaults.proxy));
  if (write_log) {
    spdlog::debug("config value sync.proxy value: {}", mask_proxy_for_log(sync_defaults.proxy));
  }
  sync_defaults.verify_ssl = get_config_or_default(config, parsed, "sync", "verify_ssl", DCS::verify_ssl, write_config);
  sync_defaults.allow_unsafe_tls_without_certificate_validation =
      get_config_or_default(config, parsed, "sync", "allow_unsafe_tls_without_certificate_validation",
                            DCS::allow_unsafe_tls_without_certificate_validation, write_config);

  for (const auto& opt : SyncOptions) {
    sync_defaults.*opt.option = get_config_or_default(config, parsed, "sync", opt.option_str, false, write_config);
  }
  if (sync_defaults.fleet_runtime) {
    spdlog::warn("Ignoring unsupported [sync].fleet_runtime=true. Fleet runtime delivery is sidecar-only; configure "
                 "[sidecar.sync].fleet_runtime instead.");
    sync_defaults.fleet_runtime = false;
    parsed["sync"].as_table()->insert_or_assign("fleet_runtime", false);
  }

  spdlog::debug("");

  parsed["sync"].as_table()->emplace<toml::table>("targets", toml::table());
  read_sync_targets(config, parsed, this->sync_targets, sync_defaults);

  if (auto* rejected_targets = parsed["sync"]["targets"].as_table()) {
    std::set<std::string> rejected_target_names;
    for (const auto& rejected : sidecar_config_result.rejected_sync_targets) {
      rejected_target_names.emplace(rejected.target_name);
    }

    for (const auto& target_name : rejected_target_names) {
      this->sync_targets.erase(target_name);
      rejected_targets->erase(target_name);
    }
  }

  // handle legacy sync options
  auto sync_url   = config["sync"]["url"].value<std::string>();
  auto sync_token = config["sync"]["token"].value<std::string>();

  const bool legacy_sync_url_configured   = sync_url.has_value() && !sync_url->empty();
  const bool legacy_sync_token_configured = sync_token.has_value() && !sync_token->empty();

  if (legacy_sync_url_configured && legacy_sync_token_configured) {
    if (sidecar_config_result.reject_legacy_sync_url) {
      spdlog::error(
          "Ignoring legacy [sync].url / [sync].token loopback sidecar endpoint. Configure [sidecar.sync] instead.");
    } else {
      spdlog::warn("Deprecation Warning: Legacy config options 'sync_url' and 'sync_token' have been moved to "
                   "[sync.targets.<name>] sections and may be removed in a future version.");

      SyncTargetConfig converted_target;
      static_cast<SyncConfig&>(converted_target) = sync_defaults;
      converted_target.url                       = sync_url.value();
      converted_target.token                     = sync_token.value();

      if (this->sync_targets.emplace("default", converted_target).second) {
        toml::table default_target{
            {"url", sync_url.value()},
            {"token", config_redaction::redact_secret_for_runtime_snapshot(converted_target.token)},
            {"proxy", config_redaction::mask_proxy_userinfo(converted_target.proxy)},
            {"verify_ssl", converted_target.verify_ssl},
            {"allow_unsafe_tls_without_certificate_validation",
             converted_target.allow_unsafe_tls_without_certificate_validation}};
        for (const auto& opt : SyncOptions) {
          default_target.insert(opt.option_str, converted_target.*opt.option);
        }
        parsed["sync"]["targets"].as_table()->emplace<toml::table>("default", default_target);
        spdlog::info("Legacy config options 'sync_url' and 'sync_token' were converted to sync.targets.default url: "
                     "{}, token: {}",
                     sync_url.value(), mask_token(sync_token.value()));
      } else {
        spdlog::error(
            "Failed to convert legacy config options sync_url: {} and sync_token: {} as [sync.targets.default] "
            "was already specified.",
            sync_url.value(), mask_token(sync_token.value()));
      }
    }
  } else if (legacy_sync_url_configured || legacy_sync_token_configured) {
    spdlog::warn("Ignoring incomplete legacy [sync].url / [sync].token configuration. Both values must be non-empty "
                 "to create sync.targets.default.");
  }

  if (auto sync_file = config["sync"]["file"].value<std::string>();
      sync_file.has_value() && !sync_file.value().empty()) {
    spdlog::error("Deprecation Notice: The 'sync_file' config option has been deprecated and removed. "
                  "For capturing sync output, please use a local HTTP server instead.");
  }

  // set global sync options to what's actually used in targets
  const auto targets_view = this->sync_targets | std::views::values;

  this->sync_options.proxy      = sync_defaults.proxy;
  this->sync_options.verify_ssl = sync_defaults.verify_ssl;
  this->sync_options.allow_unsafe_tls_without_certificate_validation =
      sync_defaults.allow_unsafe_tls_without_certificate_validation;

  for (const auto& opt : SyncOptions) {
    this->sync_options.*opt.option =
        std::ranges::any_of(targets_view, [opt](const auto& target) { return target.*opt.option; });
  }

  spdlog::debug("");

  // must explicitly include std::string typing here, or we get back char * which fails us!
  auto disabled_banner_types_str = get_config_or_default<std::string>(config, parsed, "ui", "disabled_banner_types",
                                                                      DCU::disabled_banner_types, write_log);

  this->config_settings_url =
      get_config_or_default<std::string>(config, parsed, "config", "settings_url", DCSC::settings_url, write_log);
  this->config_assets_url_override = get_config_or_default<std::string>(config, parsed, "config", "assets_url_override",
                                                                        DCSC::assets_url_override, write_log);

  // Loading Screen / Transition Screen settings
  this->loader_enabled =
      get_config_or_default(config, parsed, "graphics", "loader_enabled", DCG::loader_enabled, write_log);
  this->loader_transition =
      get_config_or_default(config, parsed, "graphics", "loader_transition", DCG::loader_transition, write_log);
  this->loader_transition_black = get_config_or_default(config, parsed, "graphics", "loader_transition_black",
                                                        DCG::loader_transition_black, write_log);
  if (!this->loader_transition)
    this->loader_transition_black = true;
#ifdef _USE_ORIGINAL_BG
  this->loader_transition_black = true;
#endif
  this->loader_image =
      get_config_or_default<std::string>(config, parsed, "graphics", "loader_image", DCG::loader_image, write_log);
  this->loader_logo_scale =
      get_config_or_default(config, parsed, "graphics", "loader_logo_scale", DCG::loader_logo_scale, write_log);
  this->loader_tip_enabled =
      get_config_or_default(config, parsed, "graphics", "loader_tip_enabled", DCG::loader_tip_enabled, write_log);

  std::vector<std::string> types = StrSplit(disabled_banner_types_str, ',');

  spdlog::debug("");

  std::string       bannerString;
  std::stringstream message;
  message << "Parsing banner strings";

  spdlog::debug(message.str());

  for (const auto& [key, value] : bannerTypes) {
    auto upper_key = AsciiStrToUpper(key);

    for (const std::string_view _type : types) {
      auto stripped_type = StripLeadingAsciiWhitespace(_type);
      auto upper_type    = AsciiStrToUpper(stripped_type);

      if (upper_key == upper_type) {
        this->disabled_banner_types.emplace_back(value);
        if (!bannerString.empty()) {
          bannerString.append(", ");
        }
        bannerString.append(key);
      }
    }
  }

  message.str("");
  message << "Final disabledbanner types: " << bannerString;
  spdlog::debug(message.str());

  parsed["ui"].as_table()->insert_or_assign("disabled_banner_types", bannerString);

  auto*      notifications_table = config["notifications"].as_table();
  const bool has_explicit_notification_toggles =
      notifications_table && std::ranges::any_of(notificationToggleSpecs, [&config](const auto& spec) {
        return notification_toggle_key_exists(config, spec);
      });

  auto*       ui_table = config["ui"].as_table();
  std::string legacy_notify_banner_types;
  bool        has_legacy_notify_banner_types = false;
  if (ui_table) {
    if (auto notify_on_value = config["ui"]["notify_on_banner_types"].value<std::string>();
        notify_on_value.has_value()) {
      legacy_notify_banner_types     = notify_on_value.value();
      has_legacy_notify_banner_types = true;
    } else if (auto notify_value = config["ui"]["notify_banner_types"].value<std::string>(); notify_value.has_value()) {
      legacy_notify_banner_types     = notify_value.value();
      has_legacy_notify_banner_types = true;
    }
  }

  const bool use_legacy_notify_allowlist = has_legacy_notify_banner_types && !has_explicit_notification_toggles;

  for (const auto& spec : notificationBoolConfigSpecs) {
    this->notifications.*(spec.member) = read_bool_config_entry(config, parsed, spec, false);
  }

  this->notifications.ClearToastStates();
  for (const auto& spec : notificationToggleSpecs) {
    const bool default_value     = use_legacy_notify_allowlist ? false : spec.default_value;
    bool       enabled_default   = default_value;
    const bool has_canonical_key = config_key_exists(config, spec.section, spec.key);
    const bool has_deprecated_key =
        !spec.deprecated_key.empty() && config_key_exists(config, spec.section, spec.deprecated_key);

    if (!use_legacy_notify_allowlist && !has_canonical_key && has_deprecated_key) {
      if (auto legacy_value = read_bool_config_value_if_present(config, spec.section, spec.deprecated_key, spec.docs);
          legacy_value.has_value()) {
        enabled_default = legacy_value.value();
      }
    }

    const bool enabled = read_bool_config_entry(config, parsed, spec.section, spec.key, spec.runtime_key,
                                                enabled_default, spec.docs, false);
    this->notifications.SetToastStateEnabled(spec.toast_state, enabled);

    if (has_deprecated_key && !has_canonical_key) {
      spdlog::warn("Deprecation Warning: [{}].{} is deprecated. Use {} instead.", spec.section, spec.deprecated_key,
                   spec.key);
    }
  }

  if (use_legacy_notify_allowlist) {
    if (!(notifications_table && notifications_table->contains("notifications_enabled"))) {
      this->notifications.enabled = true;
    }

    this->notifications.ClearToastStates();

    std::vector<std::string> notify_types = StrSplit(legacy_notify_banner_types, ',');
    const bool               legacy_notify_all_requested =
        std::ranges::any_of(notify_types, legacy_notification_allowlist_requests_all);
    if (legacy_notify_all_requested) {
      for (const auto& spec : notificationToggleSpecs) {
        this->notifications.SetToastStateEnabled(spec.toast_state, true);
      }
    } else {
      for (const auto& [key, value] : bannerTypes) {
        auto upper_key = AsciiStrToUpper(key);
        for (const std::string_view raw_type : notify_types) {
          auto stripped_type = StripLeadingAsciiWhitespace(raw_type);
          auto upper_type    = AsciiStrToUpper(stripped_type);
          if (upper_key == upper_type) {
            this->notifications.SetToastStateEnabled(value, true);
          }
        }
      }
    }

    spdlog::warn("Deprecation Warning: [ui].notify_on_banner_types / [ui].notify_banner_types is deprecated. Migrate "
                 "to [notifications].");
  } else if (has_legacy_notify_banner_types) {
    spdlog::warn(
        "Ignoring deprecated [ui] notification allowlist because explicit [notifications] toggles are present.");
  }

  this->notifications.incoming_attack_player  = this->notifications.EnabledForToastState(IncomingAttack);
  this->notifications.incoming_attack_hostile = this->notifications.EnabledForToastState(IncomingAttackFaction);
  notification_policy_load(config, parsed, this->notifications);
  spdlog::debug("");

  // if (this->enable_experimental) {
  //   parse_config_shortcut(config, parsed, "move_left",  GameFunction::MoveLeft,  DCSH::move_left);
  //   parse_config_shortcut(config, parsed, "move_right", GameFunction::MoveRight, DCSH::move_right);
  //   parse_config_shortcut(config, parsed, "move_down",  GameFunction::MoveDown,  DCSH::move_down);
  //   parse_config_shortcut(config, parsed, "move_up",    GameFunction::MoveUp,    DCSH::move_up);
  // }

  parse_disable_hotkeys_shortcut(config, parsed);
  parse_config_shortcut(config, parsed, "set_hotkeys_enable", GameFunction::EnableHotKeys, DCSH::set_hotkeys_enabled);

  parse_config_shortcut(config, parsed, "select_chatalliance", GameFunction::SelectChatAlliance,
                        DCSH::select_chatalliance);
  parse_config_shortcut(config, parsed, "select_chatglobal", GameFunction::SelectChatGlobal, DCSH::select_chatglobal);
  parse_config_shortcut(config, parsed, "select_chatprivate", GameFunction::SelectChatPrivate,
                        DCSH::select_chatprivate);
  parse_config_shortcut(config, parsed, "quit", GameFunction::Quit, DCSH::quit);

  parse_config_shortcut(config, parsed, "select_ship1", GameFunction::SelectShip1, DCSH::select_ship1);
  parse_config_shortcut(config, parsed, "select_ship2", GameFunction::SelectShip2, DCSH::select_ship2);
  parse_config_shortcut(config, parsed, "select_ship3", GameFunction::SelectShip3, DCSH::select_ship3);
  parse_config_shortcut(config, parsed, "select_ship4", GameFunction::SelectShip4, DCSH::select_ship4);
  parse_config_shortcut(config, parsed, "select_ship5", GameFunction::SelectShip5, DCSH::select_ship5);
  parse_config_shortcut(config, parsed, "select_ship6", GameFunction::SelectShip6, DCSH::select_ship6);
  parse_config_shortcut(config, parsed, "select_ship7", GameFunction::SelectShip7, DCSH::select_ship7);
  parse_config_shortcut(config, parsed, "select_ship8", GameFunction::SelectShip8, DCSH::select_ship8);
  parse_config_shortcut(config, parsed, "select_current", GameFunction::SelectCurrent, DCSH::select_current);

  parse_config_shortcut(config, parsed, "action_primary", GameFunction::ActionPrimary, DCSH::action_primary);
  parse_config_shortcut(config, parsed, "action_secondary", GameFunction::ActionSecondary, DCSH::action_secondary);
  parse_config_shortcut(config, parsed, "action_queue", GameFunction::ActionQueue, DCSH::action_queue);
  parse_config_shortcut(config, parsed, "action_queue_clear", GameFunction::ActionQueueClear, DCSH::action_queue_clear);
  parse_config_shortcut(config, parsed, "action_view", GameFunction::ActionView, DCSH::action_view);
  parse_config_shortcut(config, parsed, "action_recall", GameFunction::ActionRecall, DCSH::action_recall);
  parse_config_shortcut(config, parsed, "action_recall_cancel", GameFunction::ActionRecallCancel,
                        DCSH::action_recall_cancel);
  parse_config_shortcut(config, parsed, "action_repair", GameFunction::ActionRepair, DCSH::action_repair);
  parse_config_shortcut(config, parsed, "show_chat", GameFunction::ShowChat, DCSH::show_chat);
  parse_config_shortcut(config, parsed, "show_chatside1", GameFunction::ShowChatSide1, DCSH::show_chatside1);
  parse_config_shortcut(config, parsed, "show_chatside2", GameFunction::ShowChatSide2, DCSH::show_chatside2);
  parse_config_shortcut(config, parsed, "show_galaxy", GameFunction::ShowGalaxy, DCSH::show_galaxy);
  parse_config_shortcut(config, parsed, "show_system", GameFunction::ShowSystem, DCSH::show_system);
  parse_config_shortcut(config, parsed, "zoom_preset1", GameFunction::ZoomPreset1, DCSH::zoom_preset1);
  parse_config_shortcut(config, parsed, "zoom_preset2", GameFunction::ZoomPreset2, DCSH::zoom_preset2);
  parse_config_shortcut(config, parsed, "zoom_preset3", GameFunction::ZoomPreset3, DCSH::zoom_preset3);
  parse_config_shortcut(config, parsed, "zoom_preset4", GameFunction::ZoomPreset4, DCSH::zoom_preset4);
  parse_config_shortcut(config, parsed, "zoom_preset5", GameFunction::ZoomPreset5, DCSH::zoom_preset5);
  parse_config_shortcut(config, parsed, "zoom_in", GameFunction::ZoomIn, DCSH::zoom_in);
  parse_config_shortcut(config, parsed, "zoom_out", GameFunction::ZoomOut, DCSH::zoom_out);
  parse_config_shortcut(config, parsed, "zoom_max", GameFunction::ZoomMax, DCSH::zoom_max);
  parse_config_shortcut(config, parsed, "zoom_min", GameFunction::ZoomMin, DCSH::zoom_min);
  parse_config_shortcut(config, parsed, "zoom_reset", GameFunction::ZoomReset, DCSH::zoom_reset);
  parse_config_shortcut(config, parsed, "ui_scaleup", GameFunction::UiScaleUp, DCSH::ui_scaleup);
  parse_config_shortcut(config, parsed, "ui_scaledown", GameFunction::UiScaleDown, DCSH::ui_scaledown);
  parse_config_shortcut(config, parsed, "ui_scaleviewerup", GameFunction::UiViewerScaleUp, DCSH::ui_scaleviewerup);
  parse_config_shortcut(config, parsed, "ui_scaleviewerdown", GameFunction::UiViewerScaleDown,
                        DCSH::ui_scaleviewerdown);

  parse_config_shortcut(config, parsed, "log_debug", GameFunction::LogLevelDebug, DCSH::log_debug);
  parse_config_shortcut(config, parsed, "log_trace", GameFunction::LogLevelTrace, DCSH::log_trace);
  parse_config_shortcut(config, parsed, "log_info", GameFunction::LogLevelInfo, DCSH::log_info);
  parse_config_shortcut(config, parsed, "log_warn", GameFunction::LogLevelWarn, DCSH::log_warn);
  parse_config_shortcut(config, parsed, "log_error", GameFunction::LogLevelError, DCSH::log_error);
  parse_config_shortcut(config, parsed, "log_off", GameFunction::LogLevelOff, DCSH::log_off);

  parse_config_shortcut(config, parsed, "show_awayteam", GameFunction::ShowAwayTeam, DCSH::show_awayteam);
  parse_config_shortcut(config, parsed, "show_gifts", GameFunction::ShowGifts, DCSH::show_gifts);
  parse_config_shortcut(config, parsed, "show_artifacts", GameFunction::ShowArtifacts, DCSH::show_artifacts);
  parse_config_shortcut(config, parsed, "show_commander", GameFunction::ShowCommander, DCSH::show_commander);
  parse_config_shortcut(config, parsed, "show_daily", GameFunction::ShowDaily, DCSH::show_daily);
  parse_config_shortcut(config, parsed, "show_events", GameFunction::ShowEvents, DCSH::show_events);
  parse_config_shortcut(config, parsed, "show_exocomp", GameFunction::ShowExoComp, DCSH::show_exocomp);
  parse_config_shortcut(config, parsed, "show_factions", GameFunction::ShowFactions, DCSH::show_factions);
  parse_config_shortcut(config, parsed, "show_inventory", GameFunction::ShowInventory, DCSH::show_inventory);
  parse_config_shortcut(config, parsed, "show_missions", GameFunction::ShowMissions, DCSH::show_missions);
  parse_config_shortcut(config, parsed, "show_research", GameFunction::ShowResearch, DCSH::show_research);
  parse_config_shortcut(config, parsed, "show_scrapyard", GameFunction::ShowScrapYard, DCSH::show_scrapyard);
  parse_config_shortcut(config, parsed, "show_settings", GameFunction::ShowSettings, DCSH::show_settings);
  parse_config_shortcut(config, parsed, "show_officers", GameFunction::ShowOfficers, DCSH::show_officers);
  parse_config_shortcut(config, parsed, "show_qtrials", GameFunction::ShowQTrials, DCSH::show_qtrials);
  parse_config_shortcut(config, parsed, "show_refinery", GameFunction::ShowRefinery, DCSH::show_refinery);
  parse_config_shortcut(config, parsed, "show_ships", GameFunction::ShowShips, DCSH::show_ships);
  parse_config_shortcut(config, parsed, "show_stationexterior", GameFunction::ShoWStationExterior,
                        DCSH::show_stationexterior);
  parse_config_shortcut(config, parsed, "show_stationinterior", GameFunction::ShowStationInterior,
                        DCSH::show_stationinterior);
  parse_config_shortcut(config, parsed, "toggle_queue", GameFunction::ToggleQueue, DCSH::toggle_queue);

  if (this->hotkeys_extended) {
    parse_config_shortcut(config, parsed, "show_alliance", GameFunction::ShowAlliance, DCSH::show_alliance);

    if (this->enable_experimental) {
      parse_config_shortcut(config, parsed, "show_alliance_help", GameFunction::ShowAllianceHelp,
                            DCSH::show_alliance_help);
      parse_config_shortcut(config, parsed, "show_alliance_armada", GameFunction::ShowAllianceArmada,
                            DCSH::show_alliance_armada);
    }

    parse_config_shortcut(config, parsed, "show_bookmarks", GameFunction::ShowBookmarks, DCSH::show_bookmarks);

    if (this->enable_experimental) {
      parse_config_shortcut(config, parsed, "show_lookup", GameFunction::ShowLookup, DCSH::show_lookup);
    }

    parse_config_shortcut(config, parsed, "set_zoom_preset1", GameFunction::SetZoomPreset1, DCSH::set_zoom_preset1);
    parse_config_shortcut(config, parsed, "set_zoom_preset2", GameFunction::SetZoomPreset2, DCSH::set_zoom_preset2);
    parse_config_shortcut(config, parsed, "set_zoom_preset3", GameFunction::SetZoomPreset3, DCSH::set_zoom_preset3);
    parse_config_shortcut(config, parsed, "set_zoom_preset4", GameFunction::SetZoomPreset4, DCSH::set_zoom_preset4);
    parse_config_shortcut(config, parsed, "set_zoom_preset5", GameFunction::SetZoomPreset5, DCSH::set_zoom_preset5);
    parse_config_shortcut(config, parsed, "set_zoom_default", GameFunction::SetZoomDefault, DCSH::set_zoom_default);
    parse_config_shortcut(config, parsed, "toggle_preview_locate", GameFunction::TogglePreviewLocate,
                          DCSH::toggle_preview_locate);
    parse_config_shortcut(config, parsed, "toggle_preview_recall", GameFunction::TogglePreviewRecall,
                          DCSH::toggle_preview_recall);
    parse_config_shortcut(config, parsed, "toggle_cargo_default", GameFunction::ToggleCargoDefault,
                          DCSH::toggle_cargo_default);
    parse_config_shortcut(config, parsed, "toggle_cargo_player", GameFunction::ToggleCargoPlayer,
                          DCSH::toggle_cargo_player);
    parse_config_shortcut(config, parsed, "toggle_cargo_station", GameFunction::ToggleCargoStation,
                          DCSH::toggle_cargo_station);
    parse_config_shortcut(config, parsed, "toggle_cargo_hostile", GameFunction::ToggleCargoHostile,
                          DCSH::toggle_cargo_hostile);
    parse_config_shortcut(config, parsed, "toggle_cargo_armada", GameFunction::ToggleCargoArmada,
                          DCSH::toggle_cargo_armada);
  }

  spdlog::debug("");

  if (!std::filesystem::exists(File::MakePath(File::Config()))) {
    message.str("");
    message << "Creating " << File::Config() << " (default config file)";
    spdlog::warn(message.str());

    // Keep opt-in runtime diagnostics absent from fresh user-facing config, then restore their effective values for
    // the runtime vars snapshot.
    OmitOptInRuntimeDiagnosticsFromGeneratedUserConfig(parsed);
    notification_policy_prepare_generated_config(parsed);
    Config::Save(parsed, File::Config(), false);
    write_runtime_trace_config(parsed, g_runtime_trace_level, g_runtime_trace_track_overhead, g_mod_impact_monitor,
                               g_runtime_trace_report_interval_ms);
    config_schema::write_bool(parsed, "advanced.diagnostics.action_queue_guard_logging",
                              g_advanced_config.diagnostics.action_queue_guard_logging);
    notification_policy_write_runtime_snapshot(parsed);
  }

  const auto input_binding_bridge = input_binding::ResolveInputBindingConfig(config);
  input_binding::SetRuntimeBindingModel(input_binding::CompileBindingSet(input_binding_bridge.AsOverrides()));
  const auto& input_binding_compile = input_binding::RuntimeBindingModel();
  for (const auto& warning : input_binding_bridge.compatibility_warnings) {
    spdlog::warn("[InputBindings] {}", warning);
  }

  for (const auto& diagnostic : input_binding_compile.diagnostics) {
    if (diagnostic.message == "Binding conflict") {
      continue;
    }

    const auto* spec = input_binding::FindActionSpec(diagnostic.action);
    spdlog::warn("[InputBindings] Preview compile {} for {}: {}",
                 diagnostic.severity == input_binding::DiagnosticSeverity::Error ? "error" : "warning",
                 spec ? spec->canonical_key : std::string_view{"unknown"}, diagnostic.message);
  }

  for (const auto& conflict : input_binding_compile.conflicts) {
    const auto* action_a = input_binding::FindActionSpec(conflict.action_a);
    const auto* action_b = input_binding::FindActionSpec(conflict.action_b);
    spdlog::warn("[InputBindings] Preview conflict: {} conflicts with {} on '{}'",
                 action_a ? action_a->canonical_key : std::string_view{"unknown"},
                 action_b ? action_b->canonical_key : std::string_view{"unknown"}, conflict.chord.display);
  }

  auto input_binding_runtime =
      input_binding::BuildInputBindingRuntimeConfig(input_binding_bridge, input_binding_compile);
  parsed.emplace<toml::table>("input", toml::table());
  auto* parsed_input = parsed["input"].as_table();
  if (auto* bindings = input_binding_runtime["bindings"].as_table()) {
    parsed_input->insert_or_assign("bindings", std::move(*bindings));
  }
  if (auto* sources = input_binding_runtime["binding_sources"].as_table()) {
    parsed_input->insert_or_assign("binding_sources", std::move(*sources));
  }
  if (auto* warnings = input_binding_runtime["compatibility_warnings"].as_array()) {
    parsed_input->insert_or_assign("compatibility_warnings", std::move(*warnings));
  }
  if (auto* diagnostics = input_binding_runtime["binding_diagnostics"].as_array()) {
    parsed_input->insert_or_assign("binding_diagnostics", std::move(*diagnostics));
  }
  if (auto* conflicts = input_binding_runtime["binding_conflicts"].as_array()) {
    parsed_input->insert_or_assign("binding_conflicts", std::move(*conflicts));
  }

  message.str("");
  message << "Creating " << File::Vars() << " (final config file)";
  spdlog::info(message.str());

  if (std::filesystem::exists(FILE_DEF_PARSED)) {
    message.str("");
    message << "Removing " << FILE_DEF_PARSED << " (old parsed file)";
    spdlog::info(message.str());

    std::filesystem::remove(FILE_DEF_PARSED);
  }

  Config::Save(parsed, File::Vars());

  std::cout << "\n\n-----------------------------\n\n"
            << parsed << "\n\n-----------------------------\nVersion "

#if VERSION_PATCH
            << "Loaded beta version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << " (Patch "
            << VERSION_PATCH << ")\n\n"
            << "NOTE: Beta versions may have unexpected bugs and issues.\n\n"
#else
            << "Loaded beta version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION
            << " (Release)"
#endif

            << "\n\nPlease see https://github.com/netniv/stfc-mod for latest configuration help, examples and future "
               "releases\n"
            << "or visit the STFC Community Mod discord server at https://discord.gg/PrpHgs7Vjs\n\n";
}
