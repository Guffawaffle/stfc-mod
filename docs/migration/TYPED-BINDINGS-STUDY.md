# Typed IL2CPP bindings: long-term study and retrieval index

## Purpose and exact source

This branch is now a reference library for how typed IL2CPP bindings were used
and extended, rather than a candidate for replacing PLAY. Preserve the useful
techniques independently of the earlier product-parity assignment.

- Study branch: `study/ripper-typed-bindings-20261026`.
- Retained implementation: `c9c03ac21999b2e9cd056f64ecbed54c96c501fb`.
- Complete pre-archive documentation checkpoint:
  `cb7cb3ba4fe28d985a2a26ed1cb219622fbb38e5`.
- Existing worktree: `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`.
- The working PLAY/SPUD branch is separate:
  `upgrade/spud-only-play-20261025`. It does not adopt these typed bindings.

Do not resume feature restoration from this index. Retrieve and qualify the
smallest useful binding/helper when a future consumer actually needs it.
Earlier parity records remain historical evidence, not this study's success
criterion. They have not been deleted or rewritten into claims of equivalence.

## Concept map

### Typed lookup is useful; checked observation is a separate contract

Start with [the runtime library](../../src/libs/il2cpp/include/il2cpp/il2cpp.h),
[its implementation](../../src/libs/il2cpp/src/il2cpp.cc), and
[the binding reference](../IL2CPP.md).

`il2cpp::Class<assembly, namespace, name>` centralizes managed identity.
`Class::Method<R(P...)>`, named fields and properties centralize resolution,
signature validation and cached access. Feature behavior stays outside bindings.
These capabilities were actively used, not merely proposed.

Default-return convenience calls still exist. They cannot distinguish missing
metadata or a contained exception from an observed zero, false or null.
`CallResult<R>` and `TryInvoke` preserve success independently of value; domain
adapters also use `std::optional` where appropriate. Neither a type declaration
nor an unchecked field read becomes evidence of a successful observation.

Examples: [Galaxy](../../src/mod/game/galaxy.h),
[navigation](../../src/mod/game/navigation.h), and
[checked ship ordering](../../src/mod/checked_ship_order.h).
Tests: [controlled binding runtime](../../tests/il2cpp_bindings.cc),
[checked ordering](../../tests/checked_ship_order_test.cc).
Prior reasoning: [gameplay/native observations](M8-GAMEPLAY-AND-SYNC.md).

### Managed lifetime, native object identity and collection shape are distinct

`GcHandle` provides move-only strong/weak custody. Weak references do not keep
objects alive; local strong roots protect managed objects across synchronous
runtime operations. Metadata validity, GC liveness and Unity destruction or
replacement are separate questions.

An apparent C++ `Array<T>*` is not proof of actual managed array shape.
`ArrayObject` and `TryReferenceArray<E>` qualify the declared getter and acquired
reference-vector shape/assignability separately. Unknown is not an empty array.
Unity component lookup may declare Component[] while returning derived elements;
validate elements without assuming a covariant runtime array representation.

Examples: [Unity helpers](../../src/libs/unity/include/unity/ui.h),
[Haven](../../src/mod/features/haven.cc),
[Haven ownership policy](../../src/mod/haven_visuals_policy.h).
Tests: [GC ownership](../../tests/il2cpp_gc.cc),
[Haven lifecycle](../../tests/haven_visuals_test.cc), controlled binding runtime.
Prior reasoning: [reference arrays](M8-PRESENTATION-PRESERVATION.md),
[Haven custody and restoration](M8-HAVEN-VISUALS.md).

### Typed signatures do not prove native aggregate ABI or exact scalar meaning

`Nullable<Vector3>` required validation of the closed generic type, flag,
payload and offsets; equal byte size was insufficient. The narrow movement
action uses the runtime invoker for its aggregate argument rather than assuming
a platform C++ calling convention. Other ordinary typed calls still use their
qualified native path; this is not a blanket runtime-invoke conversion.

Likewise, signed I4/I8 sentinels, enum storage, by-reference arguments, virtual
dispatch, full generic sharing and usable invocation entries need their actual
metadata contracts. A readable metadata record is not necessarily callable.
Avoid generalizing one action's stricter policy to every existing consumer.

Example: [navigation binding](../../src/mod/game/navigation.h).
Tests: [planned warp policy](../../tests/planned_system_warp_test.cc), controlled
binding runtime and its isolated metadata-refusal cases.
Prior reasoning: [nullable/managed action boundary](M8-PLANNED-SYSTEM-WARP.md).

### Native UI callbacks require ownership and publication, not just a delegate

The Settings work demonstrates bounded native callback adaptation, rooted
delegate/context ownership and explicit row identity. `SafeFunc`/`SafeAction`
wrappers are not themselves delegates. Validate the closed wrapper/value types
and supplied getter/setter identities; matching labels or indices is inadequate.
Retain publication/reset/stale-context fences rather than extracting a callback
constructor in isolation.

Sources: [callback adapter](../../src/mod/settings/native_boolean_callback.h),
[page bindings](../../src/mod/game/settings_pages.h),
[value bindings](../../src/mod/game/settings_values.h),
[page owner](../../src/mod/features/settings_pages.cc),
[value owner](../../src/mod/features/settings_values.cc).
Tests: [page publication](../../tests/settings_page_publication_test.cc),
[page binding cases](../../tests/settings_pages_bindings_cases.h),
[value binding cases](../../tests/settings_values_bindings_cases.h).
Prior reasoning: [Settings adapter checkpoint](PLAY-PARITY.md#settings-restoration-checkpoint).
This is a technical reference, not a claim that its entire UI matched PLAY.

### Hook transaction outcomes complement typed access; neither implies parity

[HookSet](../../src/libs/il2cpp/include/il2cpp/hook.h) and
[installation ownership](../../src/mod/installation.h) retain explicit outcomes,
readiness and an unsafe-state boundary. An indeterminate rollback is not a safe
optional-feature refusal. Quarantining owners avoids additional destructive
cleanup; it cannot repair vendor behavior that already occurred.

See [M2](M2-HOOKS-AND-FRAME.md) for exact tested revisions and limitations.
The later PLAY/SPUD upgrade demonstrates that newer SPUD is independently usable
without adopting this whole typed-binding/feature framework.

## How to reuse and requalify

1. Read the complete helper, consumer and fixture at the pinned source, not just
   a remembered API name. Separate upstream base capability from our additions.
2. Preserve missing-data semantics and lifetime/callback ownership end to end.
3. Check exact-client metadata and platform ABI. Controlled-runtime fixture
   success is not a current-game or macOS qualification.
4. Port a narrow boundary into the current product when justified; do not merge
   this entire branch or replace working features to obtain one helper.

The archive pass is documentation/branch metadata only: no builds, runtime
deployment, game cycle, settings edits, pushes or promotion. Historical builds
and runtime observations remain attached to their original source candidates.
Unrelated deployment-tooling edits in this worktree remain unstaged and are not
part of the archived implementation. Focused Downstream Lex frames provide
concept retrieval; they are continuity records, not LexRunner execution.

## Downstream Lex concept frames

All records reference study checkpoint `373a7aeea7624876afa791c115b782df1c7286ce`.
They were written to Downstream's existing scope through AXF, without Runner
execution. The shared route's automatic canonical-branch metadata may name the
anomaly checkout; each frame explicitly identifies this study branch/source.

| Concept | Frame ID |
| --- | --- |
| typed-checked-observations | `b0cc84c1-09e9-42c2-b206-fc178256aa05` |
| managed-lifetimes-and-array-shape | `bdb3d89c-8956-47f5-bc2d-176956376546` |
| abi-and-exact-metadata | `c52af62f-1019-43b7-8d88-82eacd5041c8` |
| native-ui-callback-custody | `336569d1-b292-47a7-9cce-d1702ea52e36` |
| hook-outcomes-and-typed-boundaries | `b28dd7ab-c0f8-4cf5-96bb-77365ca6fd0f` |
| study-index-and-status | `e0e29dde-257c-46c5-8df8-c456681cbfc8` |
