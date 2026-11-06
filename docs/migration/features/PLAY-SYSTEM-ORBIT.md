# System camera rotation

System rotation is restored as a new opt-in control. Its earlier exclusion remains
part of the frozen migration history; this restoration does not establish a new
gameplay validation result.

`[graphics] system_camera_orbit` defaults to `false`. Community Mod → Camera →
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

The controls apply only to system navigation. Galaxy navigation and Haven retain
their existing controls. Existing backdrop code and settings are unchanged by
this request; no new backdrop adjustment is introduced.

The settings and shortcut controls report an unavailable owner instead of treating
failed hook installation or missing frame delivery as a usable feature. Native
Windows gameplay and native Mac runtime behavior remain unverified until tested
on the corresponding exact artifact and client.
