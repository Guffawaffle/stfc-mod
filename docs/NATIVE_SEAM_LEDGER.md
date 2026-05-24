# Native Seam Ledger

Purpose: maintain living confidence records for native seams and generated hook surfaces. Policy lives in [Native Probe Safety](NATIVE_PROBE_SAFETY.md); this file records seam-specific evidence.

Rules for this ledger:

- Static source or IL2CPP dump evidence is not runtime confidence.
- A generated family can be tracked as a risk surface, but it does not make any member product-safe.
- One callback's result does not transfer to siblings with similar signatures.
- Add or update a ledger entry before runtime work that goes beyond static review.
- Keep failed seams in the ledger so future work does not rediscover the same crash path.

## Entry Template

Use this shape for future seams:

- Seam:
- Owner / file:
- Intended question:
- Static evidence:
- Risk class:
- Confidence rung:
- Runtime evidence:
- Payload confidence:
- Original/trampoline confidence:
- Flag / rollback path:
- Status:
- Next action:

Confidence rungs are defined in [Native Probe Safety](NATIVE_PROBE_SAFETY.md): `symbol exists -> static relationship -> runtime observed -> state-correlated -> payload understood -> product-safe`.

Risk classes are also defined there: R0 static, R1 passive runtime, R2 managed log-only, R3 native canary, R4 native interpretation, and R5 behavioral or broad.

## Entries

### Static Inventory Snapshot - 2026-05-23

This inventory records source-level seams from a static-only review. It does not claim new runtime observation, payload confidence, original/trampoline confidence, or product-safe status. "Operationally relied on" below means the seam is part of current product code paths; it is not a new confidence rung and is not evidence from this pass.

| Seam | Owner / file | Functionality supported | Risk class | Confidence / status |
| --- | --- | --- | --- | --- |
| `ScreenManager.Update` frame tick | `mods/src/patches/frame_tick.cc`, installed from `mods/src/patches/patches.cc` | Central frame fan-out for hotkeys, optional live-debug tick, and original frame policy | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `ShortcutsManager.InitializeActions` | `mods/src/patches/parts/hotkeys.cc` | Controls whether Scopely shortcut initialization runs according to shortcut policy | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `ShortcutsManager.LateUpdate` | `mods/src/patches/parts/hotkeys.cc` | Suppresses native shortcut update when dispatcher-owned input should win | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `ShortcutsManager.SelectShip(int)` | `mods/src/patches/parts/hotkeys.cc` | Native fleet-selection suppression/fallthrough around numeric ship selection | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `RewardsButtonWidget.OnDidBindContext` | `mods/src/patches/parts/hotkeys.cc` | Reward/cargo context capture for hotkey action state | R4/R5 native interpretation / product behavior | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `PreScanTargetWidget.ShowWithFleet` | `mods/src/patches/parts/hotkeys.cc` | Pre-scan target and fleet context capture for scan/cargo actions | R4/R5 native interpretation / product behavior | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `FleetBarViewController.RequestSelect(int)`, `RequestSelect(Component)`, `ElementAction(int)` | `mods/src/patches/parts/hotkeys.cc` | Defensive suppression around native fleet selection and fleet-bar click/select paths | R5 behavioral | Grouped because the seams support one fleet-bar selection boundary; operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `SectionManager.BackButtonPressed` | `mods/src/patches/parts/hotkeys.cc` | Escape/back duplicate suppression | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `NavigationInteractionUIViewController.OnSetCourseButtonClick` | `mods/src/patches/parts/hotkeys.cc` | Duplicate set-course suppression | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `NavigationZoom.Update` | `mods/src/patches/parts/zoom.cc` | Zoom in/out, preset, min/max, and reset dispatch | R5 behavioral | Operationally relied on; static relationship mapped; not newly runtime-verified by this pass. |
| `DeploymentEvents.Trigger*` live-debug/runtime-sync hooks | `mods/src/patches/parts/live_debug.cc`, installed from `mods/src/patches/patches.cc` when live query, fleet runtime sync, or fleet notifications need them | Fleet runtime observation, live-debug recent events, notifications, and sync triggers | R4/R5 native interpretation / product behavior | Grouped because the hooks share one event-source boundary; operationally relied on where configured; static relationship mapped; not newly runtime-verified by this pass. |
| `live_debug_tick(ScreenManager*)` | `mods/src/patches/parts/live_debug.cc`, reached through the frame tick subscriber when live query is enabled | File-backed live-debug request polling and read-only response generation | R4 native interpretation | Static relationship mapped; gated by `LiveDebugChannelEnabled()`; not newly runtime-verified by this pass. |
| `probe::dump_*` / `probe::search_methods` | `mods/src/probe/probe.h` | Header-only IL2CPP runtime introspection toolkit | R0 while unused; R3/R4 if invoked in-process | Static toolkit only in this inventory. No active call site was found in the reviewed patch surface; do not treat it as a safe runtime probe without a separate ledger row and approval. |

### `Digit.Prime.GameInput.ShortcutsManager.OnShipLocateAction(InputAction.CallbackContext)`

- Owner / file: `mods/src/patches/parts/hotkeys.cc`, formerly in the removed generated pointer callback guard family.
- Intended question: suppress native locate fallthrough when dispatcher-owned input should win.
- Static evidence: source/dump evidence showed the callback exists and has `InputAction.CallbackContext` shape; this is static only.
- Risk class: R5 as part of the generated guard family; R3/R4 only if revisited as a one-callback private canary.
- Confidence rung: failed before product-safe; runtime-observed does not imply safe.
- Runtime evidence: plain `Space` worked; `Shift+Space` crashed; live config secondary was `TAB`; removing logging did not stop the crash; opaque payload treatment did not stop the crash; removing `OnShipLocateAction` from the generated guard list stopped the crash.
- Payload confidence: failed/unsafe. Do not dereference payload for this seam without a new isolated proof.
- Original/trampoline confidence: failed/unsafe. Do not rely on original/trampoline behavior for this seam without a new isolated proof.
- Flag / rollback path: keep absent from product hook code and any generated guard list. Any revisit must be one-callback, default-off, and removable without touching siblings.
- Status: failed/unsafe/superseded.
- Next action: keep deleted from the product guard family; only revisit from R0/R1 with explicit approval.

### `SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS`

- Owner / file: formerly `mods/src/patches/parts/hotkeys.cc`; removed from product hook code.
- Intended question: shared suppression for native `ShortcutsManager.On*` pointer-shaped shortcut callbacks when dispatcher-owned input should win.
- Static evidence: the former macro expanded one source list into descriptors, hook functions, and install calls for `ShortcutsManager.On*` callback guards. After the quarantine patch, the macro list, shared handler, generated hook functions, and generated install machinery are absent from product hook code.
- Risk class: R5 when installed or changed as a broad generated family; individual callbacks must start lower as separate ledger entries.
- Confidence rung: static relationship for the historical family shape only. No runtime confidence for unclassified members.
- Runtime evidence: the former `OnShipLocateAction` member failed/unsafe; that failure does not classify the remaining callbacks as safe or unsafe.
- Payload confidence: none for the family. The former shared handler cast `void*` to `InputActionCallbackContext`; each callback needs its own payload confidence before payload interpretation is trusted.
- Original/trampoline confidence: unclassified for the family. The former shared handler could call `original(_this, context)` when not suppressed, so original/trampoline confidence must be tracked per callback.
- Flag / rollback path: removal/quarantine is the rollback path. Do not use a single generated-family flag as a discovery mechanism. Any future probe should enable one callback/action at a time with an explicit disable path.
- Status: removed/quarantined; not product-safe.
- Next action: keep absent from product hook code; create individual ledger entries before touching any member; do not reintroduce without per-callback ledger promotion.

## Static Enforcement

The quarantine patch adds a source-level static guardrail for product hook code:

- `SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS` must stay absent.
- `OnShipLocateAction` must stay absent from `mods/src/patches/parts/hotkeys.cc` unless it is explicitly re-promoted through a new one-callback ledger entry.
- Generated pointer callback guard install machinery must stay absent unless a future change carries an explicit allowlist/promotion marker tied to a ledger entry.
