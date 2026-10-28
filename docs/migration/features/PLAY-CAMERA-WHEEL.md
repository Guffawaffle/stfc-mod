# PLAY-CAMERA-WHEEL — native UI wheel ownership without blocking pinch

Contract v1, source scan 2026-10-27. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`; current target `play-dev-ripper`
base `c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce`. Final results belong to
[cohort 03](../FEATURE-COHORT-20261027-03.md).

## Source and branch observations

Read the complete source scroll API admission, `PointerOwnsScroll`, both native
wheel hooks, Update-scoping and installer in
`mods/src/patches/parts/zoom.cc`; the native input/visibility wrappers and typed
target helpers were inspected too. This consumer exists in PLAY
`cad98ef7990dd66167eb64ea5dda525c1fe2c296` and frozen play-dev.

The scoped camera-wheel guard is **absent** at inspected upstream dev
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`. That source has camera zoom and
unrelated scrollers, but no `IScrollHandler`/first-raycast ownership consumer or
MouseScrollAsPinchRecognizer guard. This conclusion follows the consumers and
tree-wide symbol search, not the absence of a similarly named file.

Current implementation is `features/camera.cc` plus `game/scroll_input.h`.
`[patches].zoomhooks` gates the descriptor; existing terminal camera Update owns
the native consumption scope. The game still receives and interprets UI scroll
input. No global wheel suppression or new EventSystem is added.

## Behavioral obligations

| ID | Frozen source behavior | Target mapping / qualification |
| --- | --- | --- |
| W1 | The first sorted raycast hit determines scroll ownership; an occluded scroller cannot claim input through another window. | Existing typed native query retained; first-hit/list/value metadata and exact RaycastResult box remain explicit. Native hit-test trial pending. |
| W2 | Inspect parent handlers, allowing active Behaviour handlers or native non-Behaviour handlers; a disabled inner component must not hide an active ancestor. | Existing all-parent query and checked reference-vector storage retained. No source truncation or new hierarchy algorithm. |
| W3 | Block camera wheel only during that camera's native Update and only with known no-pinch plus owned UI wheel. | Existing scope/pinch distinction retained; actual production callback fixture substitutes only query results and verifies scope, unknown pinch and true pinch. |
| W4 | The station/Haven wheel recognizer returns zero only for owned nonzero wheel results, preserving original read once. | Existing recognizer callback retained and covered by controlled original/query fixture. No station/Haven layout/camera redesign. |
| W5 | Native query failure degrades to native wheel; unknown is not an observed false/true ownership decision. | Existing availability gate and failed-query latch preserved. Callback fixture covers failure/native fallback; native API admission is inspected, not represented as full UI runtime proof. |
| W6 | Mod keyboard zoom bypasses native wheel scope; scope restores through nested calls and unwinding. | Existing RAII scope retained and exercised. No second frame hook or altered keyboard callback ownership. |

## Ownership and limitations

`game/scroll_input.h` owns temporary strong custody for EventSystem, hit list,
boxed result, target, managed type and handler array/items. The feature retains
no borrowed pointer outside the synchronous native Update scope. Unknown or
malformed scalar/array/object observations never become wheel ownership. The
cached metadata and failed-query behavior are unchanged by this scan.

`tests/camera_bindings_cases.h` includes actual camera callbacks but replaces
the native UI ownership/pinch query with a named fixture facade. It proves the
consumer's use of those observations, not real EventSystem sorting, raycast cache
freshness, pooled native handlers, touch devices or native frame timing. Those
are explicit runtime checks. Fixture/build execution is recorded by the cohort
owner against the final candidate, not inferred from this contract.

Native trial: world wheel, side chat, Settings, nested enabled/disabled scrollers,
overlapping/occluded windows, keyboard zoom while hovering UI, and genuine touch
pinch if available. Keyboard and wheel need separate observations. Test actual
Windows client first, then macOS builds and Mcohall's trial. Earlier player reports
that chat wheel/keyboard worked remain historical artifact-specific evidence.
