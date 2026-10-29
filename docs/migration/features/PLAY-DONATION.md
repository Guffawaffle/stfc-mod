# PLAY-DONATION — donation quantity limits

Contract v1; exact baseline refs and qualification:
[cohort](../FEATURE-COHORT-20261029-01.md). Full source is `parts/misc.cc`,
InventoryForPopup, defaults, Settings, native registration and platform guards.
Present in all three inspected source baselines; target `features/shop.cc` and
`game/inventory.h` retain the existing `miscpatches` activation.

| Obligation | Source / target behavior | Qualification |
| --- | --- | --- |
| D1 | Windows: only tagged donations with incoming cap50 are changed | Actual setter fixture; unrelated values remain native |
| D2 | Positive configured cap replaces50; nonpositive preserves the initial native cap | Zero/negative and exact large-int fixture |
| D3 | Disabled or unavailable donation tag invokes original once | Actual checked getter/disabled fixtures |
| D4 | Settings remain enabled=true/max80 by default; dialog reopening applies changes | Config/Settings/source comparison; native dialog trial |
| D5 | Mac target adjusts only donation context, with owned-amount bound; original globally changed sliders50/100 | Intentional narrower behavior; native Mac validation separate |

No second quantity model or persistent owner is introduced. Settings/TOML keep
existing names and save semantics. Nonpositive Windows behavior means retaining
the game's initial limit, not assigning an invented infinite value. Mac tagging,
ownership and amount availability must be qualified independently; a Windows
setter test cannot establish Mac behavior. No donation is executed by fixtures.

Migration remains Partial for the documented platform difference; compiled code
presence is not full functional equivalence or proof of resource handling.
