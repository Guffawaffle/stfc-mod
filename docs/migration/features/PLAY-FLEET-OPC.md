# PLAY-FLEET-OPC — OPC highlight and mining ETA

Contract v1, source scan 2026-10-28. Implementation reconciliation against frozen
play-dev `1baa55fedf1758b19f843cc59707b8901a0b4998`; target base
`play-dev-ripper@5ec09aa0` in the existing Ripper Downstream checkout. This is a
selected presentation workflow, not a new alert system or overall parity claim.

| Inspected input | Exact revision | Presence / disposition |
| --- | --- | --- |
| upstream dev | Not reassessed in this slice | Do not infer presence from the old PLAY inventory. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Historical inventory baseline; not the behavioral contract for this pass. |
| play-dev / frozen source | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Complete indicator implementation, shared sample, Settings consumers and defaults inspected. |
| refactor | `5ec09aa0` plus this recorded correction delta | Existing typed producer/consumer retained; bounded lifecycle and failure corrections below. |

## Source workflow and ownership

Pinned paths are relative to the frozen tree, read through Git rather than the
unrelated branch checked out in the canonical repository:

- `mods/src/patches/parts/opc_indicators.cc`: complete six event callbacks,
  supplemental `LifecycleTick`, ETA computation/formatting, highlight cloning,
  tile/card rendering, weak artwork ownership, settings cleanup and setup retry.
- `mods/src/patches/fleet_opc_sample.h/.cc`: game-thread-only value cache, shared
  with `mods/src/patches/parts/fleet_notifications.cc`; managed addresses are
  identity tokens, never retained objects to dereference later.
- `mods/src/patches/opc_indicators.h`, `mods/src/settings/opc_indicators.cc/.h`,
  `mods/src/settings/upstream_settings.cc`: availability, two independent live
  controls, refresh and registered TOML persistence.
- `mods/src/config.cc`, `mods/src/defaultconfig.h` and
  `mods/src/patches/patches.cc`: independent false defaults and installation.
- `docs/OPC_INDICATOR_HOOKS.md`: native signatures, late data delivery, platform
  installation differences and the separation of settings/model/UI ownership.

The target keeps `features/opc_indicators.cc`, `features/fleet_opc_sample.h/.cc`
and `game/fleet_opc.h`. Existing `settings/config_settings.cc` owns the two live
Previews controls and the common registered writer. No new setting, writer,
notification producer, decoder, worker or hook registry is introduced.

## Behavioral obligations

| ID | Frozen source behavior | Target disposition and evidence layer |
| --- | --- | --- |
| O1 | `highlight_opc_fleets` and `fleet_hud_opc_eta` are independent, live and false by default; disabled settings preserve native behavior. | Retain existing Config/catalog/writer. Track both control bits independently in the shared frame; each change bypasses cadence, invalidates cargo samples and clears setup backoff. Disabled cargo callbacks now return after the original without inspecting fleet data. Production lifecycle fixture; native Settings write/reopen remains separate. |
| O2 | A ten-slot cache shares known or unknown cargo for less than one second only when slot, fleet ID, managed-object identity and state match. Cargo events invalidate before recomputation. | Existing cache and strict `current > protected` threshold unchanged. Retain typed rooted cargo/progress producer and checked observations. Production sample and cargo-callback fixtures, alongside existing pure cache tests. |
| O3 | Only Mining has ETA; unknown/nonfinite cargo or unknown/nonpositive rate hides it. Cargo over protection displays OPC; a node that finishes within remaining protection displays SAFE; otherwise show rounded duration. Equality is still protected. Selected card shows OPC: Now or the corresponding duration when tile display is available. | Existing `Evaluate`, `Duration`, `Display`, `CardDisplay` and native `IMiningSlot` producer retained, including the target's safe int64 conversion bound. Existing arithmetic tests plus actual producer/render fixtures; no arithmetic rewrite. |
| O4 | A ship with cargo over protection and a deployed-state bit gets the amber frame cloned from Background and the MiningContainer sprite. ETA/backing and selected-panel timer label use the source placements, colors and noninteractive artwork. | Keep existing typed native renderers and source layout values. Restore checked highlight configuration, required initial label/backing style, layout/color/text results and failure propagation. Production fixture covers required style setters, existing native surfaces, selected/unselected layout, SAFE/OPC, card text and failed text hiding. Real clone creation/font/Unity layout are not qualified by these supplied objects. |
| O5 | Native bind/set calls run first, then refresh; cargo events run native first, then invalidate and refresh. Late model/anchor delivery is revisited at one-second cadence. | Retain one shared frame callback instead of adding the source's seventh LifecycleTick detour. Bind/clear remain the six existing event hooks. Production callback and frame fixtures qualify native-first order, disabled admission and toggle cleanup; actual shared-frame registration/late-client delivery remain native checks. |
| O6 | State widget clear hides ETA/backing; flag clear hides highlight. Rebinding resets the old episode. Weak artwork/view ownership must not retain destroyed Unity objects. | Record Local/State/Flag ownership so pooled clears no longer hide the independent indicator sharing the body. Local release also hides its timer-card label. Retain generation checks and weak views; temporary strong-root refusal now retains a surviving weak view instead of misclassifying collection. Production lifecycle fixture covers clears, rebind generations, failed root and expired weak target. |
| O7 | Setup failure backs off 1, 2, 4, 8, 16, then at most 30 seconds; changed fleet or anchor and settings refresh reset the failure episode. Failed setup must not claim visible success. | Retain bounded backoff; reset when observed fleet ID, body identity or selected timer anchor changes. Status-driven hiding runs before setup backoff; when no indicator applies, clear the old failure episode. Checked render failures hide incomplete ETA/card surfaces and report failure. Production regression cases cover changed fleet, card failure followed by docking during retry, and timer replacement during retry. Actual missing-anchor allocation/recreation remains native qualification. |
| O8 | Source resolves exact void callback signatures; macOS refuses aliased native entries and incomplete families. Windows can retain one feature when the other's lookup family is absent; detour failure disables both. | Retain the target's single atomic six-hook group and new restart-only `patches.opcindicatorhooks` switch as explicit target safety differences. BindDataContext now checks void/reference parameters, all six entries use CheckedEntry, and incompatible aliases refuse installation on either platform. Hook signature/transaction library coverage and source inspection; no native installation outcome is claimed by the direct consumer fixture. |

## Decisions and remaining boundaries

The shared one-second frame is an existing adaptation and remains the only
periodic owner. It checks controls each frame, traverses tracked views at cadence
or a control change, and performs immediate event refreshes where already wired.
There is no additional fleet scan or new main-thread dispatcher.

The target keeps reusable ETA/card surfaces hidden for checked retry rather than
destroying and recreating both surfaces on every failed setter, as the source did.
Named artwork missing its required component or label transform is destroyed and
refused for that frame, preserving the source's duplicate-name protection during
deferred Unity destruction. Highlight clones still destroy themselves when
required configuration fails and publish their name only after successful setup.
Setup backoff controls allocation/configuration work; current status still hides
artwork that no longer applies at the existing observation cadence. Existing
ETA/card text that differs from the newly observed nonempty value is also hidden
during retry, so failure on one surface cannot leave another stale value visible.

Target atomic installation intentionally does not reproduce source Windows
partial-family availability. Its patch switch and exact feature readiness remain
distinct from the two live user choices. Missing shared frame delivery does not
qualify the source's supplemental native LifecycleTick fallback: this target
depends on the shared frame owner for periodic recovery.

Source transition-only debug logging is not reproduced by this typed consumer;
the feature relies on checked-binding diagnostics and the existing installer.
That observability difference does not add another diagnostic surface.

## Validation and disposition

New `tests/opc_indicators_bindings_cases.h` includes the actual production sample
and consumer in the established controlled runtime. Registration belongs to the
integrator in `tests/il2cpp_bindings.cc` and `zig/tests.zig`:

- `opc-consumer-sample`: actual typed producer, rooted cargo/progress, interface
  mining data, cache reuse/invalidation and contained observation failure.
- `opc-consumer-render`: production renderers and complete view refresh using
  supplied native surfaces, required initial style helpers, source placements/text,
  failed setters, changed-fleet/timer retry reset, native-first cargo invalidation,
  docking during active setup backoff and invalid named-artwork destruction.
- `opc-consumer-lifecycle`: actual set/clear/cargo callbacks and shared-frame
  consumer; independent controls, shared-body cleanup, native-first dispatch,
  rebind generation, temporary custody failure and collected weak views.

The fixture's optional type-object override supplies per-request type
tokens for the fake GetComponent body; its default leaves other cases unchanged.
It supplies native method bodies and pre-existing widget objects, not a Unity
scene, live lifecycle scheduler, allocation/font engine or hooked callback host.
Compilation/execution results are pending the integrator's checks and must be
attached to the exact final candidate. This worker performed no build, deploy,
game cycle, configuration edit or runtime trial.

Native follow-up: deliberately enable each control independently; verify already
mining startup and late delivery, threshold equality/crossing, SAFE node versus
time-to-OPC, selected-panel layout, missing model/rate and stopped mining. Disable
one control while the other remains active, pool/rebind the same widgets, switch
fleets/panels during setup retry and revisit after scene recreation. Match these
checks to the exact artifact and client. macOS CI and Mcohall native qualification
remain separate. Existing accepted Windows checks elsewhere remain valid for
their own workflows; no overall parity or platform-equivalence claim follows.
