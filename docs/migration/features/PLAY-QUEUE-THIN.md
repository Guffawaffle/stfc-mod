# PLAY-QUEUE-THIN — native prefix-prune recovery

Contract v1, 2026-10-27. [Cohort inputs and qualification](../FEATURE-COHORT-20261027-01.md).
Read frozen `parts/thin_queue_protection.cc`, `action_queue_guard_policy.h`,
ActionQueueManager, source recovery/settings and native fleet bindings. Target:
the existing shared `action_queue.{h,cc}`, checked typed game bindings and
transactional installer. No second recovery implementation.

| Obligation | Mapping | Evidence / limit |
| --- | --- | --- |
| T1 | Act only with both live queue flags; retain original callback once. | Live checks before candidate/after native getters; disable during confirmation fixture prevents engage. |
| T2 | Planner/watchdog must remove an exact prefix, retain same queue and idle matching fleet, with no live target latch on surviving suffix. | Existing pure predicates unchanged. Actual planner and watchdog adapters drive native method substitutes; identity, latches, truncation and mutation tests remain. |
| T3 | Reobserve exact suffix/identity/latches before normal native engagement; unavailable invocation is not success zero. | Checked Count/Item and TryCall; native exception fixture preserves original false. |
| T4 | Collect genuinely destroyed IDs before native disposal, then recheck enable, same queue, fleet/count and latches before ProcessQueue(target,false). | Restored frozen post-native checks; actual adapter tests disable, replacement, new engagement and count mutation. |
| T5 | Native disposal/process functions retain queue mutation ownership. | No direct list edits, flag writes, retry resets or independent planner in production. Process logs only after a successful native invocation. |
| T6 | Long/partially captured suffixes cannot become complete evidence. | First eight targets captured as in existing policy; truncated queues decline this optional acceleration and retain native handling. This is not source evidence deletion or a cap on gameplay queues. |
| T7 | Both platform installers use checked entry admission and transactional hook ownership. | Windows targets unchanged; shared Mac source restored, but both Mac builds and exact-client runtime are unverified. |

Source fixed-layout/capped native storage reads were replaced by checked metadata,
managed-reference acquisition and Count/Item reads. Extra post-read policy checks
are necessary because those managed getters can reenter native code. Do not
mistake the fixture's native-body replacements for runtime race qualification.
