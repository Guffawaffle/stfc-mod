# PLAY-GALAXY-OVERLAYS — simultaneous native map layers

Contract v1, 2026-10-27. Source is frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target starts at
`c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce` on `play-dev-ripper`.
Existing behavior is being preserved and qualified, not re-created from labels.

## Discovery and entry points

Read complete frozen `mods/src/patches/parts/galaxy_labels.cc`, its policy/header,
layout tracking and housing dependencies, `mods/src/settings/galaxy_labels.cc`,
configuration, native HUD/button/list bindings and shared zoom/LOD call sites.
Current equivalents are `features/galaxy.cc`, `game/galaxy.h`, `galaxy_policy.h`
and the existing Settings/registered writer. The original data rules and native
composition helpers remain authoritative.

The named multi-overlay capability is absent in inspected upstream dev
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5` (no multi-select/overlay-flag keys or
composition consumer). PLAY `cad98ef7990dd66167eb64ea5dda525c1fe2c296`, frozen
play-dev and the target all contain it. Native single-overlay controls are not
equivalent to this capability.

`graphics.galaxy_multi_select` defaults false. The four effective flags are
Default=true, Mining=false, Hostiles=false, Hazards=false. Both native Galaxy
buttons and existing Mod Settings controls use `SetOverlay`; removing the last
layer restores Default. All affected flags go to the established registered-key
writer. No setting/default, TOML format, save outcome or store ownership changes.

## Behavioral obligations

| ID | Frozen operation | Current mapping / verification limit |
| --- | --- | --- |
| O1 | Buttons independently toggle Default, Mining, Hostiles and Hazards; last deselection restores Default. | Existing compact policy and `SetOverlay` retained. Actual HUD/control fixture drives every click across all 15 nonempty combinations and checks effective flags/native representative. |
| O2 | Native selected-state animation matches the same effective selection, including Default fallback; disabled mode retains native single selection. | Production animation and HUD hook bodies are exercised together. Real game button rendering is still an in-game check. |
| O3 | Persist the complete effective selection, not merely the clicked flag. Settings and native buttons share the same owner. | Existing four-key registered save loop retained. The production fixture observes the save sink and runtime flags; real background write/reload status remains separate Settings-writer evidence. |
| O4 | Use native visited/range/level/hazard gates. Prefer the native helper composition path; retain bounded per-selected-overlay native passes if helpers are unavailable. | Existing `UpdateStarData` composition preserved, including resource/hostile lists, local-active container union and name priority. Pure flag tests do not qualify actual native list/layout rendering. |
| O5 | Temporary native mode is restored only while this composition owns it. A newer player selection, including the same mode, wins; config replacement stops old composition. | Existing revision/config/write-ownership checks retained. Static policy tests cover ownership; the new actual button fixture confirms revision advancement. No claim of executing all reentrant native rendering paths. |
| O6 | Restore hierarchy/layout snapshots in reverse order before rebinding/release or replacing custom layout. A collected moved child does not erase surviving group/element changes. | Repaired restoration order and operation-local strong custody. Actual lifecycle fixture tests surviving group restoration, managed/handle failure retention and reentrant replacement. |
| O7 | Disabling multi-selection restores inactive pooled layouts too, without waiting for their next visible redraw. | Restored the source's disable transition through existing Settings Refresh/AfterZoom owners. It runs before Galaxy depth admission; a failed journal keeps a pending-cleanup latch after the transition and retries on later existing callbacks. Fixture executes actual Refresh/AfterZoom without a current camera, first failing and then succeeding. |

## Ownership and deliberate differences

Weak owner identity and Galaxy node identity remain separate from temporary strong
custody. Source records are not overwritten merely because a weak owner no longer
matches the pooled address. Refreshing registries distinguishes an expired weak
target from failure to allocate a new strong handle; unavailable Unity liveness
does not establish destruction. Reentrant native cleanup cannot erase or replace
a newer record published by a nested bind.

No generalized pool, deduplication framework or competing hook owner is added.
The narrow refresh helper is used by the three existing weak registries. Native
composition, typography, icon dimensions and user-facing visual design are left
intact. Failed cleanup retains its source record rather than creating a false
successful disposal claim.

Disable cleanup has separate bounded layout/marker retry latches over the already
tracked records; it adds no frame owner or persistent object pool. Successful
restoration clears the layout latch independently of marker cleanup. Re-enabling
the feature cancels its disabled-only latch so later retries cannot unexpectedly
tear down enabled presentation. Its retained journal remains available to the
normal Data/bind/release path, which restores before reusing the layout. Controlled
fixtures cover first-failure/later-success and re-enable followed by another
disable, not merely direct calls to the cleanup helper.

## Validation and remaining work

The production overlays/lifecycle modes in
`tests/galaxy_presentation_bindings_cases.h` operate against controlled native
bodies and the existing input/config/save substitutes. They establish actual
adapter behavior at that layer, not native visual equivalence or real file
persistence. Candidate-bound execution results are recorded centrally by the
coordinator after integration.

Native trial: check all layer combinations and saved flags, toggle multi-selection
off while labels are pooled/inactive, reopen/return to Galaxy and inspect icon/text
spacing. Check unvisited/range-limited/hazard systems and inactive-parent reuse;
these retain game-owned data eligibility. Reentrant pooled release, native setter
failure and both Mac architectures remain separately qualified. Historical M8
missing-Settings claims do not describe the current registered controls.

No overall migration parity, UI redesign, AAI change, system orbit or unrelated
camera behavior is implied by this bounded contract.
