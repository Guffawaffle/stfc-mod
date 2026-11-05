# PLAY-OUT-OF-DOCK-POWER — station-only power condition

Frozen1baa55fe: complete buff_fixes.cc plus prime/IBuffComparer.h,
prime/IBuffData.h, config and installation. Target features/buffs.cc,
game/server.h and existing registry. Exact refs:
[cohort03](../FEATURE-COHORT-20261029-03.md).

- P1: CondSelfAtStation (native code114) returns false while
  [buffs].use_out_of_dock_power=true; original is not invoked.
- P2: Every other condition, comparer/buff object and both flags pass through
  exactly once; unknown conditions are not reclassified.
- P3: Target always installs under startup [patches].bufffixhooks and reads
  the current option live. Disable restores native condition result. This
  intentionally differs from frozen startup-only installation.
- P4: No buff/stat objects are edited, no totals recomputed or cached, and no
  timer/background worker added. Target condition logic is unchanged this pass.

The actual hook's condition routing and live disable are qualified by the
controlled service-buffs fixture. The display meaning of out-of-dock power,
current native enum/ABI installation and actual dock/deployed UI results are
not proved by those assertions. No new player acceptance or Mac claim.
