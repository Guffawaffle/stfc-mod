# PLAY-QUEUE-ADDRESS — conservative deployed-address substitution

Contract v1, 2026-11-02. Implementation/verification in existing `play-dev-ripper`,
starting clean at `9799f67fd172592ed36f843eb75cfcbb7d3380a2`.
Frozen source remains `1baa55fedf1758b19f843cc59707b8901a0b4998`.
This is the existing queue-preservation guard, not queue scheduling or a new feature.

| Inspected local ref | Exact revision | Presence/evidence |
| --- | --- | --- |
| upstream/dev | e884efa7c6ff66d0776b4cd4d6b76ce193296be5 | Guard present in the same source/policy paths; frozen differences add Settings availability and the shared detour include, not a different address decision. Retained local ref, not a fresh remote observation. |
| origin/play | cad98ef7990dd66167eb64ea5dda525c1fe2c296 | `mods/src/patches/parts/queue_address_guard.cc` and `patches/queue_address_guard_policy.h`. |
| origin/play-dev and frozen/play-dev-20261026 | 1baa55fedf1758b19f843cc59707b8901a0b4998 | Same owner/policy, `InstallQueueAddressGuard`, metadata-qualified queue storage and live `Enabled()` checks. |
| refactor base | 9799f67fd172592ed36f843eb75cfcbb7d3380a2 | `src/mod/features/action_queue.{cc,h}`, `src/mod/game/{queues,fleet}.h`; default iterator had a second unchecked Count and nested hooks omitted live policy checks. |

## Workflow and obligations

`[patches].queueaddressguardhooks` controls installation; live
`[control].kirshara_fixes && queue_enabled` controls use. Shared Settings controls,
Ctrl-Q, config parsing/reload and existing recovery reset ownership remain intact.
The player-state event creates a thread-local scope. A matching fleet's nonempty
dock queue admits a nested address comparison. Target lookup also borrows that
fleet's own deployment once. Only the address getter inside that comparison can
substitute the deployment address. No address, action list or queue flag is written.
Nested scopes restore their caller and retain no previous-frame address cache.

| ID | Frozen obligation | Typed mapping / verification |
| --- | --- | --- |
| O1 | Disabled fix retains native handlers/results, including `PlayerAddress` and `LookupTarget` after managed reentry. | Recheck live policy after original address call and immediately before substitution; lookup also checks policy. Explicit disable fixtures. |
| O2 | Only player-state event → same fleet → matching dock slot → nonempty fully inspected target list can admit comparison. | Checked `get_Index`, qualified queue array and managed list, one successful positive Count and checked item/class/ID for every element. Failure/index and invalid-action fixtures. |
| O3 | Native handlers still run once; original comparison's return is untouched. | Actual state/mismatch/lookup/address hook fixtures, including outside-event refusal. Own deployment lookup is the sole additional native lookup. |
| O4 | Own deployment, active player/deployed states, no recall/removal, valid addresses, wrong reported planet/system and exact deployed/target address agreement are all required. | Existing `UseDeployedAddress` model preserved. Production `TryCall` acquisition rejects unavailable state/recall/removal/coordinates; legitimate false/zero remain valid. Existing policy matrix plus adapter failure cases. |
| O5 | Failure/replacement/foreign/null evidence retains reported address; subsequent healthy observations recover without remembered correction. | Existing array and field qualification, positive ID checks and scoped context; adapter refusal/recovery cases. Native replacement timing remains a separate trial. |
| O6 | Windows/macOS native signatures and all-or-none installation; no arbitrary memory offsets. | Existing typed bindings and HookSet unchanged. No new hook entry, ABI or configuration/default change. Current Mac build/runtime qualification remains open. |

## Design and evidence

The source read list storage once and validated every element. The target's
`Count() > 0 && ranges::all_of(list, ...)` read Count again inside `end()`.
A contained failure on that second read returned zero, making the empty iteration
succeed. The correction uses one checked Count and checked indexed reads, matching
the existing retry adapter's approach. An unavailable dock-index getter likewise
cannot silently select slot zero. Source storage caps (64 queues/128 actions) are
not recreated: repository policy forbids arbitrary caps; actual array bounds and
the one qualified native list count govern access.

Source's `Enabled()` checks in both nested hooks were lost in the port. Restoring
them and checking again after evidence acquisition prevents a disabled correction
from returning a substitute during reentry. This does not cancel the game's own
event or action. No queue switch redesign, new persistence, hook ownership or
diagnostic logging is introduced.

The unchanged production base independently fails `queue-address-scan`,
`queue-address-disable` and `queue-address-index` with the new controlled-host
fixtures. These call the production hooks, policy and typed accessors; managed
bodies/storage are substitutes. They do not establish real native hook fit,
garbage collection, gameplay queue retention or Mac acceptance.

Current verification/checkpoints and exact evidence are recorded in the linked
[cohort](../FEATURE-COHORT-20261102-QUEUE-ADDRESS.md). Overall classification remains
Implemented, unverified until the exact native/platform workflows are accepted.
