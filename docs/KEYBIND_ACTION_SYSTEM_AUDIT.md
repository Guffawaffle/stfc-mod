# Keybind and User Input Action System Audit

Source folder: `D:\dev\stfc-mod`

Purpose: preserve the current understanding of every user-input-driven action the mod can handle, with special attention to the right-click/action path for the next keybind cleanup sprint.

This is a read-only audit. It records source-verified behavior and likely sprint work; it does not include code changes.

## Source Files Reviewed

- `mods/src/patches/gamefunctions.h`: enum inventory for bindable mod actions.
- `mods/src/defaultconfig.h`: default shortcut strings and input-related config defaults.
- `mods/src/config.cc`: TOML shortcut loading, gates, aliases, runtime vars writing.
- `mods/src/patches/key.cc`: key name parsing, per-frame key cache, input focus, modifier helpers.
- `mods/src/patches/mapkey.cc`: shortcut parsing, mapping storage, matching semantics.
- `mods/src/patches/modifierkey.cc`: modifier parsing and pressed/down checks.
- `mods/src/patches/frame_tick.cc`: `ScreenManager.Update` hook fan-out.
- `mods/src/patches/hotkey_router.cc`: main per-frame hotkey router.
- `mods/src/patches/hotkey_dispatch.cc`: table-driven navigation/toggle/log/UI-scale actions.
- `mods/src/patches/fleet_actions.cc`: ship select, right-click/space action routing, queue, recall, repair.
- `mods/src/patches/viewer_mgmt.cc`: Escape viewer dismissal, reward dismissal, `ActionView` info toggle.
- `mods/src/patches/cargo_display.cc`: auto cargo/rewards display by target type.
- `mods/src/patches/parts/hotkeys.cc`: shortcut initialization hook, cargo/pre-scan hooks, Escape exit suppression seam.
- `mods/src/patches/parts/zoom.cc`: zoom preset, zoom in/out, set default/preset input handling.
- `mods/src/patches/parts/fix_pan.cc`: mouse/touch pan momentum and move-key-related original suppression.
- `mods/src/patches/parts/chat.cc`: chat tab click/swipe/message suppression for disabled chat channels.
- `mods/src/testable_functions.cc`: pure router/fallthrough/escape decision helpers.
- `README.md`, `KEYMAPPING.md`, `example_community_patch_settings.toml`: user-facing shortcut documentation.

## System Shape

The mod has more than one input path. The cleanup sprint should treat these as a single product surface, but they are implemented in separate hooks today.

1. `ShortcutsManager.InitializeActions` hook:
   - Implemented in `parts/hotkeys.cc`.
   - Calls the original Scopely shortcut initializer when `use_scopely_hotkeys || allow_key_fallthrough` is true.
   - Suppresses it otherwise.

2. `ScreenManager.Update` hook:
   - Installed in `frame_tick.cc`.
   - Ticks live-debug first, then the hotkey router.
   - Calls original update when the router allows it, or when `allow_key_fallthrough` is enabled.

3. Main hotkey router:
   - Implemented in `hotkey_router.cc`.
   - Resets cached key state once per frame.
   - Handles enable/disable hotkeys, quit, ship selection, chat shortcuts, table-driven hotkeys, Escape viewer/rewards behavior, space/right-click actions, and `ActionView`.

4. Zoom hook:
   - Implemented in `parts/zoom.cc`.
   - Reads `MapKey` state directly from `NavigationZoom.Update`, not through the router dispatch table.
   - Handles zoom preset recall, smooth zoom in/out, min/max/reset, and setting preset/default zoom values.

5. Pan and touch hooks:
   - Implemented in `parts/fix_pan.cc`.
   - Adjust touch phase and mouse/touch pan momentum.
   - The current WASD/move shortcut enum values are not active, but `disable_move_keys` can suppress the game's original pan update in `NavigationPan.LateUpdate`.

6. Chat UI hooks:
   - Implemented in `parts/chat.cc`.
   - Not shortcut-based for most behavior; they intercept tab clicks, swipe focus, preview focus, and incoming chat messages.

7. Cargo/pre-scan hooks:
   - Implemented in `parts/hotkeys.cc`, `hotkey_router.cc`, and `cargo_display.cc`.
   - Track reward/pre-scan widgets and auto-show the cargo/rewards panel based on config and target type.

## Binding Parser Semantics

Shortcut values are loaded from `[shortcuts]` in TOML.

- Values are case-insensitive and converted to upper case.
- Pipe-delimited bindings mean OR, for example `SPACE|MOUSE1`.
- Hyphen-delimited parts mean modifiers plus one primary key, for example `CTRL-SHIFT-F9`.
- `NONE` explicitly unbinds an action and does not fall back to the default.
- A binding without modifiers only matches when no modifier key is pressed.
- A binding with modifiers requires every configured modifier group to be pressed, but it does not reject extra pressed modifiers.
- `MapKey::IsDown` uses Unity `GetKeyDownInt`; `MapKey::IsPressed` uses Unity `GetKeyInt`.

Important parser risks:

- `MapKey::Parse` allocates with `new MapKey()` and returns by value, leaking the allocated object for every parsed shortcut token.
- Invalid shortcut tokens are warned about, but still registered as `KeyCode::None`. Press detection skips them, but `GetShortcuts()` can still produce confusing runtime output.
- There is no config-level conflict detection for duplicate bindings or overlapping modifier sets.
- Extra modifiers are allowed for modified bindings. If `CTRL-A` and `CTRL-SHIFT-A` are both configured, pressing `CTRL-SHIFT-A` can satisfy both; dispatch order decides which behavior wins.
- `Key::HasAlt()` checks `RightShift` instead of `RightAlt`. Source search found this helper is not currently used for `MapKey` modifier matching, so this is a dormant helper bug today, not the current explanation for ALT hotkey behavior.

## Gating and Config Compatibility

- `hotkeys_enabled` is the master mod hotkey toggle.
- `use_scopely_hotkeys` lets Scopely's built-in shortcut map run and causes the router startup decision to allow original input processing when hotkeys are enabled.
- `allow_key_fallthrough` has two effects today: it allows Scopely shortcut initialization and also allows original `ScreenManager.Update` even when the router tried to suppress it.
- `hotkeys_extended` gates extended shortcuts: alliance/bookmark shortcuts, set zoom preset/default, preview toggles, cargo toggles.
- `enable_experimental` gates `show_alliance_armada`, `show_alliance_help`, and `show_lookup`, and only when `hotkeys_extended` is also true.
- `move_up`, `move_down`, `move_left`, and `move_right` exist in defaults and in the example config, but the config loading code for them is commented out. `MoveLeft` and `MoveRight` have router stubs that call `MoveOfficerCanvas`, which currently returns false; `MoveUp` and `MoveDown` are not routed.
- Disable-hotkeys shortcut has a canonical key, `set_hotkeys_disable`, plus deprecated aliases `set_hotkeys_disble` and `set_hotkeys_disabled`.
- Enable-hotkeys shortcut currently loads from `set_hotkeys_enable`, but `example_community_patch_settings.toml` shows `set_hotkeys_enabled`. That means a user editing the example spelling is ignored by current loading and the default `CTRL-ALT-=` remains active.

## Default Shortcut Inventory

The enum in `gamefunctions.h` contains 90 action values before `Max`. Most are bindable today, but not all are loaded or implemented.

### Master Control and Process

| Config key | Default | Runtime path | Notes |
| --- | --- | --- | --- |
| `set_hotkeys_disable` | `CTRL-ALT-MINUS` | Router startup check | Sets `Config::hotkeys_enabled = false` and suppresses original for that frame. Deprecated aliases exist. |
| `set_hotkeys_enable` | `CTRL-ALT-=` | Router startup check | Sets `Config::hotkeys_enabled = true` and suppresses original for that frame. Example config currently uses `set_hotkeys_enabled`, which is not loaded. |
| `quit` | `F10` | Router, Windows only | Calls `TerminateProcess(GetCurrentProcess(), 1)` under `_WIN32`. |

### Chat Opening and Channel Selection

| Config key | Default | Runtime path | Notes |
| --- | --- | --- | --- |
| `show_chat` | `C` | Router, non-chat/non-focused | Opens or focuses Alliance chat. Fullscreen if side chat is not already open. |
| `show_chatside1` | `ALT-C` | Router, non-chat/non-focused | Opens/focuses Alliance side chat. Affected by the dormant `HasAlt` bug only if future code uses `Key::HasAlt`; current modifier matching uses `ModifierKey`. |
| `show_chatside2` | `` ` `` | Router, non-chat/non-focused | Same side-chat path as `show_chatside1`. |
| `select_chatglobal` | `CTRL-1` | Router, in-chat only | Opens Global chat channel. |
| `select_chatalliance` | `CTRL-2` | Router, in-chat only | Opens Alliance chat channel. |
| `select_chatprivate` | `CTRL-3` | Router, in-chat only | Opens Private chat channel. |

Non-shortcut chat input behavior:

- Fullscreen chat tab clicks are blocked when Galaxy or Veil chat is disabled.
- Chat preview panel focus/swipe is redirected to Alliance when disabled channels are targeted.
- Incoming Galaxy/Regional message callbacks are suppressed when those channels are disabled.

### Ship Selection and Locate

| Config key | Default | Runtime path | Notes |
| --- | --- | --- | --- |
| `select_ship1` through `select_ship8` | `1` through `8` | Router -> `HandleShipSelection` | Selects the matching fleet slot. Double-tapping the already-selected ship within `select_timer` locates it, unless preview locate is disabled and a viewer can be hidden. |
| `select_current` | `CTRL-SPACE` | Router | Requests view for the currently selected fleet in the fleet bar. |

Additional ship selection behavior:

- `SHIFT` plus a ship-select key attempts Discovery tow logic by looking for a fleet with hull id `1307832955`.
- Ship selection clears any deferred space action before changing context.
- The Shift tow path assumes the selected fleet slot exists; empty slots need a null-guard pass.

### Primary Action, Right-Click, Queue, Recall, Repair, and View

The README explicitly documents `MOUSE1` as right mouse click. In current defaults, right-click is part of the same action surface as spacebar.

| Config key | Default | Runtime path | Notes |
| --- | --- | --- | --- |
| `action_primary` | `SPACE|MOUSE1` | Router -> `ExecuteSpaceAction` | Main context-dependent action. Right-click enters this path. |
| `action_queue` | `SPACE|MOUSE1` | Router -> `ExecuteSpaceAction` | Same default as primary. If queue is unlocked and the visible pre-scan target exposes an active add-to-queue button, queue handling runs before primary engage. |
| `action_recall_cancel` | `SPACE|MOUSE1` | Router -> `ExecuteSpaceAction` | Same default as primary. If the selected fleet is warping/warp-charging, this can cancel warp unless visible viewer context suppresses mouse warp cancel. |
| `action_secondary` | `TAB|MOUSE4` | Router -> `ExecuteSpaceAction` | Scan on pre-scan/mining viewers; view activation on star nodes. |
| `action_queue_clear` | `CTRL-C` | Router -> `ExecuteSpaceAction` | Clears the action queue for the selected fleet. Checked before other space-action contexts. |
| `action_view` | `V|MOUSE2` | Router -> `HandleActionView` | Toggles cargo/rewards info panel for visible pre-scan targets. Uses a multi-frame `show_info_pending` follow-up. |
| `action_recall` | `R|MOUSE3` | Router -> `ExecuteSpaceAction` | Recalls fleets in `IdleInSpace`, `Impulsing`, `Mining`, or `Capturing` states when preview recall policy allows. README describes `MOUSE3` as a side mouse button. |
| `action_repair` | `R|MOUSE3` | Router -> `ExecuteSpaceAction` | Repairs fleets in `Docked` or `Destroyed` states, with an AskHelp retry path for `Repairing`. Sharing the key with recall is state-dependent and appears intentional. |
| `toggle_queue` | `CTRL-Q` | Router | Toggles `Config::queue_enabled`. The current queue action path checks `ActionQueueManager::IsQueueUnlocked()`, not `queue_enabled`, so this toggle needs semantic review. |

Current `ExecuteSpaceAction` priority order:

1. Clear queue if `action_queue_clear` is down.
2. Cancel warp if `action_recall_cancel` is down and the selected fleet is warping/warp-charging.
3. For each visible `PreScanTargetWidget`:
   - If a mining viewer is also visible: secondary scans, primary mines.
   - If queue is unlocked and `action_queue` is down: press the target add-to-queue button when available and not full.
   - If secondary is down: scan.
   - If primary is down: engage, press armada attack, or defer if the target type is still `Any`.
4. If a mining viewer is visible outside the pre-scan loop: secondary scans, primary mines.
5. If a star-node viewer is visible: secondary views, primary initiates warp.
6. If a navigation interaction UI is visible and primary is down: join armada when possible, otherwise set course.
7. If recall is down and state permits it: recall.
8. If repair is down and state permits it: repair.

Right-click implications:

- Right-click is not a single direct action. It sets `has_primary`, `has_queue`, and `has_recall_cancel` under the defaults because all three bind to `SPACE|MOUSE1`.
- The result depends on current fleet state, visible viewer state, queue availability, armada visibility, and whether the target context has resolved from `Any` to a known hull type.
- This explains why right-click is the highest-risk cleanup target: one physical input can mean queue, mine, engage, armada attack, warp cancel, join armada, or set course.
- A cleanup should make that policy explicit and testable instead of relying on implicit ordering inside `ExecuteSpaceAction`.

### Section Navigation and Panels

These are table-driven in `hotkey_dispatch.cc`. Most return `HandledStop`, suppressing the original update. `ShowShips` returns `HandledAllowOriginal`.

| Config key | Default | Target behavior |
| --- | --- | --- |
| `show_awayteam` | `T` | `Missions_AwayTeamsList` |
| `show_gifts` | `/` | `Shop_List` |
| `show_artifacts` | `SHIFT-I` | `ArtifactHall_Inventory` |
| `show_commander` | `O` | `FleetCommander_Management` |
| `show_daily` | `Z` | `Missions_DailyGoals` |
| `show_events` | `SHIFT-E` | `Tournament_Group_Selection` |
| `show_exocomp` | `X` | `Consumables` |
| `show_factions` | `F` | `Shop_MainFactions` |
| `show_inventory` | `I` | `InventoryList` |
| `show_missions` | `M` | `Missions_AcceptedList` |
| `show_research` | `U` | `Research_LandingPage` |
| `show_scrapyard` | `Y` | `ShipScrapping_List` |
| `show_settings` | `SHIFT-S` | `GameSettings` |
| `show_officers` | `SHIFT-O` | `OfficerInventory` |
| `show_qtrials` | `SHIFT-Q` | `ChallengeSelection` |
| `show_refinery` | `SHIFT-F` | `Shop_Refining_List` |
| `show_ships` | `N` | Selected fleet manage action via fleet panel controller |
| `show_stationexterior` | `SHIFT-G` | `Starbase_Exterior`; enum value is spelled `ShoWStationExterior` |
| `show_stationinterior` | `SHIFT-H` | `Starbase_Interior` |
| `show_galaxy` | `G` | Navigation galaxy section |
| `show_system` | `H` | Navigation system section |

Extended or experimental section shortcuts:

| Config key | Default | Gate | Target behavior |
| --- | --- | --- | --- |
| `show_alliance` | `ALT-'` | `hotkeys_extended` | `Alliance_Main` |
| `show_bookmarks` | `B` | `hotkeys_extended` | Bookmark manager or `Bookmarks_Main` |
| `show_alliance_help` | `SHIFT-'` | `hotkeys_extended && enable_experimental` | `Alliance_Help` |
| `show_alliance_armada` | `CTRL-'` | `hotkeys_extended && enable_experimental` | `Alliance_Armadas` |
| `show_lookup` | `L` | `hotkeys_extended && enable_experimental` | `Bookmarks_Search_Coordinates` |

### Zoom Actions

Zoom input is handled in `NavigationZoom.Update`, separate from the main router.

| Config key | Default | Runtime path | Notes |
| --- | --- | --- | --- |
| `zoom_in` | `Q` | `NavigationZoom.Update` | Held input; zooms at `keyboard_zoom_speed * deltaTime`, anchored to mouse position. |
| `zoom_out` | `E` | `NavigationZoom.Update` | Held input; negative zoom delta. |
| `zoom_preset1` through `zoom_preset5` | `F1` through `F5` | `NavigationZoom.Update` | Absolute zoom recall from configured preset values. |
| `zoom_reset` | `=` | `NavigationZoom.Update` | Extended gate; applies `default_system_zoom`. |
| `zoom_min` | `BACKSPACE` | `NavigationZoom.Update` | Extended gate; sets zoom to configured max-distance scale. |
| `zoom_max` | `MINUS` | `NavigationZoom.Update` | Extended gate; sets close zoom value `100`. |
| `set_zoom_preset1` through `set_zoom_preset5` | `SHIFT-F1` through `SHIFT-F5` | `NavigationZoom.Update` | Extended gate; stores current normalized distance into the matching preset. |
| `set_zoom_default` | `CTRL-=` | `NavigationZoom.Update` | Extended gate; stores current normalized distance as default system zoom. |

Additional zoom behavior:

- Entering system view sets `do_default_zoom`, so the next zoom update applies `default_system_zoom`.
- If `use_presets_as_default` is true, recalling a preset stores that value as the default too.
- Zoom actions are blocked only by `Key::IsInputFocused`; they do not check `Hub::IsInChat` directly.

### UI Scale and Viewer Scale

These are table-driven and use `InputMode::Pressed`, so they repeat while held.

| Config key | Default | Runtime path | Notes |
| --- | --- | --- | --- |
| `ui_scaleup` | `PGUP` | Dispatch table -> `Config::AdjustUiScale(true)` | Clamped to `[0.1, 2.0]`; only works when `ui_scale != 0.0`. |
| `ui_scaledown` | `PGDOWN` | Dispatch table -> `Config::AdjustUiScale(false)` | Same clamp and nonzero rule. |
| `ui_scaleviewerup` | `SHIFT-PGUP` | Dispatch table -> `Config::AdjustUiViewerScale(true)` | Uses one quarter of `ui_scale_adjust`; only works when `ui_scale_viewer != 0.0`. |
| `ui_scaleviewerdown` | `SHIFT-PGDOWN` | Dispatch table -> `Config::AdjustUiViewerScale(false)` | Same viewer scaling path. |

Non-shortcut UI-scale behavior:

- `ScreenManager.UpdateCanvasRootScaleFactor` applies DPI/resolution-aware root canvas scaling.
- `CanvasController.Show` applies viewer scale to `ObjectViewerTemplate_Canvas`.
- On Windows, the root-scale hook also resets the cursor to arrow when `allow_cursor` is false.

### Preview and Cargo Display Toggles

These are gated by `hotkeys_extended`.

| Config key | Default | Runtime path | Notes |
| --- | --- | --- | --- |
| `toggle_preview_locate` | `CTRL-R` | Dispatch table | Toggles `disable_preview_locate`, affecting double-tap ship locate behavior. |
| `toggle_preview_recall` | `CTRL-T` | Dispatch table | Toggles `disable_preview_recall`, affecting recall action when viewers can be hidden. |
| `toggle_cargo_default` | `ALT-1` | Dispatch table | Toggles whether auto cargo display is globally enabled. |
| `toggle_cargo_player` | `ALT-2` | Dispatch table | Toggles auto cargo display for player fleets. |
| `toggle_cargo_station` | `ALT-3` | Dispatch table | Toggles auto cargo display for stations/no deployed target fleet. |
| `toggle_cargo_hostile` | `ALT-4` | Dispatch table | Toggles auto cargo display for hostile/marauder targets. |
| `toggle_cargo_armada` | `ALT-5` | Dispatch table | Toggles auto cargo display for armada targets. |

Non-shortcut cargo behavior:

- `RewardsButtonWidget.OnDidBindContext` and `PreScanTargetWidget.ShowWithFleet` feed widget context into cargo display logic.
- Cargo type is inferred from `TargetFleetDeployedData`, `FleetType`, and hull type.

### Logging Level Actions

These are table-driven runtime diagnostics shortcuts.

| Config key | Default | Runtime behavior |
| --- | --- | --- |
| `log_trace` | `CTRL-SHIFT-F7` | `spdlog::set_level(trace)`, flush on trace. |
| `log_info` | `CTRL-SHIFT-F8` | `spdlog::set_level(info)`, flush on info. |
| `log_debug` | `CTRL-SHIFT-F9` | `spdlog::set_level(debug)`, flush on debug. |
| `log_warn` | `CTRL-SHIFT-F10` | `spdlog::set_level(warn)`, flush on warn. |
| `log_error` | `CTRL-SHIFT-F11` | `spdlog::set_level(err)`, flush on err. |
| `log_off` | `CTRL-SHIFT-F12` | `spdlog::set_level(off)`, flush on off. |

### Movement, Pan, and Camera Movement

There are two separate concepts here:

1. Shortcut movement enum values:
   - `MoveLeft`, `MoveRight`, `MoveUp`, and `MoveDown` exist in `GameFunction` and defaults are `A`, `D`, `W`, `S`.
   - The config parser for all four is commented out.
   - The example config still includes these keys as experimental.
   - The router only checks `MoveLeft` and `MoveRight`, both through `MoveOfficerCanvas`, which currently returns false.
   - Current conclusion: WASD/move shortcut actions are effectively not implemented.

2. Mouse/touch pan hook:
   - `TKTouch.populateWithPosition` converts `Stationary` phase to `Moved`.
   - `NavigationPan.LateUpdate` optionally calls original pan update, applies momentum falloff when mouse/touch is released, and keeps extended far-radius equal to normal.
   - `disable_move_keys` suppresses the original pan update path when true, despite the name sounding like only keyboard movement.

### Escape and Back Button Behavior

Escape is fixed, not configurable through `[shortcuts]`.

- In the router, Escape clears input focus when an input field is focused or the player is in chat.
- Outside input focus, Escape hides visible object viewers via `DidHideViewers()`.
- Escape also participates in rewards screen dismissal along with `action_primary`.
- Exit prompt suppression happens at the real back-button seam, `SectionManager.BackButtonPressed`, using `disable_escape_exit` and `escape_exit_timer`.
- Because `allow_key_fallthrough` can force original `ScreenManager.Update` to run, Escape behavior needs careful seam testing whenever fallthrough is enabled.

## Highest-Risk Cleanup Items

1. Make right-click policy explicit and testable.
   - `MOUSE1` is documented as right-click and currently maps to primary, queue, and warp-cancel.
   - Current behavior depends on implicit priority order inside `ExecuteSpaceAction`.
   - Recommended outcome: a pure decision function that takes input flags, fleet state, viewer state, queue state, and target type, then returns one action enum plus suppression policy.

2. Split `allow_key_fallthrough` responsibilities.
   - It currently affects both Scopely shortcut initialization and per-frame original update allowance.
   - Recommended outcome: separate startup/original-shortcut policy from per-frame original-update policy.

3. Fix config compatibility for enable-hotkeys.
   - Loader expects `set_hotkeys_enable`; example config uses `set_hotkeys_enabled`.
   - Recommended outcome: accept both spellings, emit deprecation/alias warnings, and write the canonical key.

4. Decide the fate of movement shortcuts.
   - Defaults and example entries advertise WASD, but bindings are not loaded and behavior is stubbed.
   - Recommended outcome: either implement and test the movement actions, or remove/comment them from generated user docs and runtime expectations.

5. Add shortcut validation.
   - Detect duplicate exact bindings.
   - Detect overlapping modifier bindings such as `CTRL-A` and `CTRL-SHIFT-A`.
   - Do not register invalid `KeyCode::None` bindings.
   - Report shadowed actions in runtime vars/logs.

6. Remove parser leak and tighten parsing tests.
   - Replace heap allocation in `MapKey::Parse` with stack construction.
   - Add tests for `NONE`, invalid tokens, pipe-delimited partial invalids, modifier-only strings, duplicate modifiers, mouse keys, and extra modifier behavior.

7. Add null-guard hardening in input-triggered handlers.
   - Candidates: Shift tow path, selected fleet/fleet panel access, `HandleActionView`, cargo reward widget access, `ShowShips` fleet controller access.
   - The goal is not broad defensive churn; focus on user-input paths where a half-bound viewer or empty fleet slot can be hit by a key press.

8. Clarify suppression semantics.
   - Some actions explicitly suppress original update, some allow original, and space actions suppress implicitly via router flow.
   - Recommended outcome: handlers return an explicit decision, especially `ExecuteSpaceAction`.

9. Add observability for ambiguous action contexts.
   - Current diagnostics log slow/unresolved space actions and handled primary outcomes.
   - Recommended additions: log when multiple viewer contexts are visible, when right-click chooses queue over primary, and when fallthrough overrides router suppression.

## Suggested Sprint Slices

1. Test harness first:
   - Add pure tests for shortcut parsing and config alias behavior.
   - Extract right-click/space action decision logic into a testable function without IL2CPP calls.

2. Compatibility and correctness fixes:
   - `set_hotkeys_enable`/`set_hotkeys_enabled` alias.
   - `MapKey::Parse` leak.
   - Invalid-token registration.
   - `Key::HasAlt` dormant helper bug.

3. Right-click cleanup:
   - Define action priority in one enum-returning decision function.
   - Preserve current defaults intentionally, but make `MOUSE1` choosing queue/primary/warp-cancel visible and test-covered.

4. Movement cleanup:
   - Either implement WASD/move actions end to end or remove them from generated docs/runtime defaults.
   - Rename or document `disable_move_keys` if it continues to mean suppressing original pan update.

5. Safety and diagnostics:
   - Null guards on user-input-triggered paths.
   - Debug logs for shadowed bindings and ambiguous viewer states.

## Validation Notes

No tests were run for this audit document. The findings above are based on source reads and repo documentation. The next code sprint should validate with the repo's normal XMake test/build flow after changes are made.