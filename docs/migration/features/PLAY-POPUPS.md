# PLAY-POPUPS — shared startup popup and reward reveal policy

Contract v1; full source `parts/misc.cc`, InterstitialViewController, native
registration, configuration and Settings. Exact refs:
[cohort](../FEATURE-COHORT-20261029-01.md). Present in all inspected source
baselines; target `features/shop.cc` keeps `tempcrashfixes` ownership.

| Obligation | Preserved behavior | Qualification |
| --- | --- | --- |
| P1 | PLC offers and interstitials share one first-popup allowance per process | Actual consumers, both ordering fixtures |
| P2 | Disabled filtering neither consumes nor resets allowance | Toggle/initially-disabled fixture |
| P3 | Interstitial native show runs before CloseWhenReady; later PLC skips native show | Original/close counts and reentry fixtures |
| P4 | Reward reveal always evaluates native decision once with original ignore argument | Both return states and enabled/disabled fixture |
| P5 | Scene navigation does not reset allowance; process initialization does | Source ownership and controlled simulated-process reset |

Defaults remain first-popup filtering=false and skip-reveal=true. The original
atomic process flag is part of the source contract; retaining atomic access does
not make managed calls or Config safe for worker-thread use. Native UI work stays
on its established execution boundary. Fixtures explicitly reset only their test
process state; production never resets it on scene reload or preference toggles.
No additional ad, offer or reward action is generated. Real native ordering and
platform rendering remain distinct from controlled callback qualification.
