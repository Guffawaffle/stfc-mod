# PLAY-CONFIRMATIONS — narrow native confirmation callbacks

Contract v1; frozen `1baa55fe`, target base `266a688c`. Full source:
`parts/forbidden_tech_confirmation.cc`, `parts/reveal_confirmation.cc`, their
bindings/configuration/Settings and registration. Target:
`features/confirmations.cc`, `game/ui.h`, `game/fleet.h`.

| Obligation | Existing target behavior | Qualification |
| --- | --- | --- |
| P1 | FT confirmation only for the exact owning RequestAction closure, callback result Confirm1 | Actual consumer identity/result/disabled fixtures |
| P2 | Unrelated, null or failed callback retains native popup | Suffix-collision, missing-target and managed-exception fixtures |
| P3 | Both MessageBox overloads share one feature transaction | Source/typed signature inspection; native popup trial separate |
| P4 | Reveal auto-confirm only for Excelsior RevealHostileVger86002 | Existing native type lookup; TimeSpan/value ABI and action selection inspected |
| P5 | Disabled or unrecognized reveal leaves original arguments/action intact | Source comparison; resource-free native trial when available |

Alliance contribution is an existing target capability, not a newly restored
frozen-source consumer. Its separate preference and callback owner use Yes3,
distinct from FT Confirm1; the actual consumer fixture verifies that difference.
This pass neither enables it by default nor expands its callback match.

The source and target both fall back to a native popup if a confirmation callback
throws. That does not prove an arbitrary callback had no partial side effect
before throwing; no transactional game-action guarantee is invented here.
Fixtures invoke counted substitutes and never spend currency or activate a live
ability. Mac builds/native actions are separately qualified. Exact source refs
and evidence: [cohort03](../FEATURE-COHORT-20261028-03.md).
