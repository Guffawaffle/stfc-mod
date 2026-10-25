# Gameplay presentation preservation

This continues [the gameplay/sync checkpoint](M8-GAMEPLAY-AND-SYNC.md) in the
existing isolated migration worktree. It is not a Tactical Console or STFCKitUI
redesign. The immutable downstream preservation input remains
`45d1deaffddc3706e5c8232cf746ca46ef67be91`; queue saving was separately qualified
at `5fbb1ec915acad9e242af3eb2cfe948a2e8c78b9`.

## Shared reference-array boundary

[IL2CPP bindings](../../src/libs/il2cpp/include/il2cpp/il2cpp.h) now distinguish
two checks that a C++ array pointer alone cannot establish:

1. A named getter declares a managed array reference before invocation. The
   separate `ArrayObject` facade binds `System.Array`; it does not change the
   signature policy of every existing `Array<T>` consumer.
2. `TryReferenceArray<E>` checks the acquired object's actual SZARRAY shape,
   reference element kind and one-way element assignability before traversal.

This uses existing runtime metadata APIs and the pinned SDK's documented
SZARRAY element-type union. No runtime import or macOS required-export list is
added. Unknown metadata is unavailable, not an empty array. Successfully
observed null remains null; the shape helper cannot establish whether an
earlier unchecked read succeeded. Primitive, pointer, by-reference, bounded or
multidimensional outer arrays are rejected.

The helper qualifies reads. It does not authorize covariant writes through
`Array::Set`, nor establish a subclass identity for an unbound object wrapper.
The new consumers retain managed roots locally and read only. Existing generic
method matching remains unchanged, including its deliberate pointer conventions.

[Controlled-runtime tests](../../tests/il2cpp_bindings.cc) exercise real binding
resolution and shape checks. These are fake metadata/runtime observations, not
proof that a particular client populated a native field or that live hooks fit.

## Mission HUD: return to native choices

The existing [missions feature owner](../../src/mod/features/missions.cc) still
installs its five original-first hooks in one transaction. It now retains weak
controller/button identities and a checked Missions visibility baseline. An
override records its restoration obligation before attempting a native write;
setter success alone cannot acknowledge restoration. Returning to Auto clears
that obligation only after matching readback on the same button. A replacement
button receives its own baseline, never the retired object's value.

`mission_hud::Refresh` holds a strong owner snapshot, requests native achievement
setup followed by combined outpost/challenge setup, rechecks identity/liveness
and enabled state after each, then reapplies current choices. Recursive Refresh
is declined. Reentrant owner admission is deferred through a single weak batch;
new work waits for a later drain instead of recursively extending the operation.
The existing feature's frame callback only drains pending work and performs no
idle owner scan. No second shared update hook is introduced.

[HUD-specific policy](../../src/mod/mission_hud_policy.h) is shared with
[the fixture](../../tests/hud_visibility_test.cc), covering restoration,
side-effecting/throwing failures, owner replacement, collection, reentry and
deferred batching. Native registry and Unity liveness observations are modeled
at the fixture seam; they are not an in-game qualification. Public readiness
and Refresh hooks are available for the deferred Settings UI without adding it.

## Toast parameter fallback

[Toast bindings](../../src/mod/game/ui.h) use named `TextParameters` getters on
Toast and LocaleTextContext, both present in retained client metadata. The
[existing toast owner](../../src/mod/features/toasts.cc) remains the only hook
owner; there is no arbitrary field scan or value-array reinterpretation.

The toast's supplied nonempty array wins. Null, empty, unavailable or invalid
primary observations may fall back to the context; sources are not merged.
Unavailable, invalid, null, empty and values remain distinct in the shared
[selection policy](../../src/mod/features/toast_text.h). Unresolved placeholders
stay visible. The shared formatter scans its input template once, so braces
introduced by its supplied values remain literal. Earlier native localization
may already have substituted text before that input reaches the fallback; the
fixture does not establish that earlier text's provenance. Managed source,
context, array, current element and
conversion-string custody is local to the operation, with no permanent roots.

[Pure selection tests](../../tests/toast_parameters_test.cc) reuse that actual
policy and the existing formatter. They do not invoke desktop notifications,
game localization or live managed allocation. Parameter population, language
quality, native attachment and macOS notification delivery need separate trials.

## Officer presets: policy qualified separately from native activation

[The pure policy](../../src/mod/officer_preset_order.h) preserves SlotId zero and
exact int64 identities, fixed empty/null rows, canonical OrderId invariants and
adjacent filled-row movement. It returns a presentation permutation and copied
session order; it never rewrites native identities or preset contents.
[Its fixture](../../tests/officer_preset_order_test.cc) checks these independent
rules, including duplicate refusal and retained stale saved IDs.

There is deliberately no registered native officer-reorder feature yet. Static
inspection of a hash-verified retained client disproved a data-only handoff:
native save updates `_data[OrderId]`, then looks up a visible receiver through a
separate scroller index map. Merely swapping the data pointer could bind the
replacement preset into a different displayed row. Repainting afterward does
not establish that this was safe.

Two candidates remain: verified canonical rebuilding of both data and receiver
mapping, or a narrowly scoped canonical-index lookup translation during save.
Both require checked handoff/restoration and reentry qualification before any
activation. Failed mutation must not silently skip/retry native save, suppress
its exception or claim successful repair. Keep this safety gap explicit rather
than count a passing pure policy test as restored gameplay functionality.

Private retained evidence and adapter proposals are under
`D:/dev/stfc-workspace/.ax/ripper-gameplay-sync-20261025/officer-preset-study`.
No user preset/store was read or modified by the pure fixture.

## Qualification checkpoint

Development-only fixture results are not final source-to-binary qualification.
Final clean-source gates, exact revisions and independent artifact readback are
recorded here once available. No deployment, game cycle, branch promotion or
live sync is authorized by this implementation. Existing AAI, STFCKitUI and
native Settings UI integration remain deferred.
