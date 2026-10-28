# PLAY-FLEET-ABILITY-RECOVERY — expired native ability-button repair

Qualification update: the 382-step Windows suite and native build/boot passed at
`b36d2837`; naturally occurring stuck-cooldown recovery remains unobserved.
Exact evidence and limits: [cohort 04](../FEATURE-COHORT-20261027-04.md).

Contract v1, source scan 2026-10-27. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper` base
`557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8`. Cohort 04 records final candidate
and source/binary-bound execution evidence.

## Source, caller and scope

Read frozen `mods/src/patches/parts/hotkeys.cc::UpdateExpiredAbilityButtons` in
full, frame caller/early returns, fleet/bar/panel bindings, GenericButtonContext,
GenericButtonWidget, Widget, Canvas and ability-manager helpers. Source tests were
searched; no focused ability-repair fixture was found. Existing target was already
`src/mod/features/fleet_actions.cc::AbilityButtonsFrame`, delivered through the
single frame owner as `kAbilityButtons` under `[patches].hotkeyhooks` at that
historical checkpoint. The dedicated switch below supersedes that shared gate.

| Inspected branch | Exact revision | Source observation |
| --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | `UpdateExpiredAbilityButtons` and frame caller in `mods/src/patches/parts/hotkeys.cc` (518/716). |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Same repair/caller (505/727). |
| frozen play-dev | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete workflow (521/747), including capture exclusion and native/disabled-hotkey independence. |
| refactor | `557a6bd2e31b5a4ea6f8c5f8ae50665bd0ca66f8` | Existing typed 250 ms repair; this pass qualifies unknown values, arrays and stale sources. |

## Current configuration

```toml
[patches]
abilitybuttonrecoveryhooks = true
```

This startup switch defaults **on**, including when absent from an existing
configuration. Changing it requires a game restart. Set it to `false` to disable
only expired ability-button recovery. It is independent of `[patches].hotkeyhooks`
and `[control].hotkeys_enabled`; ordinary hotkey mode switches still leave repair
available. There is no separate live/in-game toggle. Object tracking and the
shared frame tick must be available for recovery to observe and update a panel.

`FleetActionMaintenance` retains the original `hotkeyhooks` gate and frame
position for fleet replay, cargo, tow and repeat-selection cleanup. Its null
installer preserves registration even if a later optional Hotkeys installation
fails. `AbilityButtons` follows with its dedicated gate and unchanged 250 ms
throttle/capture exclusion. The registry now has 72 feature descriptors; this
adds no native detour or second frame owner.

This repairs a stuck disabled control; it never invokes an activated ability,
spends resources, reconstructs combat mechanics, or replaces native eligibility.
The switch adds no store migration and does not rewrite existing TOML files.

## Behavioral obligations

| ID | Frozen behavior | Adaptation and verification |
| --- | --- | --- |
| AR1 | At most once per 250 ms, before native/disabled-hotkey early returns, after capture exclusion. | Existing Throttle/frame owner retained. Unthrottled `RecoverAbilityButtons` is the real consumer called by that owner, allowing deterministic tests. Capture exclusion restored; maintenance/cancellation still run. |
| AR2 | Newest bar's selected fleet; require `CurrentState & CanActivateAbility`. | Same mask, qualified state/observed ID and rooted bar/panel/fleet. Unknown state does not authorize mutation. |
| AR3 | Ability manager, visible canvas, no active ability effect. | Same native authorities with checked results. Failed effect query is unknown rather than inactive. Visibility/effect checked before publication. |
| AR4 | Parallel IDs/buttons iterated only through shorter length. | Same matching/order; exact signed Int64 SZARRAY IDs and typed reference buttons checked before header access. Arrays rooted; no arbitrary cap. |
| AR5 | Button/context exist, button active, context non-interactable. | Root references; checked active/interactable. Unknown is not false; already enabled controls untouched. |
| AR6 | Cooldown at most -10,000,000 ticks (-1 s); native `CanUseAbility(fleet,id)` true. | Runtime TimerDataContext tolerates assembly movement, but value must be exact System.TimeSpan of expected size. Native readiness remains authoritative. Threshold and signed 64-bit ID unchanged. |
| AR7 | Only `GenericButtonContext.Interactable=true`. | Same setter after rechecking fleet/panel, arrays/index/button/context, renewed cooldown and current native state. Failed readiness is never transferred to another ability/fleet. No activation call added. |

## Decisions and ownership

Prior target default-valued effect/interactability reads could reinterpret
unavailable bindings as inactive/disabled. Array facades did not qualify actual
storage. Frozen source checked methods/exceptions but likewise lacked complete
array/lifecycle proof. Bounded checked adapters preserve meaning without treating
unavailable observations as usable zeros or reinstating raw offset access.

All sources, arrays, timers and contexts have operation-local custody. Nothing
from a repair survives to the next 250 ms pass. Native eligibility can reenter:
replaced fleet IDs, panels, vectors, button contexts and renewed cooldowns cannot
consume earlier readiness. No extra hook, worker, scheduler or polling registry.
Other queue/cargo/towing frame work keeps its existing owners.

Native-hotkey mode and mod-hotkeys-off still receive repair. Text focus alone
does not disable it, matching source; shortcut capture does. Ordinary keyboard
actions remain excluded in those modes.

## Verification and native procedure

`tests/fleet_selection_ability_bindings_cases.h` supplies controlled native bodies
around the real consumer. `fleet-ability-native` covers the -1 s threshold,
unmatched array lengths, Int64 IDs beyond double precision, already-enabled/
inactive/hidden states, unavailable getters, input modes, actual wrong-shape
vectors, fleet/button/ability replacement, renewed timer and root release.
`fleet-ability-id-field` and `fleet-ability-timer-type` reject wrong declared
arrays and unrelated eight-byte values without invoking the timer body. These
are production-adapter tests, not a rendered native panel or actual cooldown race.
Cadence is retained by source inspection; no sleep-based scheduling claim.
Coordinator owns executed results/builds.

In-game: after a naturally expired cooldown, check a previously disabled button
becomes usable only when native eligibility permits. Switch ships, hide/reopen
the panel, use native/mod-off input modes, and open a shortcut editor. No forced
activation or resource expenditure needed. Ordinary play may not reproduce a
stuck-button bug; its absence does not prove recovery fired. Exact-client
frame delivery, macOS builds and runtime testing remain separate qualifications.

Disposition: bounded source-preserving implementation/fixtures authored;
test/build/runtime acceptance belongs to the exact final cohort candidate.

## Dedicated-switch verification — 2026-10-27

The follow-up changes only the startup gate and separates existing frame
housekeeping from the repair callback. The Windows suite passed **384/384**
build/run/install steps with AAI enabled at its unchanged pinned revision.
`tests/mod_logic.cc` exercises the real parser's default-on behavior, effective
output and all four combinations with `hotkeyhooks`. `tests/config_reload.cc`
confirms reload cannot change the startup decision.

The controlled-runtime `fleet-ability-gates` case models the central host's
startup mask and invokes the actual recovery callback: disabled repair makes
no cooldown query or mutation; enabled repair works with the hotkey group off.
`discovery-tow-maintenance` invokes the actual housekeeping callback with repair
disabled and confirms pending native tow cancellation and root release. These
cases do not execute the real game's feature installer or establish rendered
button recovery. Existing availability, stale-source and lifecycle cases passed.

Evidence: `D:/dev/stfc-workspace/.ax/ability-recovery-switch-20261027/tests.log`
(SHA256 `6d9fcbdaa55b5c55d37345e01e0e25a89715b4804359488093d74a7139cfecde`);
controlled-runtime executable SHA256
`ea464a075d7452fd70ec5f5332d91ed34150d531c7f57b61fc184d42a5e9107a`.
Default-on native boot qualification will be recorded against the exact build;
disabled-switch native execution, a naturally stuck button and macOS remain
unverified. No runtime TOML edit is required for the default-on trial.
