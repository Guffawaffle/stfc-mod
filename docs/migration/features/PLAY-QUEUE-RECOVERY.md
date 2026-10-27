# PLAY-QUEUE-RECOVERY — vanished-target course recovery

Contract v1, 2026-10-27. [Cohort inputs and qualification](../FEATURE-COHORT-20261027-01.md).
Read frozen `parts/action_queue.{h,cc}`, ActionQueueManager, queue_recovery
Settings, testing queue switch, config/defaults/install registry and fixtures.
Target: `features/action_queue.{h,cc}`, `game/queues.h`, Config activation,
Settings catalog, typed calls/GC handles and central transaction registry.

| Obligation | Mapping | Evidence / limit |
| --- | --- | --- |
| R1 | Restart patch gate plus live kirshara_fixes AND queue_enabled. | Existing flags/defaults retained. UI/shortcut and accepted file transitions clear correlations; rejected/unrelated reloads do not. Production Config reload fixture. |
| R2 | Record latest native engagement before original; cancel only its serial if dispatch failed. | Same eight-slot, 30-second weak request store, mutex and serial semantics. Policy/store tests retained. |
| R3 | Only failed, nonrecall response with exact boxed Int64 target and original outstanding same queue is eligible. Native handler/original retry each run once. | Actual response hook/scope tested using native-body substitutes; success/recall/wrong box remain native. |
| R4 | Preserve native retry true. Override false only if target is absent from every readable queue and a matching one-shot correlation exists. | Checked count/items, qualified array/list acquisition, cross-fleet unreadability and original call tests. |
| R5 | Preserve response field order/types, exact Int32 EngageResult and distinct callback entry identities. | Shared binding-side ABI gate; malformed same-size response, unsigned result and aliased installer refusal tests. No installed game patch in these refusal fixtures. |
| R6 | Teardown clears weak handles while runtime is alive; reused addresses, old/out-of-order/expired attempts cannot authorize work. | Existing request tests and actual clear-handler fixture. Process-lifetime owner avoids late static destruction. |
| R7 | Windows/macOS host ABI and transactional all-or-none installation, no native queue/latch/retry edits. | Shared implementation, checked macOS entry fit; current-client/game and Mac qualification remain separate. |

The frozen source already has correlation limits: a response arriving only after
30 seconds cannot accelerate recovery; native handling/watchdog remains. A native
clear body that reenters engagement can create new work after the initial clear,
and a same-target response has no newly invented server request ID. These are
preserved source limitations, not proof of a new migration regression. No new
network protocol or generalized task framework is introduced.
