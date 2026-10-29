# Ordinary warp after a manual choice — 2026-10-28

## Runtime finding

Guff reproduced normal WARP clicks opening the choice box after one Ctrl-click
on migration `8c7b3a7d`. Diagnostic checkpoint
`7e9ebb2f8d2aa6df1dfc32b8a6175ecf15fd5809` established the sequence on Windows
client 273: normal click auto-warps; Ctrl-click consumes its exact one-shot
request and retains native choice; following normal click observes modifier=false,
ask=false and empty pending intent. Its popup/course/instant-warp reads succeed,
but `PlayerFleet` is null. The label independently uses the selected fleet.

The modifier was not stuck. Frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998` fell through to its default without an
attached fleet. The migration's stronger availability guard refused a legitimate
native course shape without checking the remaining identity evidence.

## Repair boundary

Implementation: `faadd75c31e0d23c2209dad053bddb143e494207`.
[instant_warp.cc](../../src/mod/features/instant_warp.cc) retains the attached
fleet path. Only a **successful null** PlayerFleet permits recovery through its
exact positive signed Int64 FleetID and native
`FleetsManager.GetFleetPlayerData(Int64)`. The returned fleet's checked Id must
match exactly. Existing named/default policy then applies under temporary GC
custody. Revalidate the recovered course/fleet identities and popup bindings
after potentially reentrant policy and lookup calls before native dispatch.

Client 273 static evidence: `get_PlayerFleet` RVA `0x4A07B0` returns its stored
reference. `GetFleetPlayerData(Int64)` RVA `0x1173B30` delegates to FleetService
RVA `0x1804AC0`, using the player-fleet dictionary's TryGetValue. Missing IDs
return null; unavailable service state can throw, which remains a failed checked
observation. The Int32 overload instead takes a fleet index.

No selected-fleet substitution, course mutation, persistent action cache, new
hook, setting or writer. Explicit Ctrl/Command choice exits before recovery.
Failed reads, wrong types, unknown/mismatched identities and changed bindings
retain native choice. Missing names cannot rule out higher-priority filters.

## Verification

- [Actual production-consumer fixture](../../tests/instant_warp_bindings_cases.h):
  three-click sequence, repeated ordinary popups, exact IDs above 2^53, distinct
  Int64/Int32 overloads, selected-fleet independence, named policies, null/throwing
  lookup, failed-versus-null PlayerFleet, wrong types, in-place identity changes,
  lookup disappearance, reentrant popup replacement and temporary-root release.
- `test-il2cpp test-mod`: **287/287 Windows build/run steps**, Debug assertions
  with ReleaseFast dependencies. First new-fixture run incorrectly assumed no
  previous lookup; the existing missing-fleet case had already made one. Fixed
  its counter baseline, preserved the failed log, then reran successfully.
- Native Windows build: **41/41 steps**, clean source stable across the build.
- Normal AX Zig build-before-stop cycle to main ordinary game; 73 groups and
  FrameTick ready. No configuration edits. PID 45748 at qualification.
- Deployed DLL SHA256:
  `3085ae45e2bd27304cfd1503b42a52befebb228534b9a33906380d31d4984b83`.
- Guff: **“seems good”** after the requested sequence. Fresh trace independently
  shows manual ask=true, then two ordinary ask=false popups resolving the omitted
  course fleet and invoking configured warp. A later modified click again retains
  native choice. No forced resource expenditure.
- macOS build/runtime is not qualified by this Windows trial; the existing shared
  Command input and hook set remain unchanged. Exhaustive ship/filter/resource
  gameplay permutations remain outside this focused check.

Local evidence: `D:/dev/stfc-workspace/artifacts/warp-choice-diagnostic-20261028/`
contains reproduction and final trial logs, diagnostic/fix build and cycle
receipts, focused test results and candidate qualification records. No push or
promotion. The prior migration-return receipts separately show all five tracked
TOML/state hashes unchanged under
`D:/dev/stfc-workspace/artifacts/migration-return-20261028/`.
