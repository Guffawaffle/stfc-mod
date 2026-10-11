# Kirshara queue protections: Mac test candidate

This downstream candidate enables Faster Queue Recovery and Thin Queue Protection
on Apple Silicon and Intel. Queue Address Guard already uses shared Mac code.
Faster Queue Recovery is enabled by default downstream. Existing explicit `false`
settings are preserved; add these keys to the existing TOML sections for testing:

```toml
[control]
queue_enabled = true
faster_queue_recovery = true
thin_queue_protection = true
queue_address_guard = true

[patches]
actionqueuerecoveryhooks = true
thinqueueprotectionhooks = true
queueaddressguardhooks = true
```

Restart after changing hook switches. Confirm the startup log contains
`[FasterQueueRecovery] ready=true` and `[ThinQueueProtection] ready=true`.
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
5. Compare Faster Queue Recovery off/on through its live Mod Settings control.
   Thin Queue Protection is independent; restart to compare its TOML setting.

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
Queue policy is unchanged. Faster Queue Recovery now defaults to enabled downstream;
upstream code and defaults are unchanged.
