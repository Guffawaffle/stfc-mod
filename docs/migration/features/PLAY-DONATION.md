# PLAY-DONATION — donation quantity limits

Contract v2, 2026-11-03; exact historical baseline refs and qualification:
[cohort](../FEATURE-COHORT-20261029-01.md). Full source is `parts/misc.cc`,
InventoryForPopup, defaults, Settings, native registration and platform guards.
Present in all three inspected source baselines; target `features/shop.cc` and
`game/inventory.h` retain the existing `miscpatches` activation.

The current source reassessment starts at `5d7613d0383c91b6c051fecdc6605f707224ae0e`
on existing `play-dev-ripper`, against frozen
`1baa55fedf1758b19f843cc59707b8901a0b4998`. No donation, purchase or resource
operation is part of the controlled checks.

| Obligation | Source / target behavior | Qualification |
| --- | --- | --- |
| D1 | Windows intended scope is a tagged donation with incoming cap50; the typed getter must observe the actual bool | Actual setter fixture; unrelated values remain native; retain the boxed-value correction described below |
| D2 | Positive configured cap replaces50; nonpositive preserves the initial native cap | Zero/negative and exact large-int fixture |
| D3 | Disabled live policy or unavailable donation tag invokes original once | Existing checked getter/disabled fixtures; post-getter policy cases added for this reassessment |
| D4 | Settings remain enabled=true/max80 by default; dialog reopening applies changes | Config/Settings/source comparison; native dialog trial |
| D5 | Mac target adjusts only donation context, with owned-amount bound; original globally changed sliders50/100 | Intentional narrower behavior; native Mac validation separate |

No second quantity model or persistent owner is introduced. Settings/TOML keep
existing names and save semantics. Nonpositive Windows behavior means retaining
the game's initial limit, not assigning an invented infinite value. Mac tagging,
ownership and amount availability must be qualified independently; a Windows
setter test cannot establish Mac behavior. No donation is executed by fixtures.

## Source gate order and typed observation

Frozen `misc.cc::InventoryForPopup_set_MaxItemsToUse` evaluates the managed
IsDonationUse property before reading extend_donation_slider. The typed starting
point instead reads the enable flag before `TryCall<"get_IsDonationUse", bool>()`
and does not recheck it. If the managed boundary changes or replaces Config with
disabled policy, that callback can still override the cap; frozen's later policy
read would refuse the override. The correction retains the initial fast guard
and typed availability handling, then checks the current enable policy after the
managed observation and reads the cap from the current Config. This is a
source-order discrepancy, not an observed ordinary-play reentry report or a
claim about the native getter body's purity.

The frozen wrapper's bool accessor also returns GetRaw's boxed-object pointer
as bool. A successfully boxed false is consequently truthy there. Keep the typed
bool observation, so unrelated inventory does not acquire a donation override;
restoring raw box truthiness would undo an intentional correctness improvement.

`tests/donation_policy_cases.h` adds explicit Windows modes to the existing
controlled executable without new targets or CI registration. It uses the actual
production setter and typed metadata, with local managed getter bodies. Two
policy-off cases independently change the current Config or replace it while
leaving the old enabled object unchanged. Controls cover initial disabled policy,
nonmatching native cap, false/failed/missing tag, current positive cap changes and
zero/negative cap changes after the getter. Nonpositive behavior still preserves
the exact initial native cap; it does not manufacture an unlimited sentinel.
The preserved baseline has nine passing controls and two intended late-policy
failures. Corrected executable-bound results are recorded in the
[donation/dock cohort](../FEATURE-COHORT-20261103-DONATION-DOCK.md): all eleven
Windows modes pass on the final 423/423-step combined build. No actual client getter, UI or game resource
is used.

The non-Windows starting row has a separate bounded discrepancy: its last
`current()` check calls the managed Context getter after copying the configured
cap and checking the enable flag. A getter callback can change or replace Config
and still return the same context identity. The correction places the current
enable check and cap read after that final identity observation. It retains the
scoped donation and known-owned-amount rule; it does not restore the frozen global
slider detour.

Eight `donation-row-*` modes call the actual production row hook. Four controlled
policy changes occur only during that final Context read: disable, replace with
disabled Config, change the positive cap and change it to zero. Zero retains the
existing non-Windows rule of using the known owned amount. Controls cover a
reported replacement context, a failed final Context getter, unavailable owned
amount and the native owned-amount bound. All callbacks and writes remain local
to the fixture. The preserved Linux baseline has four passing row controls and
four intended late-policy failures. Linux exercises this non-Windows common code
path with controlled bodies; it cannot qualify native Mac hooking, dialog
rendering or the client getter's ordinary behavior. All eight corrected row modes
pass on the final 157/157-step Linux `test-il2cpp` run; exact inputs and ELF
identity remain attached to the cohort record. Both Mac core build requests stop during
configuration because the SDK is missing; no Mac source compile is established.

The historical basic Windows dialog acceptance remains attached to source
`a93e688c5db9acd23ec86d7a6e5641d70b39db02` and installed SHA-256
`0612568a5e66dfd89b707001f2e4eaf7f4e008cf5dc0ad8fc0fa17bd4eab59d9`. It does
not qualify this continuation's changed bytes or every native edge. No deployment,
cycle, live config edit, donation or resource operation occurs in this slice.

Migration remains Partial for the documented platform difference; compiled code
presence is not full functional equivalence or proof of resource handling.
