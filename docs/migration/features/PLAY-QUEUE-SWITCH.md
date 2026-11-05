# PLAY-QUEUE-SWITCH — live queue enable and persisted toggle

Contract v1, 2026-11-02. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target continuation starts from
`f9e66f2428944c09f24ae338252de1eeb5dc1479`. Full source workflow spans
`parts/testing.cc::IsQueueEnabled`, `parts/hotkeys.cc`'s ToggleQueue branch,
`settings/upstream_settings.cc`, configuration/defaults and queue recovery reset.
Current owners are `features/fleet_actions.cc`, `features/hotkeys.cc`,
`settings/basic_gameplay_settings.h`, `settings/config_settings.cc` and config
reload. This closes the switch's source contract without changing scheduling.

| ID | Frozen obligation | Existing typed behavior |
| --- | --- | --- |
| Q1 | Live control.queue_enabled=false makes IsQueueUnlocked return false without calling native; enabled forwards native result. | ActionQueueManager_IsQueueUnlocked_Hook uses the same short-circuit. A mod setting cannot unlock a natively locked queue. |
| Q2 | ToggleQueue flips the live value, persists control.queue_enabled and resets stale recovery authorizations. | FleetShortcuts uses ToggleBoolean's existing Queue identity, registered SaveSetting boundary and ResetQueueRecoveryTracking, then consumes the shortcut. Default binding remains Ctrl-Q. |
| Q3 | Toggling retires recovery identity without deleting native queued targets or outstanding responses. | ResetQueueRecoveryTracking retains that narrow ownership; current queue/replay protections consume live settings separately. No extra native clear/engage operation is added. |
| Q4 | The native Settings boolean mutates the same value, persists it and resets recovery tracking. | Existing catalog row is Live, with the shared queue capability availability and reset observer. Reload also resets recovery tracking when queue_enabled or kirshara_fixes changes. |
| Q5 | Default queue_enabled is true. IsQueueUnlocked belongs to the restart-only testpatches installation group. | Existing Config reader/default, QueueSwitch descriptor and HookSet retain those identities. Live enable is distinct from hook installation and hotkey input mode. |
| Q6 | Ordinary shortcut focus/mode precedence remains with the main hotkey dispatcher. | FleetShortcuts remains in its existing ordered dispatch after the shared input gates; changing queue enable does not bypass capture, text focus or native-hotkey modes. |

This feature installs no additional frame hook or new native entry. Queue
recovery/address guards remain distinct owners; their current stricter
availability, identity and reentry checks are preserved. See
[PLAY-FLEET-ACTIONS](PLAY-FLEET-ACTIONS.md) and
[PLAY-QUEUE-ADDRESS](PLAY-QUEUE-ADDRESS.md). No config schema, default, queue
format, action policy or platform guard changes are required by this assessment.

Existing `fleet-native` cases in `tests/fleet_actions_bindings_cases.h:171–190`
drive the production shortcut and unlock callbacks. They verify the registered
section/key/value, retirement of an old recovery root, disabled suppression,
enabled native forwarding and preservation of native false. Existing
`tests/basic_gameplay_settings_test.cc` exercises the shared queue mutation with
the real registered writer, including saved true/false, external conflict and
retained live intent during reload. Existing queue-native/address fixtures
separately exercise live disable during callbacks; those are not new switch
tests or native gameplay trials. The coordinator records the current reruns
with exact source and executable inputs.

Guff has accepted normal queue workflows, waves and recovery on the running
build. That player evidence remains attached to its tested artifact; no repeat
normal queue trial is requested. Controlled callbacks and file checks do not
establish every rare recovery race, exact-current native hook fit, restart/store
equivalence or Mac acceptance. This is source/evidence closure of an existing
switch, not a claim of complete queue or migration parity. AAI and parked queue
science remain deferred.
