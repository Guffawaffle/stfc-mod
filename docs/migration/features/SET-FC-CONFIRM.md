# SET-FC-CONFIRM — Fleet Commander confirmation preference

Contract v1, 2026-11-02. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`; continuation starts from
`f9e66f2428944c09f24ae338252de1eeb5dc1479`. Full source workflow spans
`mods/src/patches/parts/fc_confirmation_reset.cc` and native Settings boolean
publication/session invalidation in `mods/src/settings/native/value_widgets.cc`.
The typed preference owner is `src/mod/settings/confirmation_settings.cc`, with
bindings in `src/mod/game/settings.h`. Existing native row/delegate ownership
remains in `src/mod/features/settings.cc`.

| ID | Frozen obligation | Typed mapping |
| --- | --- | --- |
| F1 | Observe only an existing initialized PersistentPrefsManager singleton; rendering must not call a singleton factory or initialize storage. | ExistingSingleton reads validated static metadata directly, without Class::Field's static initializer. Missing/uninitialized/foreign singleton observations do not authorize action. |
| F2 | A ready owner requires loaded cloud data, no load in progress and a saved-data object. | ConfirmationAvailable qualifies required fields/GetBool. Ready keeps bounded weak owner/data identity and invalidates its generation when storage becomes unavailable or is replaced. |
| F3 | Read `options/hide_fcaa_use_confirmation` with defaultHidden=false and shouldAddKey=false. Display means the inverse of the native hidden flag. | ReadFc roots the existing owner/key and invokes the exact typed GetBool signature. Observation never inserts a preference or writes the mod's TOML. |
| F4 | Apply only to the generation previously observed and through an existing FleetCommanderManager. | WriteFc rechecks readiness/generation around manager acquisition, then calls set_HideFleetCommanderAbilityUseConfirmationHidden with the inverse display value. Missing manager rejects; failed native setter remains unverified. |
| F5 | Preference/session boundaries invalidate stale BooleanSetting snapshots and cached owner/data before native transition work. | InvalidateConfirmationSettings invalidates model sessions and backend generation. Existing PrefsBoundary wiring retains the before-original ordering; page/native publication qualification stays separate. |
| F6 | Unsupported schema, unready storage and failed reads are distinct from a known false value. | The typed adapter preserves Unsupported/Unavailable and additionally rechecks readiness/generation after a native GetBool call, refusing a read that reentered into replacement storage. |

The retained key is an immutable string root; it contains no account data.
Owner/data are weak roots, and temporary strong custody spans native calls.
This row restores the native game's confirmation preference. It is independent
of the mod's Forbidden Tech auto-confirm setting and does not activate a Fleet
Commander ability.

`tests/confirmation_settings_cases.h` includes the actual production adapter in
a private namespace, reusing the production BooleanSetting model and typed
bindings. Controlled metadata, singleton static values, GetBool/setter bodies,
feature availability and configuration sinks replace the external boundaries.
The existing shared `tow_static_field_read_override` seam supplies the two
singleton fields; it does not replace ExistingSingleton or the adapter's own
field/readiness/generation checks. Fixture runtime/configuration sinks are private
to this adapter, so FC checks cannot call other fixtures' persistence substitutes.
It uses synthetic preference objects and no real cloud storage, game account or
ability. The supplied manager setter changes only a local boolean. No new test
target or CI registration is intended; explicit `fc-confirm-*` modes run through
the existing controlled executable.

All fifteen explicit FC modes pass against unchanged production source at
`f9e66f24`, with only the consumer fixture added. The source-bound baseline is
recorded in `D:/dev/stfc-workspace/artifacts/camera-confirmation-20261102/before-cases.json`.
They cover ready/unready storage, required schema failures, noncreating/
noninserting observation, inversion and exact key, owner/data generation changes,
native-read reentry/failure, setter failure, missing manager and public
session invalidation. No FC backend correction was needed. The coordinator's
final combined rerun also passes all fifteen modes; see the
[camera/confirmation cohort](../FEATURE-COHORT-20261102-CAMERA-CONFIRMATION.md).
Native Settings row presentation, actual cloud preference persistence, current-client ABI/hook fit
and Mac runtime remain unqualified. No production behavior correction is claimed
without a reproduced discrepancy. AAI and new upstream features stay deferred.
