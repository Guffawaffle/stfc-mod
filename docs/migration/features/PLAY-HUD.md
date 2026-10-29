# PLAY-HUD — four mission HUD visibility controls

Contract v1; frozen `1baa55fe`, target base `266a688c`. Full source:
`parts/mission_hud_tweaks.cc`, `patches/mission_hud.h`, configuration, Settings and
five hook callsites. Target: `features/missions.cc`, `game/hud.h`,
`mission_hud_policy.h` and existing HUD policy tests.

Q Trials, Field Training, Outposts and Missions independently use Auto/Always/Never.
Auto preserves native decisions; changing back to Auto refreshes native setup.
Missions has no corresponding native setup method, so its measured baseline is
retained/restored for that exact button. A pooled/replaced button must acquire
its own baseline. Missing observations cannot certify successful restoration.

All five native setup/lifecycle seams remain installed together: OnEnable,
SetupAchievementsButton, SetupChallengesButton(Boolean),
SetupOutpostsButton(Boolean), HandleOutpostsAndChallengesHUD. Native code runs
first, including the planetary/outpost branch; only explicit preferences override
it. Runtime Settings availability and refresh use the existing installed owner.

The target's weak registry, strong operation snapshot, reentry guard and deferred
owner admission already strengthen the source. This pass preserves that code.
Fixtures exercise actual application, native-first delivery, return-to-Auto,
replacement baseline and idle service; pure restoration/refresh fixtures remain.
Managed substitutes do not prove actual hierarchy, frame cost or rendered layout.

Shared desktop behavior; exact refs, source-input identities, final tests/build
and native qualification: [cohort03](../FEATURE-COHORT-20261028-03.md).
