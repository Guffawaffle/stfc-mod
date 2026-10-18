# Kirshara queue protections: Mac test candidate

This downstream candidate enables Faster Queue Recovery and Thin Queue Protection
on Apple Silicon and Intel. Queue Address Guard already uses shared Mac code.
The supported protections are enabled together by default through
`control.kirshara_fixes`. The former separate feature keys (including explicit
`false` values) are ignored; legacy-only configurations log a replacement warning.
Add these keys to the existing TOML sections for testing:

```toml
[control]
queue_enabled = true
kirshara_fixes = true

[patches]
actionqueuerecoveryhooks = true
thinqueueprotectionhooks = true
queueaddressguardhooks = true
```

Restart after changing hook switches. Confirm the startup log contains
`[FasterQueueRecovery] ready=true`, `[ThinQueueProtection] ready=true` and
`[QueueAddressGuard] ready=true`.
Settings alone do not establish that hooks installed. Preserve any `unavailable`
or hook-install failure messages.

Test with the Kirshara artifact:

1. Queue same-system hostiles on ship A. Move/recall ship B from a different
   system; let A engage the next target. A's queue should survive.
2. Recall A itself, and separately clear its queue manually. Both should clear.
3. Have another player remove A's queued head target before A engages it.
   Check whether A advances to the next eligible target without a long stall.
4. Repeat through hostile respawns and with queues on two ships. Check for
   duplicate engagements, repeated failed courses, unexpected clearing or crashes.
5. Compare the combined fixes off/on through Mod Settings > Controls >
   Kirshara queue protections. This controls all supported
   protections together and saves `control.kirshara_fixes`. Both transitions
   forget pending mod recovery tracking without clearing native targets. Verify
   the saved off/on value survives a restart when convenient.

Report the mod commit/artifact, Mac architecture, game version, steps and log.
Disable the affected hook switch and restart if necessary.

## Evidence and remaining qualification

The previous Windows-only guard represented missing qualification, not a proven
Mac ABI incompatibility. Inspected Mac client 199 (`1.000.52486`) has the same
24-byte course response layout: fleet at 0, success at 8, recall at 9, target at 16.
Its callback receives that value indirectly in x1 on Apple Silicon, and on the
stack on Intel, matching each platform's C++ aggregate ABI. All seven added
detour targets have native extents exceeding SPUD's overwrite window on both
architectures. These historical binaries do not qualify a newer tester client.

Runtime metadata checks still reject incompatible method/field layouts. They do
not prove native hook fit or callback behavior on every client update. Current
Mac builds and exact-artifact player smoke remain separate qualification steps.
Native queue policy and hook platform support are unchanged by this consolidation.
Use `kirshara_fixes = false` to disable the combined behavior; hook-install
compatibility switches remain independent.
