# Settings reset and shortcut editing — 2026-11-01

Source: frozen `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target base: `2195077fcc996bea332ef22baaf5c7d0108a4c64`, existing Ripper
play-dev-ripper checkout. Contracts: [SET-RESET-SAVE](features/SET-RESET-SAVE.md)
and [SET-SHORTCUT-EDITOR](features/SET-SHORTCUT-EDITOR.md).

## Result and source comparison

No production correction was needed. Complete maintenance.h, value_settings.h,
shortcut_draft.h and shortcut_undo.h match frozen source bytes. Their existing
prepare/current/apply, revision/epoch, persistence and exact Undo contracts remain.
Current shortcut_settings.cc adapts the legacy MapKey/native popup workflow to the
existing Shortcut dispatcher, shared native text-entry owner and page-confirm
actions. It retains explicit confirmation, overlap information, release/cancel,
remove/default/Undo and adds the already-present global generation/page guards,
exact entry, unbind and exact duplicate/oversized-list recovery paths. No parallel
parser, matching engine, worker or UI owner was introduced in this slice.

The inventory's old "No native consumer" / "Parser/dispatch only" descriptions
were stale. BuildModPages registers both real controllers; typed Settings page,
value and action callbacks feed the existing text-entry/capture owner. Shortcut
availability follows actual Hotkeys installation, native shortcut mode, active
tiers and the parked orbit decision. Text entry separately requires native frame
delivery, a current page visit, invoking widget custody and capture availability.
Source's popup confirmation and target's Stage-then-page-Confirm presentation
remain different; fixture success is not a claim of identical native UX.

## Exact branch observations

| Role | Revision | Named reset and shortcut control owners |
| --- | --- | --- |
| upstream dev | `ff179589f834b95331b4d7e38b1054fb6ff5ace9` | Absent at the inspected paths |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Present |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Present |
| refactor base | `2195077fcc996bea332ef22baaf5c7d0108a4c64` | Present and registered with BuildModPages |

These observations identify named source controls, not native player acceptance.

## New bounded integration evidence

The existing real-file settings integration now prepares/cancels a reset without
changing live preference or disk, invalidates an old reset plan by session epoch,
then applies a fresh reset against an externally changed fixture. The verified
live default remains distinct from the Conflict receipt. External bytes survive;
restoring the last acknowledged fixture bytes and explicitly retrying saves the
default. Existing quit/reopen checks then continue. Only native delivery and the
separate state owner are substituted; the value model, adapter, background writer,
TOML engine, checked replacement and receipt are production implementations.

The production configuration/editor fixture now stages a shortcut and explicitly
cancels it, then stages another, leaves its page and invokes the delayed confirm.
Both paths preserve the live dispatcher and produce no save. Existing cases cover
recording/Record again, opening-key release, exact parsing, explicit confirm,
foreign global replacement, capture departure, removal/Undo/default/unbind,
duplicate and oversized player-authored lists, tiers and native shortcut mode.
Its dispatcher/editor are real; native host and persistence endpoints are controlled.

## Verification and disposition

Windows native MSVC environment, Zig0.16.0, x86_64-windows-msvc, ReleaseFast
production / Debug tests, moddbg=true, four jobs:

- `build test-settings-models test-runtime-config`: 54/54 build/run steps passed.
- `build test-il2cpp`: 163/163 steps passed, including Settings exact callback,
  category/array signatures and controlled production slider-consumer observations.

Step counts are not test counts. The native binding fixture uses a controlled
runtime, not the live client's rendered pages. Exact-client reset/capture/Undo,
hardware/focus behavior and current Mac execution remain unqualified. No live
preferences, shortcuts or files were reset/edited. No production/build/CI change,
deployment or game cycle was needed; installed source remains4498a8b8. No new
player trial gate is introduced by these limits.

Evidence: `D:/dev/stfc-workspace/artifacts/settings-reset-shortcuts-20261101/`
contains frozen sources/diffs, branch observations, production/test input hashes,
commands, cwd/base, durations, exit codes and bounded outputs. The two tests and
migration records are the only changes. Remaining native Settings interactions
and broader refactor/platform acceptance stay separate from this source/model
qualification. Preserve accepted mission/pin behavior and all parked scopes.
