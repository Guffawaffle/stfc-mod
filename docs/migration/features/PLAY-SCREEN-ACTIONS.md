# PLAY-SCREEN-ACTIONS — native screen shortcuts and deferred ship/officer buttons

Qualification update: the 382-step Windows suite and native build/boot passed at
`b36d2837`; the ordinary Windows trial was accepted ("looks good to me"). Evidence and limits:
[cohort 04](../FEATURE-COHORT-20261027-04.md).

Contract v1, source-preserving scan 2026-10-27. Frozen play-dev is
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper` begins at
`557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8`. This covers existing screen-opening,
arrow-navigation and delayed button operations, not a Settings or preset redesign.
Candidate-bound tests/builds and native qualification are recorded by the cohort
coordinator; source presence and the fixture implementation alone establish none
of those outcomes.

## Complete source scan and integration

Read the frozen `mods/src/patches/parts/hotkeys.cc` screen-opening chain,
`MoveDockInManagementView`, `MoveShipSelectionInDock`, `MoveArtifactCanvas`,
`MoveOfficerCanvas`, all three `Press*Button` helpers, native handler invocation,
deferred per-frame processing, input-mode gates and section-navigation helpers.
The historical inventory's `ScreenHotkeys` symbol is shorthand: these operations
actually live in `ScreenManager_Update_Hook` and the named helpers.

Supporting source read: `mods/src/prime/{OfficerAssignmentViewController,
ShipManagementScreenStateViewController,ShipManagementViewContext,
ShipManagementViewController,AssignShipsWidget,ArtifactHallDetailsViewController,
ElementSelectorViewController,InventoryListViewController,CanvasController,
SelectableList,Button,NavigationSectionManager,SectionManager}.h`, the separate
`patches/parts/focus_search.cc`, settings/shortcut registrations and current typed
counterparts. The target also relies on `tracked_objects.cc` and the existing
`hotkeys.cc::HandleShortcuts` frame owner. No new hook, tracker, binding layer,
general scheduler, persistence owner or independent screen model is introduced.

| Inspected branch | Exact revision | Presence and evidence |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Consumer present: legacy hotkeys file contains the ship/officer/preset helpers, deferred counters, screen destinations and arrow operations; separate focus-search source also exists. This is not a downstream-only capability. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Consumer present in the same legacy hotkeys workflow, including the three awaited button paths. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete source workflow and dependencies inspected. Frozen source is immutable. |
| refactor base | `557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8` | Existing typed `features/screens.cc` already implements the table, stage wait, arrows and search. This pass preserves it and corrects specific ownership/availability failures. |

Activation: existing `[patches] hotkeyhooks` and object tracking; live
`[control] hotkeys_enabled` (true) and `use_scopely_hotkeys` (false). The existing
shortcut catalog supplies bindings and native Settings editing. Defaults include
Alt-N Ship Swap, Alt-O Officer Swap, Ctrl-O Officer Presets, N Manage Ships,
A/Left and D/Right stepping. `ui.arrow_keys_to_select_ship` and `ui.focus_search`
both default true. Existing shortcut tiers, aliases, save outcomes and TOML
ownership remain unchanged.

## Source obligations and target disposition

| ID | Source operation | Target adaptation / qualification |
| --- | --- | --- |
| S1 | Match screen shortcuts in their existing first-match order, with native screen section IDs and flags. Manage Ships occurs after the complete setting-shortcut chain and does not consume native Update. | Existing `kScreens` destinations retained; Manage Ships is moved to a narrow `LateScreenShortcut` call after the setting chain, restoring source precedence. Production routing fixture drives 21 direct destinations, exact change flags, single invocation, overlapping-screen precedence and the separate late helper. Root-owned dispatcher wiring requires its own validation; the helper fixture does not pretend to execute the entire hotkey dispatcher. Native handler, Haven and navigation-state paths remain source-preserved and require separate native checks. |
| S2 | Galaxy/System restore stored section state; Bookmarks/Coordinate Search retain their manager/fallback paths; Ship Construction, Shields, Inbox, native Galaxy/Events call native handlers. | Existing typed helpers reused, without replacing native navigation or serialized state. No new forced fallback on failed state acquisition is added. |
| S3 | Ship/Officer Swap press an already visible active screen's native button before opening Manage for the selected fleet. | Same source operation and native button fields. Operation-local strong handles retain the controller/button. Successful invocation is distinguished from a nonnull field or failed checked `Press`. Failed custody or hidden/inactive Canvas declines. |
| S4 | Officer Presets use its widget → semaphore listener → native Button path; if absent, open officer assignment once, then await Presets. | Existing stage path retained. Actual-consumer fixture proves the intermediary swap press occurs once, waits through transition, and completes the nested presets button without repeating the previous action. |
| S5 | Open Manage with the selected fleet, `ActionType::Manage`, index zero and Default behavior; refuse when no fleet is selected. | Same native call and flags with rooted bar/panel/fleet. Checked native rejection now clears a newly awaited request rather than reporting field presence as acceptance. This does not claim server persistence or successful navigation beyond the native acknowledgement. |
| S6 | Deferred button work expires after 300 unsuccessful frame polls and is cancelled when mod hotkey mode/capture no longer owns input. | Original budget retained. A shared owned request snapshot survives synchronous native callbacks. Completion mutates only that request, never a replacement. Input capture, text focus, Escape, new screen intent, changed section owner/path and selected-fleet replacement cancel stale requests. Fixtures exercise acquisition cancellation and cancellation/replacement during getters/Press, timeout and mode gates. |
| S7 | Deferred work must not target a newly selected fleet or different screen source. | Strong SectionManager custody, checked section path and (when Manage is requested) exact bar/panel/fleet plus observed fleet ID. Only source section → Manage → officer assignment progression is allowed; unrelated navigation or regression after destination admission cancels. Ownership does not establish visual or gameplay equivalence; HUD lifetime while those screens open is an explicit native trial item. |
| S8 | Arrow order is ship details → Assign Ships (skip empty docks) → artifacts → selectors; all active instances of the first acting kind are processed. | Existing order and source loops retained. Ship/dock index/count/HasShip and native setters now use checked observations, preventing missing index from becoming measured zero. Ship-details fixture covers boundaries, unavailable observations, hidden Canvas and context replacement between setters. Full dock-list/native selector interactions remain a trial item. |
| S9 | Search focus uses visible active inventory/roster, assignment, then Assign Ships, without stealing focused text input. | Adjacent existing `PLAY-FOCUS-SEARCH` behavior remains intact; the outer input gate continues owning ordinary dispatch. This pass does not change its format aliases or settings. |
| S10 | Preserve less obvious source behavior and known limitations. | Fleet Commander still opens the native first-commander section; no selected-commander improvement is claimed. Native Button.Press retains its own interactability policy; completed invocation does not prove the native callback navigated. No destructive/resource action, default change, store migration or AAI change. |

## Ownership and deliberate corrections

Legacy source used independent global integer counters, while the typed target
already used one staged request. Keep that target's single-request replacement
policy; do not resurrect three simultaneous stale waits. The previous target
could dereference/reset `pending_button` after native Press synchronously cancelled
or replaced it. The bounded request is now locally owned through native calls,
and generation/request equality prevents stale acquisition or completion from
publishing into a different request. Its managed roots are explicitly released on
the game thread; process shutdown has no static GC-handle destructor.

The same episode fence applies before a wait exists: each immediate Show action
captures its starting revision, and the button helper stops its complete pooled
iteration if a native getter creates/cancels another request. If Press creates a
newer request and then throws, the older Show action cannot replace it with a
fallback wait. Actual-consumer fixtures cover all three immediate Show routes
under that throw-after-callback case and a first-hidden/second-actionable pooled
visibility replacement.

The target also placed its nonconsuming Manage Ships entry before Settings
shortcuts. A shared chord could therefore both navigate and change a setting;
the source's else-if chain executed only the setting action. The narrow late
helper restores that ordering; root owns the `hotkeys.cc` matched-setting result
and call site. No generic dispatch framework is introduced.

The original delayed paths ran before text-focus processing. This pass deliberately
cancels deferred actions on text focus and Escape and gives a newer screen chord
precedence over an older deferred button. This is a safety correction, not a claim
that the frozen source already implemented those cases.

Manage navigation retains exactly the selected bar/panel/fleet. These are bounded
roots for one request, never a scene scan or global retained UI cache. Replacement,
unavailable identity, native rejection and mode changes retire that request.
Returning from assignment to management after assignment was observed also
cancels it. Closing a native screen before its transition is observed can remain
indistinguishable from a slow screen; the original 300-poll timeout is retained.

## Verification and remaining native checks

`tests/screen_actions_bindings_cases.h` includes actual `screens.cc`, reusing the
existing controlled-runtime tracker/input/config/GC substitutes. Modes:
`screen-actions-buttons`, `screen-actions-pending`, `screen-actions-routing`,
`screen-actions-stepping`. Native bodies for Canvas, Button.Press, section change,
fleet Manage acknowledgement and detail-index mutation are counted substitutes.
Tests do not install hooks, render Unity UI, establish exact-client availability,
exercise actual GC/scene pooling, write TOML or persist presets. No test execution
is claimed here before the coordinator's candidate-bound run.

Native workflow: use existing/default or operator-chosen bindings. Open Ship Swap,
Officer Swap and Presets from space and directly from their screens; wait for
transitions and verify once-only activation. Start another screen, change selected
fleet, enter text or press Back/Escape before completion; verify no late button
press. Verify hidden/reopened pooled views and normal native back navigation.
Check HUD bar lifetime across Manage/Assignment, ship detail boundaries, empty
dock skipping, artifact/selector arrow precedence and inventory/roster search.
Check navigation returns to saved camera state, bookmarks/search and all native
shortcut routes. Both macOS builds and Mcohall's client tests remain independent
of Windows compilation or controlled fixture results.

Settings/preset functionality, AAI and Crew & Effects, stored configuration,
layout redesign, system orbit and other cohort lanes remain outside this file's
restoration. Final candidate, fixture/build results and runtime observations are
bound by the coordinator's cohort record rather than inferred from source.
