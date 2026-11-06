# System camera rotation

System rotation is restored as a configurable control, enabled by default. Its
earlier exclusion remains part of the frozen migration history; the default
policy change does not establish a new gameplay validation result.

`[graphics] system_camera_orbit` defaults to `true`; an explicit `false` remains an
opt-out. Community Mod → Camera →
**System camera rotation** changes it live when the orbit hooks and frame delivery
are available. The preference uses the existing settings writer.

`[patches] navigationorbithooks` defaults to `true` and controls startup hook
installation only. An explicit existing `false` is respected: enable that switch
and restart before using the live rotation preference. Changing the preference
does not install or remove hooks.

With mod shortcuts enabled, the default controls are middle-mouse drag to rotate
and tilt the system camera, and Alt + middle-mouse to reset it. The existing
`navigation_orbit_drag` and `navigation_orbit_reset` bindings remain configurable;
explicit empty bindings are respected. Native shortcut mode disables these mod
controls.

The controls apply only to system navigation. Galaxy navigation retains its
existing controls. Haven rotation is already enabled by default through
`[patches] havenzoomhooks = true` and its existing `haven_orbit_drag` /
`haven_orbit_reset` bindings, also middle-mouse and Alt + middle-mouse. No separate
Haven feature flag is added. Explicit disabled hooks, empty bindings and custom
bindings remain respected for both views. Existing backdrop appearance,
calculations and settings are unchanged. Its existing draw-distance requests
share orbit's clip owner so reset and disable restore their original baseline,
including when ordinary zoom hooks are disabled.

The settings and shortcut controls report an unavailable owner instead of treating
failed hook installation, missing frame delivery or runtime capability failure as a usable feature. Native
validation applies only to the exact artifact and client tested. Native Mac
runtime behavior remains unverified until tested there.
