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

On a raw translation, the toast's supplied nonempty array wins. Null, empty,
unavailable or invalid primary observations may fall back to the context;
sources are not merged. Retained native-body inspection distinguishes that raw
LanguageManager path from LocaleUtilities output, which already formats context
parameters. Already formatted text must not undergo a second context pass.
Manual toast substitution on that path is allowed only when context parameters
are positively null or empty; unavailable/invalid/nonempty context preserves the
native result verbatim. Surviving braces alone do not prove an unresolved slot.
Unavailable, invalid, null, empty and values remain distinct in the shared
[selection policy](../../src/mod/features/toast_text.h). Unresolved placeholders
stay visible. The shared formatter scans its input template once, so braces
introduced by its supplied values remain literal. The caller policy preserves
the distinction between raw and already formatted text instead of inferring it
from the rendered string. Managed source, context, array, current element and
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

Implementation is `70f82359bd24b5c62f52317f6a0c896c41c35106`; test-runtime
correction is `809ca38e25a1fe0d3a1a77d8f9361a739c12f71c`; the **final qualified
source** including the one-pass caller correction is
`5e58d3d7cf9ad644cf338873ecd59e7eb99726e4`.

| Clean final source lane | Result |
| --- | --- |
| Windows x64 Zig 0.16, Profiles host | 207/207 steps; 8/8 reported Zig tests; 89 artifacts |
| Windows x64 Zig 0.17, default off | 185/185 steps; 75 artifacts |
| WSL/Linux x64 Zig 0.16 | 168/168 steps; 8/8 reported Zig tests; 36 artifacts |

Both Windows lanes ran the actual production uploader against temporary owned
`127.0.0.1` listeners using synthetic data. Three inert Profiles controls ran in
the host lane. No Profiles account/preferences/catalog lifecycle fixture ran in
the developer execution environment. Negative build-option controls were not
repeated; their earlier results remain bound to their earlier source. Linux
qualified portable logic, not the Windows HTTP or product DLL runtime.

Every lane began and ended at the exact clean source. Independent readback
verified **488 unique file facts with zero mismatches**, including source,
compiler archives/executables, dependency-prefix ownership, logs, required
fixture markers and artifacts. The executed uploader fixtures matched their
retained installed bytes. Static PE checks verified exact build identities,
static CRT, 18 host versus 16 default-off exports, one compiled TOML provider,
the derived Profiles host/core closure and no Profiles libraries in default off.
No product DLL was loaded by those checks.

| Product DLL | SHA-256 |
| --- | --- |
| Host 0.16 | `99409aa765513114187a6a0cdb4600df0a40f4bb54fd1345b1e13a819e123559` |
| Default off 0.17 | `70a806a67e651fca105c1d3aa73cc2fba2679dc92a746752911f56e05c98c33a` |

Evidence root:
`D:/dev/stfc-workspace/.ax/ripper-gameplay-sync-20261025/candidates/5e58d3d7cf9ad644cf338873ecd59e7eb99726e4`.
Independent proof `independent-presentation-readback.json`, SHA-256
`a3ff0559c5e7b1ee62d3bbe5e486cde9bf72fb78a20cde50d802f904b6f9f53a`.
Private named-getter and localization-chain evidence receipts are included in
that readback; native-body findings came from a hash-verified retained Windows
client, not execution of the installed client.

The initial Linux run at `70f82359` failed its new isolated missing-array-class
case. The fake runtime's diagnostic-name stub interpreted an SZARRAY element
type as a class; the product guard was not bypassed. Routing that test stub
through its existing class lookup fixed it. The intermediate clean Linux run
at `809ca38e` passed before the separate caller-level formatting correction.
All superseded receipts remain preserved. Final results above were rerun on
`5e58d3d7`; no earlier passing result was relabeled as later qualification.

No deployment, signing, game cycle, branch promotion, live sync or macOS runtime
qualification occurred. Existing AAI, STFCKitUI and native Settings UI
integration remain deferred. Direct collaboration and scoped AXF continuity do
not claim Runner-governed execution.

## Next preservation work

Continue the inactive officer handoff experiment only after its recoverable
failure/rebind rules and actual adapter fixture are specified. Independently,
the next coherent visual cohort is Haven water visibility and road-offset
restoration, retaining the foundation's existing zoom/history geometry and
stronger replacement discovery. Private read-only source study:
`D:/dev/stfc-workspace/.ax/ripper-gameplay-sync-20261025/navigation-haven-study/NEXT-COHORT.md`.
Neither those findings nor this checkpoint establish full gameplay parity.
