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
#include <vector>

#include <toml++/toml.h>

#if _WIN32
#include <Windows.h>
#endif

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
    Inventory,
    Jobs,
    Missions,
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
  bool verify_ssl = true;

  bool battlelogs = false;
  bool battlelogs_realtime = false;
  bool buffs      = false;
  bool buildings  = false;
  bool inventory  = false;
  bool jobs       = false;
  bool missions   = false;
  bool officer    = false;
  bool research   = false;
  bool resources  = false;
  bool ships      = false;
  bool slots      = false;
  bool tech       = false;
  bool traits     = false;

  /** @brief Check whether a given sync type is enabled on this config. */
  [[nodiscard]] bool enabled(Type type) const;
};

/// Master table mapping every SyncConfig::Type to its JSON/TOML keys and member pointer.
constexpr std::array SyncOptions{
    SyncConfig::Option{SyncConfig::Type::Battles, "battlelog", "battlelogs", &SyncConfig::battlelogs},
  SyncConfig::Option{SyncConfig::Type::BattlelogsRealtime, "battlelog_realtime", "battlelogs_realtime", &SyncConfig::battlelogs_realtime},
    SyncConfig::Option{SyncConfig::Type::Buffs, "buff", "buffs", &SyncConfig::buffs},
    SyncConfig::Option{SyncConfig::Type::Buildings, "module", "buildings", &SyncConfig::buildings},
    SyncConfig::Option{SyncConfig::Type::EmeraldChain, "emerald_chain", "buffs", &SyncConfig::buffs},
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
  for (const auto& opt : SyncOptions) {
    if (opt.type == type) {
      return std::string(opt.type_str);
    }
  }

  return {};
}

constexpr std::string operator+(const std::string& prefix, const SyncConfig::Type type)
{
  return prefix + to_string(type);
}

constexpr std::string operator+(const SyncConfig::Type type, const std::string& suffix)
{
  return to_string(type) + suffix;
}

/**
 * @brief A single sync target: base SyncConfig toggles plus endpoint credentials.
 *
 * Multiple targets can be defined under [sync.targets.<name>] in the TOML file,
 * each with its own URL, bearer token, and per-category overrides.
 */
class SyncTargetConfig : public SyncConfig
{
public:
  std::string url;   ///< Endpoint URL for this sync target.
  std::string token; ///< Bearer token / API key.
};

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

  bool enabled                  = false;
  bool incoming_attack_player       = false;
  bool incoming_attack_hostile      = false;
  bool fleet_arrived_in_system      = false;
  bool fleet_arrived_at_destination = false;
  bool fleet_started_mining         = false;
  bool fleet_node_depleted          = false;
  bool fleet_docked                 = false;
  bool fleet_repair_complete        = false;

  [[nodiscard]] bool AnyIncomingAttackEnabled() const
  {
    return incoming_attack_player || incoming_attack_hostile;
  }

  [[nodiscard]] bool IncomingAttackSplitEnabled() const
  {
    return incoming_attack_player != incoming_attack_hostile;
  }

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
  {
    toast_state_enabled.reset();
  }

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
  [[nodiscard]] static float   GetDPI();

  /** @brief Force a DPI re-read (e.g. after a display change). */
  static float                 RefreshDPI();

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
  void        Load();

  /** @brief Bump UI scale up or down by ui_scale_adjust, clamped to [0.1, 2.0]. */
  void        AdjustUiScale(bool scaleUp);

  /** @brief Bump object-viewer UI scale (finer step: ui_scale_adjust * 0.25). */
  void        AdjustUiViewerScale(bool scaleUp);

  // Disallow copying/moving to enforce singleton
  Config(const Config&)            = delete;
  Config& operator=(const Config&) = delete;
  Config(Config&&)                 = delete;
  Config& operator=(Config&&)      = delete;

  float ui_scale;
  float ui_scale_adjust;
  float ui_scale_viewer;
  float zoom;
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

  float system_zoom_preset_1;
  float system_zoom_preset_2;
  float system_zoom_preset_3;
  float system_zoom_preset_4;
  float system_zoom_preset_5;
  float transition_time;
  NotificationConfig notifications;

  bool             borderless_fullscreen;
  std::vector<int> disabled_banner_types;

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
  bool       sync_sidecar_jsonl;
  int        sync_resolver_cache_ttl;
  SyncConfig sync_options;

  std::map<std::string, SyncTargetConfig> sync_targets;

  // ─── Patch Toggles (debug builds only — release forces all true) ──────────
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
  bool installObjectTracker;
  bool installFleetArrivalHooks;

  std::string config_settings_url;
  std::string config_assets_url_override;

  // Loading Screen Background
  bool        loader_transition;
  bool        loader_enabled;
  std::string loader_image;

  bool installLoadingScreenBgHooks;
};

/**
 * @brief Whether unhandled key events pass through to the game's default input.
 *
 * Stored as a file-scope static in config.cc rather than a Config member to
 * avoid changing the struct layout, which can trigger LTO-related crashes.
 * @see fix/lto-and-sync-crashes
 */
bool AllowKeyFallthrough();

/**
 * @brief Whether the file-backed live debug channel is enabled.
 */
bool LiveDebugChannelEnabled();

/**
 * @brief Whether live battle_log decoding is enabled.
 */
bool BattleLogDecoderEnabled();

/**
 * @brief Whether decoded battle_log segment summaries should be emitted.
 */
bool BattleLogDecoderEmitSegments();

/**
 * @brief Whether sidecar-ready battle report feed events should be emitted.
 */
bool BattleLogDecoderEmitFeed();
