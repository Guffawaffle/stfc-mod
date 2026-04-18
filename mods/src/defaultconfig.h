/**
 * @file defaultconfig.h
 * @brief Compile-time default values for every TOML configuration key.
 *
 * Organised into namespaces that mirror the [section] structure of
 * community_patch_settings.toml.  Config::Load() uses these as fallbacks
 * when the user file is missing or a key is absent.
 */
#pragma once

namespace DefaultConfig
{
namespace Buffs
{
  /// Apply out-of-dock power buffs even when the ship is undocked.
  /// Default: true.
  constexpr bool use_out_of_dock_power = true;
} // namespace Buffs

namespace SystemConfig
{
  /// Override URL for downloading remote asset bundles (empty = use game default).
  constexpr const char* assets_url_override = "";
  /// URL for fetching remote settings updates (empty = disabled).
  constexpr const char* settings_url        = "";
} // namespace SystemConfig

namespace Control
{
  /// Enable experimental/unstable features (e.g. pan-momentum, WASD move keys). Default: false.
  constexpr bool enable_experimental = false;
  /// Master toggle for keyboard hotkeys. Default: true.
  constexpr bool hotkeys_enabled     = true;
  /// Enable the extended hotkey set (bookmarks, cargo toggles, etc.). Default: true.
  constexpr bool hotkeys_extended    = true;
  /// When true, use Scopely's built-in hotkey layer instead of the mod's. Default: false.
  constexpr bool use_scopely_hotkeys = false;
  /// Enable the action queue system. Default: true.
  constexpr bool queue_enabled       = true;
  /// Delay in ms before a tap is registered as a select action. Default: 500.
  constexpr auto select_timer        = 500;
} // namespace Control

namespace Graphics
{
  /// Start in borderless-fullscreen mode. Default: true.
  constexpr bool borderless_fullscreen       = true;
  /// Show the OS cursor instead of hiding it. Default: true.
  constexpr bool allow_cursor                = true;
  /// Default system-view zoom level (game units). Default: 1750.
  constexpr auto default_system_zoom         = 1750;
  /// Allow free window resizing (not locked to 16:9). Default: true.
  constexpr bool free_resize                 = true;
  /// Speed of keyboard-driven zoom (game units/s). Default: 350.
  constexpr auto keyboard_zoom_speed         = 350;
  /// Expose all display resolutions, not just 16:9. Default: false.
  constexpr bool show_all_resolutions        = false;
  /// Momentum decay rate for system-view panning (0–1, higher = slower decay). Default: 0.8.
  constexpr auto system_pan_momentum_falloff = 0.8;
  /// Initial pan-momentum strength (0–1). Default: 0.4. Requires enable_experimental.
  constexpr auto system_pan_momentum         = 0.4;
  /// Zoom preset levels 1–5 (game units, low = close, high = far). Defaults: 50–5000.
  constexpr auto system_zoom_preset_1        = 50;
  constexpr auto system_zoom_preset_2        = 500;
  constexpr auto system_zoom_preset_3        = 1250;
  constexpr auto system_zoom_preset_4        = 2750;
  constexpr auto system_zoom_preset_5        = 5000;
  /// Camera transition duration in seconds. Default: 0.01 (near-instant).
  constexpr auto transition_time             = 0.01;
  /// Base UI scale multiplier. Default: 0.6.
  constexpr auto ui_scale                    = 0.6;
  /// Step size for PgUp/PgDown UI scale adjustment. Default: 0.05.
  constexpr auto ui_scale_adjust             = 0.05;
  /// Scale multiplier for the object-viewer panel. Default: 1.2.
  constexpr auto ui_scale_viewer             = 1.2;
  /// Use zoom presets as the initial zoom on system entry. Default: true.
  constexpr bool use_presets_as_default      = true;
  /// Maximum camera zoom distance (game units). Default: 5000.
  constexpr auto zoom                        = 5000;
} // namespace Graphics

namespace Patches
{
  // Debug-build toggles for individual hook categories.
  // In release builds these are ignored and all patches are force-enabled.
  constexpr bool bufffixhooks               = true;  ///< Out-of-dock power / buff calculation fixes.
  constexpr bool chatpatches                = true;  ///< Chat-related UI patches.
  constexpr bool freeresizehooks            = true;  ///< Free window resize hooks.
  constexpr bool hotkeyhooks                = true;  ///< Keyboard hotkey injection.
  constexpr bool improveresponsivenesshooks = true;  ///< Input-responsiveness improvements.
  constexpr bool miscpatches                = true;  ///< Misc one-off fixes.
  constexpr bool objecttracker              = true;  ///< In-system object tracking overlay.
  constexpr bool fleetarrivalhooks          = true;  ///< Fleet arrival detection from the bottom fleet bar.
  constexpr bool panhooks                   = true;  ///< Pan-momentum hooks.
  constexpr bool resolutionlistfix          = true;  ///< Resolution-list population fix.
  constexpr bool syncpatches                = true;  ///< Data-sync network hooks.
  constexpr bool tempcrashfixes             = true;  ///< Temporary crash mitigations.
  constexpr bool testpatches                = true;  ///< Test / experimental patches.
  constexpr bool toastbannerhooks           = true;  ///< Toast-banner filtering hooks.
  constexpr bool uiscalehooks               = true;  ///< UI scale override hooks.
  constexpr bool zoomhooks                  = true;  ///< Camera zoom override hooks.
} // namespace Patches

/// Default key-binding strings for the [shortcuts] TOML section.
/// Format: \"MODIFIER-KEY\" or \"KEY1|KEY2\" for multi-bind.
/// \"NONE\" unbinds the action entirely.  See KEYMAPPING.md for full syntax.
namespace Shortcuts
{
  constexpr const char* toggle_queue          = "CTRL-Q";
  constexpr const char* action_queue          = "SPACE|MOUSE1";
  constexpr const char* action_queue_clear    = "CTRL-C";
  constexpr const char* action_primary        = "SPACE|MOUSE1";
  constexpr const char* action_recall         = "R|MOUSE3";
  constexpr const char* action_recall_cancel  = "SPACE|MOUSE1";
  constexpr const char* action_repair         = "R|MOUSE3";
  constexpr const char* action_secondary      = "TAB|MOUSE4";
  constexpr const char* action_view           = "V|MOUSE2";
  constexpr const char* set_hotkeys_disabled  = "CTRL-ALT-MINUS";
  constexpr const char* set_hotkeys_enabled   = "CTRL-ALT-=";
  constexpr const char* log_off               = "CTRL-SHIFT-F12";
  constexpr const char* log_error             = "CTRL-SHIFT-F11";
  constexpr const char* log_warn              = "CTRL-SHIFT-F10";
  constexpr const char* log_debug             = "CTRL-SHIFT-F9";
  constexpr const char* log_info              = "CTRL-SHIFT-F8";
  constexpr const char* log_trace             = "CTRL-SHIFT-F7";
  constexpr const char* quit                  = "F10";
  constexpr const char* select_chatalliance   = "CTRL-2";
  constexpr const char* select_chatglobal     = "CTRL-1";
  constexpr const char* select_chatprivate    = "CTRL-3";
  constexpr const char* select_current        = "CTRL-SPACE";
  constexpr const char* select_ship1          = "1";
  constexpr const char* select_ship2          = "2";
  constexpr const char* select_ship3          = "3";
  constexpr const char* select_ship4          = "4";
  constexpr const char* select_ship5          = "5";
  constexpr const char* select_ship6          = "6";
  constexpr const char* select_ship7          = "7";
  constexpr const char* select_ship8          = "8";
  constexpr const char* set_zoom_default      = "CTRL-=";
  constexpr const char* set_zoom_preset1      = "SHIFT-F1";
  constexpr const char* set_zoom_preset2      = "SHIFT-F2";
  constexpr const char* set_zoom_preset3      = "SHIFT-F3";
  constexpr const char* set_zoom_preset4      = "SHIFT-F4";
  constexpr const char* set_zoom_preset5      = "SHIFT-F5";
  constexpr const char* show_alliance         = "ALT-'";
  constexpr const char* show_alliance_armada  = "CTRL-'";
  constexpr const char* show_alliance_help    = "SHIFT-'";
  constexpr const char* show_artifacts        = "SHIFT-I";
  constexpr const char* show_awayteam         = "T";
  constexpr const char* show_bookmarks        = "B";
  constexpr const char* show_chat             = "C";
  constexpr const char* show_chatside1        = "ALT-C";
  constexpr const char* show_chatside2        = "`";
  constexpr const char* show_commander        = "O";
  constexpr const char* show_daily            = "Z";
  constexpr const char* show_events           = "SHIFT-E";
  constexpr const char* show_exocomp          = "X";
  constexpr const char* show_factions         = "F";
  constexpr const char* show_galaxy           = "G";
  constexpr const char* show_gifts            = "/";
  constexpr const char* show_inventory        = "I";
  constexpr const char* show_lookup           = "L";
  constexpr const char* show_missions         = "M";
  constexpr const char* show_officers         = "SHIFT-O";
  constexpr const char* show_qtrials          = "SHIFT-Q";
  constexpr const char* show_refinery         = "SHIFT-F";
  constexpr const char* show_research         = "U";
  constexpr const char* show_scrapyard        = "Y";
  constexpr const char* show_settings         = "SHIFT-S";
  constexpr const char* show_ships            = "N";
  constexpr const char* show_stationexterior  = "SHIFT-G";
  constexpr const char* show_stationinterior  = "SHIFT-H";
  constexpr const char* show_system           = "H";
  constexpr const char* toggle_cargo_armada   = "ALT-5";
  constexpr const char* toggle_cargo_default  = "ALT-1";
  constexpr const char* toggle_cargo_hostile  = "ALT-4";
  constexpr const char* toggle_cargo_player   = "ALT-2";
  constexpr const char* toggle_cargo_station  = "ALT-3";
  constexpr const char* toggle_preview_locate = "CTRL-R";
  constexpr const char* toggle_preview_recall = "CTRL-T";
  constexpr const char* ui_scaledown          = "PGDOWN";
  constexpr const char* ui_scaleup            = "PGUP";
  constexpr const char* ui_scaleviewerdown    = "SHIFT-PGDOWN";
  constexpr const char* ui_scaleviewerup      = "SHIFT-PGUP";
  constexpr const char* zoom_in               = "Q";
  constexpr const char* zoom_max              = "MINUS";
  constexpr const char* zoom_min              = "BACKSPACE";
  constexpr const char* zoom_out              = "E";
  constexpr const char* zoom_reset            = "=";
  constexpr const char* zoom_preset1          = "F1";
  constexpr const char* zoom_preset2          = "F2";
  constexpr const char* zoom_preset3          = "F3";
  constexpr const char* zoom_preset4          = "F4";
  constexpr const char* zoom_preset5          = "F5";
  constexpr const char* move_up               = "W";
  constexpr const char* move_down             = "S";
  constexpr const char* move_left             = "A";
  constexpr const char* move_right            = "D";
} // namespace Shortcuts

namespace Sync
{
  // Per-category defaults — each maps to a [sync] TOML key.
  // Individual [sync.targets.<name>] sections can override these.
  constexpr bool        battlelogs         = true;   ///< Sync battle-log reports.
  constexpr bool        buffs              = true;   ///< Sync buff / Emerald Chain data.
  constexpr bool        buildings          = true;   ///< Sync station module data.
  constexpr bool        inventory          = true;   ///< Sync inventory contents.
  constexpr bool        jobs               = true;   ///< Sync active job/build queues.
  constexpr bool        missions           = true;   ///< Sync mission progress.
  constexpr bool        officer            = true;   ///< Sync officer roster.
  constexpr const char* proxy              = "";     ///< HTTP proxy for sync requests (empty = none).
  constexpr bool        research           = true;   ///< Sync research tree state.
  constexpr bool        resources          = true;   ///< Sync resource amounts.
  constexpr bool        ships              = true;   ///< Sync fleet / ship data.
  constexpr bool        slots              = true;   ///< Sync crew-slot assignments.
  constexpr bool        tech               = true;   ///< Sync forbidden tech data.
  constexpr bool        traits             = true;   ///< Sync officer traits.
  constexpr const char* token              = "";     ///< Bearer token (legacy, prefer targets).
  constexpr const char* url                = "";     ///< Endpoint URL (legacy, prefer targets).
  constexpr bool        debug              = false;  ///< Extra debug logging for sync subsystem.
  constexpr bool        logging            = false;  ///< Log raw sync payloads.
  constexpr bool        verify_ssl         = true;   ///< Verify TLS certificates on sync requests.
  /// DNS resolver cache TTL in seconds. Default: 300 (5 min).
  constexpr auto        resolver_cache_ttl = 300;
} // namespace Sync

namespace UI
{
  /// Auto-skip the ship/officer reveal animation. Default: true.
  constexpr bool        always_skip_reveal_sequence = true;
  /// Auto-confirm new system discoveries. Default: true.
  constexpr bool        auto_confirm_discovery      = true;
  /// Block the Escape key from closing the game. Default: true.
  constexpr bool        disable_escape_exit         = true;
  /// Max ms between two Escape presses to count as a double-tap.
  /// 0 = disabled (Escape fully blocked). 500 = half-second window.
  constexpr auto        escape_exit_timer           = 0;
  /// Suppress the first-run welcome popup. Default: false.
  constexpr bool        disable_first_popup         = false;
  /// Hide galaxy chat entirely. Default: false.
  constexpr bool        disable_galaxy_chat         = false;
  /// Disable WASD movement keys. Default: false.
  constexpr bool        disable_move_keys           = false;
  /// Disable the "locate" preview on fleet-select. Default: false.
  constexpr bool        disable_preview_locate      = false;
  /// Disable the "recall" preview on fleet-select. Default: false.
  constexpr bool        disable_preview_recall      = false;
  /// Suppress all toast banners. Default: false.
  constexpr bool        disable_toast_banners       = false;
  /// Hide Veil-sector chat. Default: false.
  constexpr bool        disable_veil_chat           = false;
  /// Comma-separated list of toast banner type names to suppress (empty = none).
  constexpr const char* disabled_banner_types       = "";
  /// Comma-separated list of banner types that trigger a desktop notification (empty = none).
  constexpr const char* notify_banner_types         = "";
  /// Maximum alliance-donation slider value (percentage). Default: 80.
  constexpr auto        extend_donation_max         = 80;
  /// Enable the extended donation slider range. Default: true. (Windows only.)
  constexpr bool        extend_donation_slider      = true;
  /// Show cargo overlay on armada targets by default. Default: true.
  constexpr bool        show_armada_cargo           = true;
  /// Show cargo overlay on all entities by default. Default: true.
  constexpr bool        show_cargo_default          = true;
  /// Show cargo overlay on hostile ships. Default: true.
  constexpr bool        show_hostile_cargo          = true;
  /// Show cargo overlay on player ships. Default: true.
  constexpr bool        show_player_cargo           = true;
  /// Show cargo overlay on stations. Default: true.
  constexpr bool        show_station_cargo          = true;
} // namespace UI

} // namespace DefaultConfig
