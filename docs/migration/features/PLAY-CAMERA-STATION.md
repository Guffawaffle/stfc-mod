# PLAY-CAMERA-STATION — station camera radius restoration

Source reconciliation, 2026-10-31. Frozen play-dev `1baa55fe` and historical play
`cad98ef7` remain unchanged. Current lifecycle correction targets `play-dev-ripper`,
base `ee1127292aa04cbd934903b5638758e8e235a67e`.

Read the complete frozen `interior_zoom.cc`, shared orbit hook/constraint bindings,
current `camera.cc` and upstream fix #349 (`f9a131ea677365c8927e3bf360b4e7c30749bbe9`).
Exterior rotation/input behavior is retained. The initial radius-only correction
at `ee112729` addressed the normal shared-radius transition but omitted frozen
lifecycle protections. The frozen fork already implements shared-radius alias
lookup; describing this only as a fix outside the frozen source was incorrect.
This follow-up ports those existing protections through typed bindings.

| Obligation | Preserved behavior and correction |
| --- | --- |
| S1 | Existing `interiorzoomhooks` gate installs through the shared orbit constraint hook; no added detour. |
| S2 | In Starbase Interior, a finite `interior_zoom_scale > 1` multiplies the captured native range. Repeated updates never compound it. |
| S3 | Weak provider/constraint/radius handles distinguish collection, address reuse and replacement. Aliases of one `Constraint.FloatData` reuse one native baseline, preserving frozen semantics and the applicable upstream #349 behavior. |
| S4 | Outside the interior, or with native/invalid scale, restore all cached radii before the game's original update, including owners that no longer tick. Clear entries and release handles. |
| S5 | Restore only when the current range still equals the last mod-applied range. Adopt changed native ranges as fresh baselines. Never overwrite a subsequent native or other-feature write on exit. |
| S6 | Call the native original exactly once, then retain the existing Haven constraint adapter. Exterior arrow rotation, system zoom and Haven geometry are not rewritten. |
| S7 | Prune expired weak identities at most once per second, restoring their surviving radii. Admit at most 16 live owner entries; refuse new owners at capacity. Keys are comparison-only. Process-lifetime storage avoids handle destruction after IL2CPP teardown. |
| S8 | Refuse installation if provider constraint, constraint radius or range min/max metadata are unavailable. Never turn missing-as-zero fields into an authorized write. |
| S9 | Reject nonfinite widened ranges, conditionally restore the last finite write, and retire the entry. Failed strong/weak custody cannot authorize a new write. |

Pinned source: `1baa55fe:mods/src/patches/parts/interior_zoom.cc`, from `NativeRange`
through the update callback and `InstallInteriorZoomHooks`; shared declaration in
`mods/src/patches/orbit_constraints.h`. The adapter uses existing `il2cpp::GcHandle`
instead of the frozen raw handle wrapper and existing typed field metadata checks.
It adds temporary strong custody for the current provider/constraint/radius chain.

`tests/hud_owner_cases.h` exercises the actual production callback with controlled
native scene objects: provider aliases, exit/disable, no-longer-ticking ranges,
native replacement writes, replaced constraints, expired weak identities/address
reuse, overflow, custody failure and bounded admission. Separate process modes
exercise each missing critical field. These are local modes, not new CI registrations.
The Windows compile and existing suites passed 432/432 build/run/install steps;
all nine selected local modes passed. Exact input hashes and executable hashes are
in `artifacts/integration-audit-20261031/station-lifecycle-verification.json`
outside the checkout.
Actual station rendering, labels and repeated native interior/exterior/Haven
transitions require their own exact-artifact trial; the user's `ee112729` trial
does not qualify later lifecycle changes.
