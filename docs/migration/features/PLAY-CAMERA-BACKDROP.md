# PLAY-CAMERA-BACKDROP — system planet/backdrop scale

Contract v1, 2026-11-02. Frozen source is
`1baa55fedf1758b19f843cc59707b8901a0b4998`; refactor starting point is
`f9e66f2428944c09f24ae338252de1eeb5dc1479`. Frozen ownership is
`mods/src/patches/parts/zoom.cc`; typed ownership is
`src/mod/features/camera.cc`, with `PlanetViewUtils` and `PopulatedSystemData`
bindings in `src/mod/game/navigation.h`.

The existing `Zoom` descriptor and restart-only `[patches] zoomhooks` gate own
the callbacks on both platforms. `[graphics] fr_scale` retains its default 2.0.
The Settings control remains restart-timed: a new live restoration mechanism is
outside this preservation task. Source off/native values leave authored content
alone; they do not restore transforms that the mod already multiplied.

| ID | Frozen reachable behavior | Typed contract and preservation |
| --- | --- | --- |
| B1 | ScaleFR skips null renderables, unavailable transform/scale and factors <=0 or ==1; otherwise multiplies all three axes. | Keep actual zero distinct from unavailable reads. Qualify declared Vector3 identity and the returned box before copying a scale; nonfinite input cannot authorize an invalid transform. |
| B2 | ScaleFR remembers one expected X scale, skipping a subsequent matching read within 0.1. | Retain the target's per-transform correction: differently sized planets must not compound each other. Alive entries must survive cache growth; expired managed identities cannot transfer an observation to recycled addresses. |
| B3 | get_FlatRenderable calls native first, scales the returned component and returns it. CameraZoomedEventHandler calls native first, then probes this getter because the game also reads the field directly. | Preserve receiver, result and native parameters; use the same scaling owner. |
| B4 | Both CreateBackdrop overloads call native, then, for a nonnull resource, increment load revision and reset the expected-scale observation before probing the getter. | Preserve Object and SystemVariationConfigCollection paths. Newly authored content is scaled even when its X equals an earlier applied value. |
| B5 | Populate captures population identity and load revision, calls native, and probes a changed nonnull SolarSystem variation population only if CreateBackdrop did not run within that operation. Cached prefabs keep their scaled content. | Preserve new variation, cached prefab, unchanged population, non-system and nested-loader controls without a second multiplication. |

The frozen global X cache is not restored: it made different authored sizes
interfere. The typed raw-pointer cache's clear-all limit can forget still-live
transforms and rescale them, so it needs identity-aware retirement instead of
discarding all history. A missing or throwing typed localScale getter also must
not become a successful zero-vector write, unlike the current default-valued
accessor. A successful setter is required before publishing an applied-scale
observation. No Config reference is retained across managed access in this path;
the multiplier is a copied scalar, as in frozen source.

`tests/camera_backdrop_cases.h` drives the actual callbacks and typed metadata
lookup using a controlled managed runtime. Its getter, runtime invoker/boxed
result, setter, renderable and population objects are synthetic; it does not install a native detour or render
planet scenery. Planned cases cover failures, authored/cached loaders, independent
sizes, more than 256 live entries, recycled managed identity, zero and nonfinite
values, including null and foreign returned boxes. All11 modes pass in the
[camera/confirmation cohort](../FEATURE-COHORT-20261102-CAMERA-CONFIRMATION.md),
with eight independent failures and three passing controls against unchanged
production source. Its manifest binds the exact source and executable inputs.

Native cached-system revisits, real scenery/resources, Unity object retirement,
current-client ABI/hook fit and Mac qualification remain separate. No new feature,
settings schema, live scale restoration, AAI or orbit diagnostic activation is
part of this contract.
