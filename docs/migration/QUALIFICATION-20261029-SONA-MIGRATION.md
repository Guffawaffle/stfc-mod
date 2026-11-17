# Sona migration pilot — candidate qualification

Source worktree: D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream,
play-dev-ripper, based on df56d9893baf5b3c19fd952f5e9564cfd32c9e74.
The navigation and service cohorts are checkpointed together; raid experiments
are excluded. This is implementation/controlled-test evidence, not full parity.

## What ran

- Assertion-enabled Windows suite: **424/424 steps succeeded**. Command:
  `zig build test-il2cpp test-mod test-hud-visibility test-aai-host build-tests
  -Daai-host=true -Daai-source=D:/dev/stfc-mod/third_party/stfc-aai
  -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast -Dtest-optimize=Debug
  -j4 --summary all`. Final log: regression-final.log in the evidence directory.
- New endpoint and buff modes execute actual production consumers with controlled
  metadata, config replacement and native method bodies. Baseline failures and
  final assertions are retained. This does not prove live endpoint routing or
  actual client GC/buff behavior.
- Windows native build through canonical Downstream AX:
  `.ax/ax.ps1 build --zig -ZigSourceRoot
  D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream
  -BuildMode releasedbg -Tail 18`. Exit0, source stable across build, valid x64
  DLL, static CRT and16 proxy exports verified with the exact provenance marker.
- AAI remains enabled and pinned to clean4bece2e5f1afa7c7343f009e83398742a0f86114.
- LexSona adapter selector tests3/3; fresh live MCP concurrent/default/refusal
  trial and existing scoped acceptance passed. These are direct tests, not Runner.

Native build source identity:
`worktree:bd7afcd6e57390e69ccef08943ab5b50ec881628b5709a0faa71436fde63bc2d`.
Invocation: `ax-zig-20261030T032657-2afade12`.
DLL SHA256: `bd90e6128fe12ad2061b97bb783ea3ff18c7d5b83f067edef86001a0b91d521f`.
The build was from a dirty source candidate. Subsequent qualification prose does
not have that same whole-worktree fingerprint. The compiled-file manifest and
archived receipt bind the tested implementation; do not infer it from HEAD alone.

Evidence directory:
`D:/dev/stfc-workspace/artifacts/sona-migration-fit-20261029/`.
`native-build.log`, archived ax-build-receipt.json and qualification.json preserve
the source/binary/test identities. The first build's source-stability refusal is
also retained: documentation changed while it ran. Its output was not treated
as qualified; the stable retry above succeeded. Keep the source quiet during AX
builds, including documentation. No guard was bypassed.

## Limits and next native trial

No deployment, launch, restart, configuration edit, network endpoint test or
deliberate buff crash was performed during this pilot. The runtime-context read
observed the historical diagnostic DLL on disk; no live process was selected.
It does not establish which module is in memory or a current gameplay result.
The ordinary checked-out canonical source is a different branch; use explicit
ZigSourceRoot for this candidate. All earlier player outcomes remain historical.

For a future ordinary main-game trial: build before downtime; verify the exact
installation/profile/session through the normal AX flow; check startup and buff
binding errors, shortcut-hint toggling including a newly assigned chord, search
focus/typing precedence, and ordinary retained-card behavior. Do not unpark the
raid-primary bug or change real endpoint URLs just to qualify this batch.
Endpoint reentrancy remains a controlled fixture, not a player acceptance claim.
macOS build and runtime, live failure/GC edge cases and overall parity remain open.
