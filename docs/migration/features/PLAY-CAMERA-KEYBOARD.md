# PLAY-CAMERA-KEYBOARD — pointer-centered keyboard zoom

Contract v1, source scan 2026-10-27. Source frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`; target `play-dev-ripper`
base `c9b2e1cfb2dbaf15c67b175ae7ca563936a49cce`.
[Cohort 03](../FEATURE-COHORT-20261027-03.md) owns final candidate qualification.

Read complete zoom Update, MathUtils/NavigationZoom wrappers, MapKey/key/input
helpers, shortcut dispatch/defaults and camera settings in the frozen source.
Current code is `features/camera.cc`, shared typed navigation/Unity input bindings
and the existing Shortcut capture owner. The original zoom consumer exists in
`mods/src/patches/parts/zoom.cc` at inspected upstream dev
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`, PLAY
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`, and frozen play-dev. Current source
presence does not establish native input parity.

## Activation and obligations

Default Q/E zoom while held at `graphics.keyboard_zoom_speed * deltaTime`, with
the pointer location/world point as pivot. `control.hotkeys_enabled`,
`control.use_scopely_hotkeys`, text focus, capture and configured shortcut tiers
retain their existing meanings. The Settings keyboard-speed control remains
owned by Zoom; no new input handler or page is introduced.

| ID | Source behavior | Target mapping / verification |
| --- | --- | --- |
| K1 | Poll current keyboard state before reading keys; held Q/E have signed steps scaled by frame delta. | Existing polling and shortcut resolution reused. Callback fixture supplies held actions and current finite delta, checking both signs and native command counts. |
| K2 | Text focus and native-hotkey mode prevent mod keyboard zoom; capture owns its keys. | Update focus gate, control gate and existing Shortcut capture guard retained. Fixture uses actual focus helper and controlled Shortcut capture sink. |
| K3 | Read pointer and native camera world point, then invoke native zoom; no fabricated origin or alternate movement algorithm. | Checked mouse/scene/world observations and owned native references replace default-valued reads. Existing qualified world conversion is shared with Galaxy rather than copied. |
| K4 | Absolute preset/default uses qualified inverse; relative steps update native deltas. | Field admission and range qualification before mutation, checked distance setter/zoom invocation. Native failure does not trigger a second option or successful session-default publication. |
| K5 | UI wheel ownership must not suppress mod keyboard zoom at the pointer. | Mod command remains outside `NativeZoomUpdateScope`; wheel callback fixture verifies its original remains reachable even when a UI scroller owns the wheel. |
| K6 | Disabled/unavailable observations leave native camera behavior available. | Invalid/nonfinite pointer, world, range or delta skip mod injection. Native Update still runs except a successful explicit store as required by the source. |

## Boundaries and validation

Native negative authored speed values are not rewritten or silently clamped.
Missing/zero frame delta causes no movement; negative or nonfinite frame duration
is not treated as a meaningful forward-frame step. Valid finite commands retain
the source formulas and direction. Original callback ordering and the shared
terminal NavigationZoom owner remain unchanged.

The actual camera callback fixture substitutes native camera bodies and bounded
input/save sources. It is not a real key-layout, native device, Unity frame or
rendering test. Source comparison, fixture execution, build, launch and player
confirmation must stay distinct. Adjacent pan, momentum, backdrop, station/Haven
and excluded system-orbit behavior are preserved rather than counted as this scan.

Native trial: Q/E at several pointer positions over world and side chat; focus a
text field or shortcut editor, switch native hotkeys and disable mod hotkeys.
Check configured chords and preset/reset precedence. Verify ordinary mouse wheel
and touch pinch independently. Repeat with supported macOS key conventions after
both architecture builds and Mcohall's native trial.
