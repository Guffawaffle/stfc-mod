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

### `Digit.Prime.GameInput.ShortcutsManager.OnShipLocateAction(InputAction.CallbackContext)`

- Owner / file: `mods/src/patches/parts/hotkeys.cc`, formerly in `SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS`.
- Intended question: suppress native locate fallthrough when dispatcher-owned input should win.
- Static evidence: source/dump evidence showed the callback exists and has `InputAction.CallbackContext` shape; this is static only.
- Risk class: R5 as part of the generated guard family; R3/R4 only if revisited as a one-callback private canary.
- Confidence rung: failed before product-safe; runtime-observed does not imply safe.
- Runtime evidence: plain `Space` worked; `Shift+Space` crashed; live config secondary was `TAB`; removing logging did not stop the crash; opaque payload treatment did not stop the crash; removing `OnShipLocateAction` from the generated guard list stopped the crash.
- Payload confidence: failed/unsafe. Do not dereference payload for this seam without a new isolated proof.
- Original/trampoline confidence: failed/unsafe. Do not rely on original/trampoline behavior for this seam without a new isolated proof.
- Flag / rollback path: keep absent from the generated guard list. Any revisit must be one-callback, default-off, and removable without touching siblings.
- Status: failed/unsafe/superseded.
- Next action: keep deleted from the product guard family; only revisit from R0/R1 with explicit approval.

### `SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS`

- Owner / file: `mods/src/patches/parts/hotkeys.cc`.
- Intended question: shared suppression for native `ShortcutsManager.On*` pointer-shaped shortcut callbacks when dispatcher-owned input should win.
- Static evidence: the macro expands one source list into descriptors, hook functions, and install calls for `ShortcutsManager.On*` callback guards. The current source list is a generated-family risk surface after `OnShipLocateAction` was removed.
- Risk class: R5 when installed or changed as a broad generated family; individual callbacks must start lower as separate ledger entries.
- Confidence rung: static relationship for the family shape only. No runtime confidence for unclassified members.
- Runtime evidence: the former `OnShipLocateAction` member failed/unsafe; that failure does not classify the remaining callbacks as safe or unsafe.
- Payload confidence: none for the family. The shared handler casts `void*` to `InputActionCallbackContext`; each callback needs its own payload confidence before payload interpretation is trusted.
- Original/trampoline confidence: unclassified for the family. The shared handler may call `original(_this, context)` when not suppressed, so original/trampoline confidence must be tracked per callback.
- Flag / rollback path: do not use a single generated-family flag as a discovery mechanism. Any future probe should enable one callback/action at a time with an explicit disable path.
- Status: broad generated-family risk / inventory candidate.
- Next action: create individual ledger entries before touching any member; do not invent confidence for the remaining callbacks.
