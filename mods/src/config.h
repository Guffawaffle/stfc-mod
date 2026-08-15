/**
 * @file config.h
 * @brief TOML-based configuration system for the STFC Community Mod.
 *
 * Defines the Config singleton (runtime settings loaded from
 * community_patch_settings.toml) and the SyncConfig / SyncTargetConfig
 * structures that drive per-target data synchronisation.
 */
#pragma once

#include <array>
#include <bitset>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <toml++/toml.h>

#include "patches/hotkey_policy.h"

#if _WIN32
#include <Windows.h>
#endif

struct KirsharaQueueRepairConfig;

/**
 * @brief Per-target synchronisation toggles and proxy settings.
 *
 * Each boolean flag controls whether a specific data category is sent
 * to the sync backend.  SyncTargetConfig extends this with URL/token.
 */
class SyncConfig
{
public:
  /// Categories of game data that can be synced to an external service.
  enum class Type {
    Battles,
    BattlelogsRealtime,
    Buffs,
    Buildings,
    EmeraldChain,
    FleetRuntime,
    Inventory,
    Jobs,
    Missions,
    FleetAssignments,
    ModCapabilities,
    Officer,
    Research,
    Resources,
    Ships,
    Slots,
    Tech,
    Traits
  };

  /**
   * @brief Maps a sync type to its JSON key, TOML key, and member pointer.
   *
   * Used by SyncOptions[] to drive config loading and JSON serialisation
   * from a single table, avoiding per-type boilerplate.
   */
  struct Option {
    Type             type;
    std::string_view type_str;   ///< Key used in the outbound JSON body.
    std::string_view option_str; ///< Key used in the TOML config file.
    bool SyncConfig::* option;   ///< Pointer-to-member for the bool toggle.
  };

  std::string proxy;
  bool        verify_ssl                                      = true;
  bool        allow_unsafe_tls_without_certificate_validation = false;

  bool battlelogs          = false;
  bool battlelogs_realtime = false;
  bool buffs               = false;
  bool buildings           = false;
  bool fleet_runtime       = false;
  bool inventory           = false;
  bool jobs                = false;
  bool missions            = false;
  bool officer             = false;
  bool research            = false;
  bool resources           = false;
  bool ships               = false;
  bool slots               = false;
  bool tech                = false;
  bool traits              = false;

  /** @brief Check whether a given sync type is enabled on this config. */
  [[nodiscard]] bool enabled(Type type) const;
};

/// Master table mapping every SyncConfig::Type to its JSON/TOML keys and member pointer.
constexpr std::array SyncOptions{
    SyncConfig::Option{SyncConfig::Type::Battles, "battlelog", "battlelogs", &SyncConfig::battlelogs},
    SyncConfig::Option{SyncConfig::Type::BattlelogsRealtime, "battlelog_realtime", "battlelogs_realtime",
                       &SyncConfig::battlelogs_realtime},
    SyncConfig::Option{SyncConfig::Type::Buffs, "buff", "buffs", &SyncConfig::buffs},
    SyncConfig::Option{SyncConfig::Type::Buildings, "module", "buildings", &SyncConfig::buildings},
    SyncConfig::Option{SyncConfig::Type::EmeraldChain, "emerald_chain", "buffs", &SyncConfig::buffs},
    SyncConfig::Option{SyncConfig::Type::FleetRuntime, "fleet_runtime", "fleet_runtime", &SyncConfig::fleet_runtime},
    SyncConfig::Option{SyncConfig::Type::Inventory, "inventory", "inventory", &SyncConfig::inventory},
    SyncConfig::Option{SyncConfig::Type::Jobs, "job", "jobs", &SyncConfig::jobs},
    SyncConfig::Option{SyncConfig::Type::Missions, "mission", "missions", &SyncConfig::missions},
    SyncConfig::Option{SyncConfig::Type::Officer, "officer", "officer", &SyncConfig::officer},
    SyncConfig::Option{SyncConfig::Type::Research, "research", "research", &SyncConfig::research},
    SyncConfig::Option{SyncConfig::Type::Resources, "resource", "resources", &SyncConfig::resources},
    SyncConfig::Option{SyncConfig::Type::Ships, "ship", "ships", &SyncConfig::ships},
    SyncConfig::Option{SyncConfig::Type::Slots, "slot", "slots", &SyncConfig::slots},
    SyncConfig::Option{SyncConfig::Type::Tech, "ft", "tech", &SyncConfig::tech},
    SyncConfig::Option{SyncConfig::Type::Traits, "trait", "traits", &SyncConfig::traits},
};

constexpr std::string to_string(const SyncConfig::Type type)
{
  if (type == SyncConfig::Type::ModCapabilities) {
    return "mod_capabilities";
  }
  if (type == SyncConfig::Type::FleetAssignments) {
    return "fleet_assignments";
  }

  for (const auto& opt : SyncOptions) {
    if (opt.type == type) {
      return std::string(opt.type_str);
    }
  }

  return {};
}

constexpr std::string operator+(const std::string& prefix, const SyncConfig::Type type)
{ return prefix + to_string(type); }

constexpr std::string operator+(const SyncConfig::Type type, const std::string& suffix)
{ return to_string(type) + suffix; }

/**
 * @brief A single sync target: base SyncConfig toggles plus endpoint credentials.
 *
 * Multiple targets can be defined under [sync.targets.<name>] in the TOML file,
 * each with its own URL, bearer token, and per-category overrides.
 */
class SyncTargetConfig : public SyncConfig
{
public:
  enum class Mode {
    Legacy,
    Majel,
  };

  std::string url;                 ///< Endpoint URL for this sync target.
  std::string token;               ///< Bearer token / API key.
  Mode        mode = Mode::Legacy; ///< Outbound wire contract for this target.
};

enum class MissionHudVisibility {
  Auto,
  Always,
  Never,
};

/**
 * @brief Canonical local sidecar delivery settings from `[sidecar.sync]`.
 *
 * These values are parsed independently from `[sync.targets.*]` so local
 * sidecar routing can move off the external/community sync surface.
 */
struct SidecarSyncConfig {
  bool        enabled   = false;
  std::string transport = "legacy_http";
  std::string pipe_name;
  std::string url;
  std::string token;
  std::string proxy;
  bool        verify_ssl                                      = true;
  bool        allow_unsafe_tls_without_certificate_validation = false;
  bool        battlelogs_realtime                             = false;
  bool        battlelog_enrichment                            = false;
  bool        fleet_runtime                                   = false;
};

/**
 * @brief Deprecated legacy aliases for reserved observability probe toggles.
 *
 * Canonical ownership now lives under `[advanced.diagnostics]`, but these
 * members remain in the runtime model for low-risk compatibility.
 */
struct SidecarProbesConfig {
  bool ship_identity      = false;
  bool battle_log_decoder = false;
  bool battle_catalog     = false;
};

/**
 * @brief Local sidecar logging settings from `[sidecar.logging]`.
 */
struct SidecarLoggingConfig {
  bool jsonl                = false;
  int  jsonl_replay_seconds = 30;
  int  jsonl_recent_logs    = 300;
};

/**
 * @brief Deprecated legacy aliases for reserved diagnostics toggles.
 *
 * Canonical ownership now lives under `[advanced.diagnostics]`, but these
 * members remain in the runtime model for low-risk compatibility.
 */
struct SidecarDiagnosticsConfig {
  bool reserved_native_debug           = false;
  bool reserved_native_payload_logging = false;
};

/**
 * @brief Unified sidecar-native config surface rooted at `[sidecar]`.
 */
struct SidecarConfig {
  SidecarSyncConfig        sync;
  SidecarProbesConfig      probes;
  SidecarLoggingConfig     logging;
  SidecarDiagnosticsConfig diagnostics;
};

/**
 * @brief Canonical native diagnostics surface rooted at `[advanced.diagnostics]`.
 *
 * Targeted concerns, refinery diagnostics, and live query live here. The
 * remaining keys stay dormant/reserved until later diagnostic families
 * migrate off `[debug]`.
 */
struct AdvancedDiagnosticsConfig {
  struct ConcernsConfig {
    std::vector<std::string> enabled;
  };

  struct FilesConfig {
    std::string root                      = "";
    int         navhook_trace_max_kb      = 4096;
    int         navhook_trace_files       = 3;
    int         action_queue_probe_max_kb = 8192;
    int         action_queue_probe_files  = 3;
  };

  bool        ship_identity                    = false;
  bool        battle_log_decoder               = false;
  bool        battle_catalog                   = false;
  bool        reserved_native_debug            = false;
  bool        reserved_native_payload_logging  = false;
  bool        hotkey_suppression_logging       = false;
  bool        notification_skip_logging        = false;
  bool        fleet_selection_timing_logging   = false;
  bool        live_query                       = false;
  bool        action_queue_guard_logging       = false;
  bool        refinery_diagnostics             = false;
  ConcernsConfig concerns;
  FilesConfig files;
};

/**
 * @brief Queue experiment/dev-test namespace rooted at `[advanced.queue]`.
 */
struct AdvancedQueueConfig {
  bool queue_repair_enabled     = false;
  bool thin_queue_protection    = true;
  bool queue_add_direct_handler = false;
};

/**
 * @brief Unified advanced-native config surface rooted at `[advanced]`.
 */
struct AdvancedConfig {
  AdvancedDiagnosticsConfig diagnostics;
  AdvancedQueueConfig       queue;
};

constexpr std::string_view to_string(const SyncTargetConfig::Mode mode)
{
  switch (mode) {
    case SyncTargetConfig::Mode::Legacy:
      return "legacy";
    case SyncTargetConfig::Mode::Majel:
      return "majel";
  }

  return "legacy";
}

/**
 * @brief Unified OS-notification selection model.
 *
 * The `[notifications]` TOML section drives this model. Toast-backed
 * notifications are tracked by Toast::State enum value, while non-toast events
 * (such as fleet-derived notifications) get their own explicit toggles.
 */
class NotificationConfig
{
public:
  static constexpr size_t MaxToastStates = 64;

  bool enabled                       = true;
  bool audio_enabled                 = true;
  bool audio_fleet_arrived_in_system = false;
  bool incoming_attack_player        = false;
  bool incoming_attack_hostile       = false;
  bool fleet_arrived_in_system       = false;
  bool fleet_arrived_at_destination  = false;
  bool fleet_started_mining          = false;
  bool fleet_node_depleted           = false;
  bool fleet_docked                  = false;
  bool fleet_repair_complete         = false;

  [[nodiscard]] bool AnyIncomingAttackEnabled() const
  { return incoming_attack_player || incoming_attack_hostile; }

  [[nodiscard]] bool IncomingAttackSplitEnabled() const
  { return incoming_attack_player != incoming_attack_hostile; }

  [[nodiscard]] bool EnabledForToastState(int state) const
  {
    if (state < 0 || static_cast<size_t>(state) >= toast_state_enabled.size()) {
      return false;
    }

    return toast_state_enabled.test(static_cast<size_t>(state));
  }

  void SetToastStateEnabled(int state, bool isEnabled)
  {
    if (state < 0 || static_cast<size_t>(state) >= toast_state_enabled.size()) {
      return;
    }

    toast_state_enabled.set(static_cast<size_t>(state), isEnabled);
  }

  void ClearToastStates()
  { toast_state_enabled.reset(); }

private:
  std::bitset<MaxToastStates> toast_state_enabled{};
};

/**
 * @brief Singleton holding all runtime configuration for the Community Mod.
 *
 * Constructed once via Config::Get().  The constructor calls Load(), which
 * parses the user's TOML file (falling back to DefaultConfig values),
 * writes a merged "runtime vars" snapshot, and populates every member.
 */
class Config final
{
public:
  Config();

  /** @brief Access the process-wide singleton. */
  [[nodiscard]] static Config& Get();

  /** @brief Current monitor DPI scale factor (cached per monitor change). */
  [[nodiscard]] static float GetDPI();

  /** @brief Force a DPI re-read (e.g. after a display change). */
  static float RefreshDPI();

#ifdef _WIN32
  [[nodiscard]] static HWND WindowHandle();
#endif

  /**
   * @brief Serialise a toml::table to disk.
   * @param config    The table to write.
   * @param filename  Target filename (resolved via File::MakePath).
   * @param apply_warning  If true, prepend a "this is not the config file" header.
   */
  static void Save(const toml::table& config, std::string_view filename, bool apply_warning = true);

  /** @brief Parse the user TOML and populate all members. Called by the constructor. */
  void Load();

  /** @brief Bump UI scale up or down by ui_scale_adjust, clamped to [0.1, 2.0]. */
  void AdjustUiScale(bool scaleUp);

  /** @brief Bump object-viewer UI scale (finer step: ui_scale_adjust * 0.25). */
  void AdjustUiViewerScale(bool scaleUp);

  // Disallow copying/moving to enforce singleton
  Config(const Config&)            = delete;
  Config& operator=(const Config&) = delete;
  Config(Config&&)                 = delete;
  Config& operator=(Config&&)      = delete;

  float ui_scale;
  float ui_scale_adjust;
  float ui_scale_viewer;
  float zoom;
  float fr_scale;
  bool  allow_cursor;
  bool  free_resize;
  bool  adjust_scale_res;
  bool  show_all_resolutions;

  bool  use_out_of_dock_power;
  float system_pan_momentum;
  float system_pan_momentum_falloff;

  float keyboard_zoom_speed;
  int   select_timer;

  bool  queue_enabled;
  bool  hotkeys_enabled;
  bool  hotkeys_extended;
  bool  use_scopely_hotkeys;
  bool  use_presets_as_default;
  bool  enable_experimental;
  float default_system_zoom;

  float              system_zoom_preset_1;
  float              system_zoom_preset_2;
  float              system_zoom_preset_3;
  float              system_zoom_preset_4;
  float              system_zoom_preset_5;
  float              transition_time;
  NotificationConfig notifications;

  bool             borderless_fullscreen;
  std::vector<int> disabled_banner_types;

  int  extend_chest_purchase_max;
  int  extend_donation_max;
  bool extend_donation_slider;
  bool disable_move_keys;
  bool disable_preview_locate;
  bool disable_preview_recall;
  bool disable_escape_exit;
  int  escape_exit_timer;
  bool disable_galaxy_chat;
  bool disable_veil_chat;
  bool disable_first_popup;
  bool disable_toast_banners;

  bool show_cargo_default;
  bool show_player_cargo;
  bool show_station_cargo;
  bool show_hostile_cargo;
  bool show_armada_cargo;

  bool always_skip_reveal_sequence;

  bool       sync_logging;
  bool       sync_debug;
  bool       sidecar_logging_jsonl;
  int        sync_resolver_cache_ttl;
  SyncConfig sync_options;

  std::map<std::string, SyncTargetConfig> sync_targets;

  // ─── Patch Toggles (honored in every build mode) ─────────────────────────
  bool installUiScaleHooks;
  bool installZoomHooks;
  bool installBuffFixHooks;
  bool installToastBannerHooks;
  bool installPanHooks;
  bool installImproveResponsivenessHooks;
  bool installHotkeyHooks;
  bool installFreeResizeHooks;
  bool installTempCrashFixes;
  bool installTestPatches;
  bool installMiscPatches;
  bool installChatPatches;
  bool installResolutionListFix;
  bool installSyncPatches;
  bool installGameVersionHook;
  bool installObjectTracker;
  bool installFleetArrivalHooks;
  bool installRepairActionInterlock;

  std::string config_settings_url;
  std::string config_assets_url_override;

  // Loading Screen / Transition Screen
  bool        loader_enabled;
  bool        loader_transition;
  bool        loader_transition_black;
  std::string loader_image;
  float       loader_logo_scale;
  bool        loader_tip_enabled;

  bool installLoadingScreenHooks;
  bool installTransitionScreenHooks;
};

/**
 * @brief Whether unhandled key events pass through to the game's default input.
 *
 * This legacy setting is stored at file scope and exposed through a read-only accessor.
 */
bool AllowKeyFallthrough();

/**
 * @brief Resolved Scopely shortcut initialization policy.
 */
ScopelyShortcutPolicy ScopelyShortcutsPolicy();

/**
 * @brief Resolved per-frame original ScreenManager::Update policy.
 */
OriginalFramePolicy OriginalFramePolicySetting();

/**
 * @brief Whether the file-backed live debug channel is enabled.
 */
bool LiveDebugChannelEnabled();

/**
 * @brief Whether the queue repair/probe experiment layer is allowed to activate.
 */
bool QueueRepairEnabled();

/**
 * @brief Whether the focused Kir'shara queued-combat advancement repair is enabled.
 */
bool KirsharaQueueRepairEnabled();

/**
 * @brief Canonical `[advanced.kirshara_queue]` settings resolved during config load.
 */
const KirsharaQueueRepairConfig& KirsharaQueueRepairSettings();

/**
 * @brief Whether queue-add should use the widget's direct click handler instead of the generic button press path.

 */
bool QueueAddDirectHandlerEnabled();

/**
 * @brief Whether live battle_log decoding is enabled.
 */
bool BattleLogDecoderEnabled();

/**
 * @brief Canonical `[sidecar]` settings resolved during config load.
 */
const SidecarConfig& SidecarSettings();

/**
 * @brief Canonical `[advanced]` settings resolved during config load.
 */
const AdvancedConfig& AdvancedSettings();

/**
 * @brief Canonical local sidecar delivery settings from `[sidecar.sync]`.
 */
const SidecarSyncConfig& SidecarSyncSettings();

/**
 * @brief Deprecated legacy observability probe aliases from `[sidecar.probes]`.
 */
const SidecarProbesConfig& SidecarProbesSettings();

/**
 * @brief Local sidecar logging settings from `[sidecar.logging]`.
 */
const SidecarLoggingConfig& SidecarLoggingSettings();

/**
 * @brief Deprecated legacy observability aliases from `[sidecar.diagnostics]`.
 */
const SidecarDiagnosticsConfig& SidecarDiagnosticsSettings();

/**
 * @brief Canonical reserved observability toggles from `[advanced.diagnostics]`.
 */
const AdvancedDiagnosticsConfig& AdvancedDiagnosticsSettings();

/**
 * @brief Native diagnostics file policy from `[advanced.diagnostics.files]`.
 */
const AdvancedDiagnosticsConfig::FilesConfig& AdvancedDiagnosticsFileSettings();

/**
 * @brief Canonical queue experiment/dev-test namespace from `[advanced.queue]`.
 */
const AdvancedQueueConfig& AdvancedQueueSettings();

/**
 * @brief Whether decoded battle_log segment summaries should be emitted.
 */
bool BattleLogDecoderEmitSegments();

/**
 * @brief Whether sidecar-ready battle report feed events should be emitted.
 */
bool BattleLogDecoderEmitFeed();

/**
 * @brief Seconds retained in the optional local sidecar JSONL evidence window.
 */
int SidecarLoggingJsonlReplaySeconds();

/**
 * @brief Number of recent battle-log groups retained in optional local sidecar JSONL evidence.
 */
int SidecarLoggingJsonlRecentLogs();

/**
 * @brief Whether focused refinery diagnostics should be installed.
 */
bool RefineryDiagnosticsEnabled();

/**
 * @brief Whether the Gifts view should open its existing bulk-claim flyout automatically.
 */
bool AutoOpenBulkClaimGiftsEnabled();

/**
 * @brief Whether the removed Below Deck Ability Manage Ship assignment sort should be restored.
 */
bool OfficerBelowDeckAssignmentSortEnabled();

/**
 * @brief Visibility override for one `[ui.mission_hud]` button.
 */
MissionHudVisibility MissionHudButtonVisibility(std::string_view button_name);

/**
 * @brief Whether mission HUD visibility has any non-auto button overrides.
 */
bool MissionHudTweaksEnabled();
