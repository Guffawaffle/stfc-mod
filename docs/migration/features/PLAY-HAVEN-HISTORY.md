# PLAY-HAVEN-HISTORY — help history reversal with managed custody

Contract version: 1. Status: implementation; controlled-runtime validation passed, native qualification pending.

## Inputs and scope

Source: `frozen/play-dev-20261026` at `1baa55fedf1758b19f843cc59707b8901a0b4998`.
Target: existing `play-dev-ripper`, base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c`,
plus `features/haven.cc` and `tests/haven_native_cases.h` working changes.

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | config/gameplay consumer | `mods/src/patches/parts/haven_history.cc`; snapshot lacks persistent roots across setters |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | control/catalog and consumer | Complete rooted snapshot, same frozen behavior apart from SPUD include |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | control/catalog and consumer | `ReverseRecords`, config/defaults/example, `settings/upstream_settings.cc` |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` plus scoped working changes | control/catalog and consumer | `features/haven.cc`, `game/haven.h`, `settings/config_settings.cc` |

Scanned 2026-10-28. Shared Windows/macOS workflow. Restart-only
`patches.havenhistoryhooks=true`; live `ui.reverse_haven_history=true`, taking effect
on the next native list application/reopen. Persistence belongs to the settings owner.

## Source workflow and ownership

Complete pinned source: `mods/src/patches/parts/haven_history.cc`, with `config.h`,
`config.cc`, `defaultconfig.h`, `patches/patches.cc`, English example and
`settings/upstream_settings.cc`. No pinned feature-specific history test exists.
`PlanetaryHelpHistoryContext.ApplyRecords` calls reversal before native application.
It first reads every record, rooting each non-null reference. Only a complete acquisition
can begin the forward managed setter pass. The native original runs once regardless of
ordinary observation/setter failure. No list or record is owned after the callback.

## Behavioral obligations

| ID | Pinned source behavior | Target mapping | Verification |
| --- | --- | --- | --- |
| O1 | Disabled/null/empty/singleton paths preserve native delivery | Reuse, with checked count and a rooted list owner | `haven-history-native` disabled/empty/singleton cases passed; null-list guard is source-inspected |
| O2 | Acquire every row before any write; failed getter/root acquisition performs no setters | Restored source snapshot logic via `TryCall` and `GcHandle::Strong` | Actual callback getter/count/row-root failure cases passed with unchanged input |
| O3 | Keep all original records rooted throughout every managed setter, including known-null records | Complete rooted vector, managed setters retain write barriers | Entire-original-cohort and list custody observed during every setter; passed |
| O4 | Publish reversed order through native setters, then native original once with the original list identity | Source forward write pass retained | Many/null-record, 2001-row and exact-original-count cases passed |
| O5 | Stop after a failed setter and retain native fallback | Checked setter outcome; no invented successful reversal | Injected second-setter failure preserved source-compatible partial state and native delivery; passed |

## Design and behavior decisions

The base target swapped pairs through default-return accessors and unconditionally continued
after errors. That could publish null for an unavailable getter and mutate before discovering
a later read failure. The working correction restores the source's full acquisition phase
and fail-fast setter behavior. A successful null remains distinct from an unavailable read.

The source skips counts greater than 2000. The target intentionally removes this arbitrary
cap under the repository's instruction not to cap game-reported counts. A 2001-row fixture
records that decision. Negative/zero/singleton counts never authorize mutation. As in the
source, setter failure can leave a partially reversed list; no rollback or atomicity is claimed.

## Validation evidence and recovery

Source inspection and `git diff --check` completed. The new fixture includes the actual
production callback, with controlled native list methods and live root bookkeeping.
It observes custody during the setter interval; it does not claim a real GC collection,
native detour installation or player UI verification. The implementing lane performed no
build or execution; coordinator qualification followed on the settled inputs in
`D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/source-inputs.json`.
See [FEATURE-COHORT-20261028-02](../FEATURE-COHORT-20261028-02.md).

Coordinator `tests-3.log` reports 416/416 steps succeeded on Windows x64, including
`haven-history-native`, all ten Haven modes and the existing visual fixture. Explicit
embedded and custom background integration variants each passed 302/302 steps
(`tests-embedded.log`, `tests-custom-build.log`). Signed source and product-build evidence
are separate coordinator records, not implied by these controlled-runtime executions.

Source mapping and current controlled-runtime checks are complete for this workflow.
Exact-client hook fit, native/player UI behavior and both Mac architectures remain pending.
Next: compare empty/single/many rows and the live setting toggle on the exact product artifact.
