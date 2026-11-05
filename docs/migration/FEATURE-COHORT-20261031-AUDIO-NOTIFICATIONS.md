# Audio and notification source reconciliation — 2026-10-31

Source frozen/play-dev-20261026 `1baa55fedf1758b19f843cc59707b8901a0b4998`;
historical PLAY `cad98ef7990dd66167eb64ea5dda525c1fe2c296`; upstream discovery
reference `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` unchanged. Target worktree
`D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`, branch
`play-dev-ripper`, base `41da6fc2f713552c571cc7bda88df0a094965644`.
No source baseline, AAI pin, user configuration or store format advanced.

## Complete source paths, not a replacement implementation

| Capability | Contract | Outcome |
| --- | --- | --- |
| PLAY-AUDIO-EVENTS | [Fabric filters](features/PLAY-AUDIO-EVENTS.md) | Named/all/cleanup/native delegation preserved; actual hook-body cases added |
| PLAY-AUDIO-CUES | [Cue playback](features/PLAY-AUDIO-CUES.md) | Prepared custom audio, asynchronous ownership and coalescing retained; existing dispatch/file/Settings tests rerun |
| PLAY-NOTIFY-BANNERS | [Banner forwarding](features/PLAY-NOTIFY-BANNERS.md) | Existing native visibility/audio independence and comparison arguments qualified in controlled consumers |
| PLAY-NOTIFY-INCOMING | [Incoming player alerts](features/PLAY-NOTIFY-INCOMING.md) | Current settings reacquired after native payload reads; duplicate policy unchanged |
| PLAY-NOTIFY-FLEET | [Fleet producers](features/PLAY-NOTIFY-FLEET.md) | Seven producers, availability/activation/live-mask/depletion suppression paths preserved |
| PLAY-NOTIFY-MAC | [Mac delivery](features/PLAY-NOTIFY-MAC.md) | Native platform file preserved; permission/runtime behavior remains separate |

The scan followed original audio filtering/install code, cue catalog/patterns,
local-file loader, both platform decoders/players, fleet transition/watch/OPC/
arrival/activation policies, depletion original and forwarding scopes, incoming
payload/tracker, banner hooks, localization/body selection, desktop backends,
config activation and Audio Alerts settings/save consumers. Existing destination
components remain owners; no new registry, decoder, timer, queue or framework.

Target adaptations already present include transactional banner installation,
typed game access, qualified/rooted toast parameter sources instead of arbitrary
array scanning, separate depletion/observer readiness and live desktop masks.
These are explicitly different from frozen implementation details, not silent
claims of identical failure behavior. In particular, existing fleet-watch polling
is documented, not redesigned. No feature is called player-equivalent merely
because code compiled or its hook installed.

## Reproduced lifecycle defect and bounded correction

Frozen Config was a stable singleton. Migration Config::Activate replaces its
owned Config object. HandleIncomingPlayerAlert retained a reference across
ReadIncomingPlayerAttack's managed getters and reused it for audio/desktop policy.
The controlled actual consumer fixture changes its settings provider during
payload decoding, retaining the old object so the stale choice is deterministic
without relying on freed memory. The original consumer selects the old cue;
the corrected consumer selects the current Alarm cue. Its next identical event
does not replay. Current cue is copied so immutable prepared audio has independent
custody. Native/generic banner handling and duplicate identities are unchanged.

This proves the consumer's stale-reference hazard, not that a real native getter
normally reloads settings. Payload decoding, playback and watcher subscription
are controlled substitutes. No attack, OS notification or audible sound is
generated. Desktop permissions, actual localization and account/session changes
remain native qualification boundaries.

## Offline verification

Evidence: `D:/dev/stfc-workspace/artifacts/audio-notifications-20261031/`.

- Final assertion-enabled Windows suite: **428/428** build/run/install steps.
  `regression-final.log` runs `test-il2cpp test-mod test-hud-visibility
  test-aai-host build-tests`, ReleaseFast libraries and Debug tests, Windows
  MSVC target, AAI host enabled with unchanged pinned source `4bece2e5`.
- `notification-native` executes production Fabric filtering, both banner hook
  bodies, null delegation, audio independent of hidden banners, fleet audio-only
  activation/disable/readiness, dock transition and depletion suppression scope.
- `notification-reload` supplies native payload-decoding reentry and verifies
  current cue plus duplicate suppression. Existing dispatch/files/coalescing,
  text formatting, arrival/OPC/dedup, Settings and prior migration tests remain.
- Initial fixture compilation lacked watcher type aliases, then Windows COM
  libraries; those test-host setup failures are retained separately from the
  production stale-settings reproduction. SDK-less invocation was not a source
  failure; the installed Visual Studio environment supplies native library paths.
- Important build-cache observation: after an included production `.cc` and
  fixture header changed, Zig reused the earlier compiled test host. Those
  intervening failures are stale-binary evidence, not failed correction trials.
  Updating the actual test translation unit forced compilation; the fresh run
  printed `Incoming reload cue: alarm` and passed. Final source/executable hashes
  are retained; do not validate future included-source edits by stale test output.

No Mac compile or runtime result is claimed for this new candidate. Mac CI
qualification for the earlier preset artifact is a different source checkpoint.
No player notification-delivery acceptance or overall migration parity follows
from the offline suite. Native build/startup results, if subsequently performed,
must be recorded against their exact artifacts separately.

## Native Windows checkpoint

Signed implementation `203e724a00004dceecbd0eea45cf49157deb4d21` passed the
AX Zig native build with clean/stable source, x64 proxy verification, 16 exports,
static CRT and exact embedded provenance. AAI host/UI Lab remained enabled at
unchanged `4bece2e5`. The cycle built and verified before stopping the existing
main ordinary session PID9668, then copied/verified and launched PID4432.

Installed DLL SHA256:
`c005b06b99a3e518be0d99307d7fd567365a4c477d110ad52b9081f8f8aafceb`.
New session creation FILETIME `134379033263993446`, client273, installation
`C:/Games/Star Trek Fleet Command/default/game`. This session identity becomes
historical after another restart; ordinary launch does not establish account
identity or profile isolation. Both installed and built hashes match.

Actual startup logs show **73/73** installed groups, FrameTick ready, AAI's two
native hooks and no error/critical/unavailable lines in the captured check.
The older AX boot parser counted only the two AAI `status=installed` strings;
`startup-summary.json` counts actual `+ Patching` lines instead. Normal beta and
existing `sync.officer` deprecation warnings are not installation failures.
The existing `Music_All` filter ran naturally. This does not qualify cleanup
semantics, audible/desktop delivery, incoming reentry or the full player workflow.

All five tracked TOML/state files stayed byte-identical. No config edits, preset
mutations, induced fleet action, endpoint changes or release promotion occurred.
`cycle.log`, `cycle-build-receipt.json`, `startup-log-snapshot.txt`,
`startup-summary.json`, `stores-before.json`, `stores-after.json` and
`qualification.json` retain the exact results under the evidence directory.
Guff's optional Audio Alerts/usual-notification trial is pending separately.

## Continuity and next qualification

The connected scoped preservation-first_migration@1 persona was read directly;
its five preservation/evidence duties informed source reuse, baseline-versus-
migration lifetime attribution and the cache experiment. The exact receipt is
`persona-receipt.json`. Direct source inspection, implementation and tests were
not Runner execution. No tooling or learned-rule changes were made.

Separately, Mcohall's signed Mac preset feedback accepts reordering and retaining
order after save without backing out. It remains attached to signed source
`84584fa4`, not this later candidate; architecture/client were not reported.
See [preset handoff](OFFICER-PRESETS-MAC-20261030.md).

For native qualification use the normal explicit-target AX lifecycle, build
before downtime, preserve stores, inspect hook/frame readiness, then ordinary
Audio Alerts Preview and already-configured fleet/banner behavior. Do not enable
new notifications or induce attacks/repairs/resource spending merely for a test.
Permission-denied/Mac delivery and rare incoming payloads remain separately open.
Primary-action raid work, UI redesign, generic file persistence and release
promotion remain parked. Next source group is Settings/load/save/state lifecycle
reconciliation, using existing complete code rather than an implementation rewrite.

## Bedtime stop — 2026-10-31

Guff deferred the optional Audio Alerts Preview/usual fleet/banner trial until
tomorrow. It has **not** been performed or accepted; preserve that distinction.
Stop implementation and runtime operations here. Source `203e724a` is deployed;
documentation checkpoint before this note was `17c29392`. The worktree was clean
at the stop. No further code, settings, deployment or game-cycle work is required
overnight. Prior recovery frame: `45711377-dc41-4a3f-89bb-74d5cfbde7f5`.

On return, recover this document and the machine inventory; re-observe runtime
identity before any target-dependent action, because PID4432 may be historical.
Offer the short deferred trial when convenient, then continue CFG-LOAD-RELOAD,
CFG-WRITER, STATE-STORE and STATE-QUIT source reconciliation. Use frozen
`1baa55fe` and complete original sources/callers, preserve the working owners,
formats, save receipts and failure contracts. Do not repeat completed audio
discovery or silently resume the parked raid-primary investigation.
