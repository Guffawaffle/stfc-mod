# Community Mod feature catalog

Generated from [the functional inventory](play-functional-parity.json) by
`python scripts/render-feature-catalog.py`. Edit the inventory, then regenerate;
`--check` detects a stale view. This is an incremental product catalog, not a
claim that every branch or workflow has been inspected or verified.

Branch presence refers to the exact **local revision inspected**. Consumer-only
means the TOML/gameplay path exists but the named in-game control does not.
Not assessed never means absent. Source presence, automated checks, builds,
and player-confirmed behavior are separate evidence.

## Features examined through the migration procedure

| ID / feature | Player entry point | Upstream dev | play | play-dev | Refactor | Current qualification |
| --- | --- | --- | --- | --- | --- | --- |
| [PLAY-OFFICER-SORT — Below Deck Ability sort](features/PLAY-OFFICER-SORT.md) | Officer roster and assignment sort menus | present @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `e979f692` | Native BDA reported available; compatibility module retained; testing held |
| [PLAY-OFFICER-PRESETS — Preset ordering and canonical native save](features/PLAY-OFFICER-PRESETS.md) | Officer Presets; Shift/Ctrl/Command name edit | absent @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `a0565d7e` | Basic Windows preset workflow accepted after initialization-order fix; macOS and native crew create/replace/delete remain unqualified |
| [PLAY-PINS-ORDER — Stable-ID ship pins and legacy-name fallback](features/PLAY-PINS-ORDER.md) | Swap Ship pin order | partial @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `e979f692` | Source-preserved; partial pin trial; further testing held |
| [PLAY-PINS-INPUT — Badge double-click and hold-drag pin gestures](features/PLAY-PINS-INPUT.md) | Swap Ship badge double-click and held drag | absent @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `e979f692` | Partial player acceptance; pinned-group visual issue open; testing held |
| [PLAY-DOCK-ASSIGN — Double-click and Enter assignment](features/PLAY-DOCK-ASSIGN.md) | Swap Ship; Mod Settings → Officers & Ships | present @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `8aa5f2ac` | Windows basic workflow confirmed; edge/custody limits retained |
| [SET-AUDIO-PICKER — Audio pages, custom file picker and preview](features/SET-AUDIO-PICKER.md) | Mod Settings → Audio Alerts → alert page | absent @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `8aa5f2ac` | Windows basic acceptance reported; detailed edge cases and macOS pending |
| [SET-IMAGE-PICKER — Loading-image picker and exact entry](features/SET-IMAGE-PICKER.md) | Mod Settings → Loading Screens | consumer-only @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `8aa5f2ac` | Windows basic workflow confirmed; edge/custody limits retained |
| [SET-WARP-FILTERS — Per-ship jump, warp and always-ask editors](features/SET-WARP-FILTERS.md) | Mod Settings → Map & Travel → Ship route filters | consumer-only @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `8aa5f2ac` | Windows basic acceptance reported; detailed edge cases and macOS pending |
| [SET-DAILY-FACTIONS — Daily bulk-claim faction selector](features/SET-DAILY-FACTIONS.md) | Mod Settings → Claims & Confirmations | consumer-only @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `8aa5f2ac` | Windows basic acceptance reported; detailed edge cases and macOS pending |
| [SET-BANNER-FILTERS — Hidden banners and desktop notification filter editors](features/SET-BANNER-FILTERS.md) | Mod Settings → Notifications | consumer-only @ `6ceb892b` | present @ `cad98ef7` | present @ `1baa55fe` | present @ `8aa5f2ac` | Windows basic acceptance reported; detailed edge cases and macOS pending |
| [PLAY-AAI — Native Battle Analysis, Crew & Effects and Tactical Console host](features/PLAY-AAI.md) | Native battle summary -> Battle Analysis; Mod Settings -> Interface & Scale | absent @ `6ceb892b` | absent @ `cad98ef7` | present @ `1baa55fe` | partial @ `f0ad3838` | Native host implemented; all42 analytical fixtures and96 retained-report comparisons pass; in-game qualification pending |

Full pinned paths, symbols, differences and history are in each inventory entry.
Frozen play-dev remains `1baa55fedf1758b19f843cc59707b8901a0b4998`.

## Other recorded capabilities

These include the earlier PLAY reconciliation inventory. Their historical
dispositions are retained; they have **not** all been rescanned against frozen
play-dev or the current refactor. Newly discovered entries remain visible even
when their migration is deferred.

| ID | Capability | Recorded disposition | Source evidence |
| --- | --- | --- | --- |
| PLAY-FLEET-SELECT | Fleet shortcut selection and repeated locate | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-FLEET-TOW | Discovery shift-ship towing | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-FLEET-ACTIONS | Contextual movement, attack, repair, recall and queue actions | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-FLEET-PREVIEW-GATES | Preview, locate, recall and Escape precedence | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-FLEET-ABILITY-RECOVERY | Expired ability button recovery | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-QUEUE-SWITCH | Queue enable and persisted toggle | Implemented, unverified | mods/src/patches/parts/testing.cc |
| PLAY-QUEUE-RECOVERY | Vanished-target course recovery | Partial | mods/src/patches/parts/action_queue.cc |
| PLAY-QUEUE-THIN | Native prefix-prune recovery | Partial | mods/src/patches/parts/thin_queue_protection.cc |
| PLAY-QUEUE-ADDRESS | Conservative deployed-address substitution | Implemented, unverified | mods/src/patches/parts/queue_address_guard.cc |
| PLAY-CARGO-PREVIEW | Category defaults and delayed cargo opening | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-MINING-CYCLE | Mining node, occupant and cargo cycle | Implemented, unverified | mods/src/patches/parts/mining_preview.cc |
| PLAY-CARGO-FORMAT | Cargo abbreviation precision | Implemented, unverified | mods/src/patches/parts/cargo_format.cc |
| PLAY-CARGO-INSTANT | Instant cargo counter with native flag restoration | Implemented, unverified | mods/src/patches/parts/instant_cargo.cc |
| PLAY-DOCK-SYNC | Management ship selects actual dock slot | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-SCREEN-ACTIONS | Ship/officer/preset navigation and deferred buttons | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-FOCUS-SEARCH | Search focus and legacy cross-section setting alias | Implemented, unverified | mods/src/patches/parts/focus_search.cc |
| PLAY-WARP-AUTO | Per-ship jump/warp/ask precedence | Implemented, unverified | mods/src/patches/parts/instant_warp_confirmation.cc |
| PLAY-WARP-MODIFIER | Ctrl/Command-click manual warp/jump choice | Implemented, unverified | mods/src/patches/parts/instant_warp_confirmation.cc |
| PLAY-WARP-LABEL | WARP/JUMP visit-label preview | Implemented, unverified | mods/src/patches/parts/warp_action_label.cc |
| PLAY-WARP-COURSE | Retained hidden Galaxy course action | Implemented, unverified | mods/src/patches/planned_system_warp.cc |
| PLAY-SHIP-TECH-ART | Exact-ship FT/CT indicators | Implemented, unverified | mods/src/patches/parts/ship_tech_indicators.cc |
| PLAY-FLEET-BADGES | Configured shortcut on own system fleet labels | Implemented, unverified | mods/src/patches/parts/ship_shortcut_badges.cc |
| PLAY-FLEET-LABELS | Player/nonplayer label detail and thresholds | Implemented, unverified | mods/src/patches/parts/zoom.cc |
| PLAY-FLEET-OPC | OPC highlighting and mining ETA | Implemented, unverified | mods/src/patches/parts/opc_indicators.cc |
| PLAY-CAMERA-ZOOM | Extended system zoom and clipping | Implemented, unverified | mods/src/patches/parts/zoom.cc |
| PLAY-CAMERA-PRESETS | Stored/jumped zoom presets and defaults | Implemented, unverified | mods/src/patches/parts/zoom.cc |
| PLAY-CAMERA-KEYBOARD | Keyboard zoom at pointer | Implemented, unverified | mods/src/patches/parts/zoom.cc |
| PLAY-CAMERA-WHEEL | UI wheel ownership with native pinch intact | Implemented, unverified | mods/src/patches/parts/zoom.cc |
| PLAY-CAMERA-BACKDROP | Planet/backdrop scale without compounding | Implemented, unverified | mods/src/patches/parts/zoom.cc |
| PLAY-CAMERA-PAN | System pan and glide with modal gates | Implemented, unverified | mods/src/patches/parts/fix_pan.cc |
| PLAY-CAMERA-STATION | Exterior rotation and interior zoom restoration | Implemented, unverified | mods/src/patches/parts/interior_zoom.cc |
| PLAY-SYSTEM-ORBIT | System orbit/free pan/environment artwork | Intentionally excluded | mods/src/patches/parts/navigation_orbit.cc |
| PLAY-GALAXY-SELECTION | Far-tier system tapping | Partial | mods/src/patches/parts/galaxy_selection.cc |
| PLAY-GALAXY-DETAIL | Independent major/minor label and culling profiles | Partial | mods/src/patches/parts/galaxy_labels.cc |
| PLAY-GALAXY-OVERLAYS | Multiple simultaneous overlays and native selection state | Partial | mods/src/patches/parts/galaxy_labels.cc |
| PLAY-GALAXY-HOUSING | Data-backed station housing marker | Partial | mods/src/patches/parts/galaxy_housing.cc |
| PLAY-HAVEN-ZOOM | Qualified planetary zoom | Implemented, unverified | mods/src/patches/parts/haven_zoom.cc |
| PLAY-HAVEN-VISUALS | Reversible road depth and water visibility | Implemented, unverified | mods/src/patches/parts/haven_road_depth.cc |
| PLAY-HAVEN-ORBIT | Haven right-drag and modifier reset | Implemented, unverified | mods/src/patches/parts/haven_zoom.cc |
| PLAY-HAVEN-HISTORY | Reverse help history with managed custody | Implemented, unverified | mods/src/patches/parts/haven_history.cc |
| PLAY-CHAT-CHANNELS | Galaxy/Veil tabs and preview filtering | Implemented, unverified | mods/src/patches/parts/chat.cc |
| PLAY-CHAT-AVATARS | Portrait retention during pooled chat binding | Implemented, unverified | mods/src/patches/parts/chat_avatar_retention.cc |
| PLAY-CHAT-TITLES | Hide titles only in chat | Implemented, unverified | mods/src/patches/parts/chat_titles.cc |
| PLAY-CHAT-NAVIGATION | Full/side chat, focus and channel shortcuts | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-ART-LOGIN | Custom/fallback login image and logos | Implemented, unverified | mods/src/patches/parts/loading_screen.cc |
| PLAY-ART-TRANSITION | Transition artwork, black mode and tips | Implemented, unverified | mods/src/patches/parts/transition_screen.cc |
| PLAY-AUDIO-EVENTS | Fabric named/all audio filters and tracing | Implemented, unverified | mods/src/patches/parts/audio_events.cc |
| PLAY-AUDIO-CUES | Built-in/custom audio and coalescing | Implemented, unverified | mods/src/patches/notification_audio.cc |
| PLAY-NOTIFY-BANNERS | Independent banner suppression and readable desktop forwarding | Implemented, unverified | mods/src/patches/notification_service.cc |
| PLAY-NOTIFY-MAC | Permission-aware macOS desktop alerts | Implemented, unverified | mods/src/notification_desktop_mac.mm |
| PLAY-NOTIFY-FLEET | Seven fleet-event producers and alerts | Implemented, unverified | mods/src/patches/parts/fleet_notifications.cc |
| PLAY-NOTIFY-INCOMING | Own-ship incoming player alert | Implemented, unverified | mods/src/patches/incoming_player_attack.cc |
| PLAY-SHORTCUT-HINTS | Native hint replacement and visibility toggle | Implemented, unverified | mods/src/patches/parts/hotkeys.cc |
| PLAY-WINDOW | Windows free resizing | Implemented, unverified | mods/src/patches/parts/free_resize.cc |
| PLAY-CURSOR | Plain cursor and clipping release | Implemented, unverified | mods/src/patches/parts/testing.cc |
| PLAY-GAME-CONFIG | Settings/assets endpoint overrides | Implemented, unverified | mods/src/patches/parts/testing.cc |
| PLAY-DONATION | Donation slider extension | Partial | mods/src/patches/parts/misc.cc |
| PLAY-CHEST-LIMIT | Chest purchase cap bounded at 160 | Implemented, unverified | mods/src/patches/parts/misc.cc |
| PLAY-OFFER-DETAILS | Cooldown offer opens auxiliary view | Implemented, unverified | mods/src/patches/parts/misc.cc |
| PLAY-POPUPS | Shared first-popup-only session and reveal skip | Implemented, unverified | mods/src/patches/parts/misc.cc |
| PLAY-BUFF-GUARD | Null buff-list crash guard | Implemented, unverified | mods/src/patches/parts/misc.cc |
| PLAY-CLAIM | Bulk gift flyout and daily faction claiming | Implemented, unverified | mods/src/patches/parts/daily_faction_bulk_claim.cc |
| PLAY-BASE-CARD | Starbase level/name card presentation | Implemented, unverified | mods/src/patches/parts/base_card.cc |
| PLAY-HUD | Four mission HUD visibility choices | Implemented, unverified | mods/src/patches/parts/mission_hud_tweaks.cc |
| PLAY-FT-SHIP-NAME | Equipped ship under Forbidden Tech header | Implemented, unverified | mods/src/patches/parts/ship_tech_indicators.cc |
| PLAY-CONFIRMATIONS | FT, alliance and reveal confirmation choices | Implemented, unverified | mods/src/patches/parts/forbidden_tech_confirmation.cc |
| PLAY-ANOMALY | Galactic anomaly timer | Implemented, unverified | mods/src/patches/parts/galactic_anomaly_timer.cc |
| SET-NAV | Mod pages and native back/navigation | Partial | mods/src/settings/native/page_navigation.cc |
| SET-VALUES | Boolean, exact numeric, choices and slider controls | Partial | mods/src/settings/native/value_widgets.cc |
| SET-FC-CONFIRM | Restore FC confirmation after do-not-show | Implemented, unverified | mods/src/patches/parts/fc_confirmation_reset.cc |
| SET-SEARCH | Settings search with contextual return | Implemented, unverified | mods/src/settings/native/settings_search.cc |
| SET-RESET-SAVE | Defaults, reset, conflicts and retry UI | Implemented, unverified | mods/src/settings/maintenance.h |
| SET-SHORTCUT-EDITOR | Interactive binding capture, conflict, undo/defaults | Implemented, unverified | mods/src/settings/shortcut_settings.cc |
| CFG-LOAD-RELOAD | Existing config defaults, aliases and activation | Implemented, unverified | mods/src/config.cc |
| CFG-WRITER | Registered-key preserving saves and quit | Implemented, unverified | mods/src/patches/parts/runtime_config.cc |
| STATE-STORE | Versioned state merge and atomic replacement | Implemented, unverified | mods/src/mod_state.cc |
| STATE-QUIT | Pending state writer on normal quit | Implemented, unverified | mods/src/mod_state.cc |
| SYNC-RECORDS | Owned per-target sync and session lifecycle | Implemented, unverified | mods/src/patches/parts/sync.cc |
| SYNC-BATTLES | Battle fetch, journals and participant resolution | Implemented, unverified | mods/src/patches/parts/sync.cc |
| SYNC-RESTART | Legacy attempted-ID suppression is not delivery ACK | Intentionally excluded | mods/src/patches/parts/sync.cc |
| PLAT-PATHS | Ordinary/custom/profile scoped file routing | Implemented, unverified | mods/src/file.cc |
| PLAT-MAC-LOG | Ordinary macOS log fallback | Implemented, unverified | mods/src/patches/patches.cc |
| PLAT-MAC-LAUNCH | macOS loader, launcher and update lifecycle | Implemented, unverified | macos-loader/src/main.cc |
| DIAG-OVERLAY | Debug console and six history/opacity controls | Missing | mods/src/dev/dev_console.cc |
| DIAG-BUS | Awake/source-gated lazy diagnostic history | Missing | mods/src/dev/diagnostics.cc |
| DIAG-SCIENCE-ERROR | Opt-in debug game-error JSONL recorder | Missing | mods/src/patches/parts/game_error_probe.cc |
| DIAG-FLEET-PROBE | Environment-gated debug fleet transitions | Implemented, unverified | mods/src/patches/fleet_watch.cc |
| DIAG-KEYBOARD-VARS | Layout diagnostics in runtime vars | Implemented, unverified | mods/src/patches/keyboard_layout.cc |
| PLAY-UI-SCALE | Interface scale and DPI | Implemented, unverified | mods/src/patches/parts/ui_scale.cc |
| PLAY-VIEWER-SCALE | Object-viewer scale | Implemented, unverified | mods/src/patches/parts/ui_scale.cc |
| PLAY-SHIP-MODEL-SCALE | Existing and newly loaded ship scale | Implemented, unverified | mods/src/patches/parts/ui_scale.cc |
| PLAY-ARTIFACT-EXCHANGE-ALL | Hide only artifact bulk conversion | Implemented, unverified | mods/src/patches/parts/artifact_exchange.cc |
| PLAY-OUT-OF-DOCK-POWER | Exclude station-only power buffs | Implemented, unverified | mods/src/patches/parts/buff_fixes.cc |
| PLAY-WINDOW-PROFILE-TITLE | Custom/profile window title | Implemented, unverified | mods/src/patches/parts/free_resize.cc |
| SET-AAI-ANALYSIS-ENTRY | Battle Analysis settings entry and availability | Implemented, unverified | mods/src/settings/upstream_settings.cc |

The JSON remains the sole machine-readable inventory. This document adds no
independent migration status or execution authority.

## Active migration

Branch: `play-dev-ripper`. Development integration: launch accepted; partial Windows player test; Settings UX follow-ups deferred. Not release or overall parity.
See [branch routing and preserved checkpoints](PLAY-PARITY.md#active-ripper-migration-branch--2026-10-26).
