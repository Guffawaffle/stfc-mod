# Galaxy controls restoration

This is the Galaxy cohort of [PLAY-PARITY.md](PLAY-PARITY.md), with dispositions in
[play-functional-parity.json](play-functional-parity.json). `play` baseline:
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`. It does not establish full migration parity.
Source checkpoint: `251889690a2377429e72e233dafffcbf370fa6dc`.

## Restored gameplay paths

- Far-zoom selection uses native Galaxy positions, native minor-node qualification and native hit testing.
  Temporary POIs keep immutable system identities; no enlarged hit radius or forced target is introduced.
  The native recognizer runs on both platforms; a correlated fallback is attempted only when it did not
  dispatch that tap. Normal zoom tiers keep native selection.
- Default, Mining, Hostiles and Hazards can be selected independently. The last deselection falls back to
  Default. Native toggle highlighting uses the same selection; all four flags are saved through the existing
  registered, preserving TOML writer. Single selections synchronize the native representative too.
- Native composition retains visited/range/level/hazard eligibility. The preferred native helper path avoids
  four full passes; a bounded native-per-selected-layer fallback remains. Container union uses local activation,
  including when its parent is inactive. Layout changes have reverse restoration journals for pooled reuse.
- Major and minor systems have independent Native/Always/Threshold label profiles, including normalized zoom
  changes inside one native tier. Minor-node culling expands when required. Camera and LOD callbacks each have
  one shared terminal hook owner, independent of optional planet/zoom installation success.
- The housing marker reuses the native home sprite and retained `play` dataset. It identifies cataloged
  housing-capable systems, **not available docking slots**. Unknown/out-of-coverage systems remain unknown.
  Markers are removed on pooled release/rebind and disabled when native label eligibility is absent.

## Controls and preservation boundary

Existing `[graphics]` keys are preserved:

```toml
galaxy_extended_selection = false
galaxy_multi_select = false
galaxy_station_housing = false
galaxy_overlay_default = true
galaxy_overlay_mining = false
galaxy_overlay_hostiles = false
galaxy_overlay_hazards = false
galaxy_label_major_detail = "native"
galaxy_label_major_threshold = 0.5
galaxy_label_minor_detail = "native"
galaxy_label_minor_threshold = 0.5
```

Defaults remain `play` defaults. Invalid threshold values fall back to 0.5; an empty overlay selection records
Default as effective in both runtime configuration and vars. Native map buttons implement multi-selection
when enabled. The **full native Mod Settings Galaxy page**, including boolean controls, choices and sliders,
is not restored by this cohort. It remains part of the separately tracked Mod Settings gap. The four Galaxy
inventory entries therefore remain Partial rather than claiming complete player-facing equivalence.

No game TOML, store or installed DLL was changed during local implementation. System orbit remains excluded.

## Ownership and failure behavior

The selection pool owns at most 64 POIs and activates at most 16 fully qualified candidates for a tap. Those
limits bound temporary objects, not native Galaxy source counts: all source nodes are scanned. Excess density
falls back to native selection rather than selecting a truncated candidate set. Arrays are rooted and read
without per-node boxing; counts, capacities and exact primitive element types are qualified.

POIs never change identity. Retirement requires complete observed owner coverage, no pin, and a conservative
age/click interval. Owners include selection lists, viewers and contexts, pending/current/empty and cached
interactions, and camera tracking. Cached dictionary keys use the retained InteractableType catalog; the
observed count must match before/after native counts. Unknown future keys or failed observations deny
retirement. This is conservative observed coverage, not proof against every possible future game owner.

Partial construction retains cleanup custody in the same bounded pool. Inactive readback, rather than setter
success alone, discharges activation cleanup. Failed cleanup remains pending and is retried through the installed
feature frame callback. Competing synchronous taps are suppressed while the temporary target set is unsafe.

Label/housing state is bound to weak owner identity and native node context. Strong-handle allocation failure
does not mean an object disappeared and cannot discard a restoration journal. Partial housing markers carry
readiness state and are cleaned/rebuilt before reuse. Temporary overlay mode restoration is conditional on
write ownership: config replacement stops composition, but an unchanged temporary mode is restored; a newer
player selection wins, including when it chooses the same mode. No new ScreenManager.Update owner is added.

## Local evidence and remaining qualification

See the exact local receipt under
`D:/dev/stfc-workspace/.ax/galaxy-parity-20261025/` for candidate inputs, binary hash and executed checks.
The first component build passed 201/201 steps. The Profiles-enabled intermediate build passed 213/213 steps;
the final corrected source checkpoint `25188969` also passed 213/213 steps in `checkpoint-build.log`,
including the production dictionary observation fixture. The final DLL passed the 18-export Profiles host,
static CRT and provenance checks. `galaxy-local-receipt.json` and `checkpoint-inputs.json` bind actual source
inputs and DLL bytes. The pre-existing unrelated `build.zig` dev-install wiring was present during the build
and is explicitly recorded; it was not included in this source commit.

Final DLL SHA-256: `fbefb2e523be3c575dd81b6fa0eb6ee2a099bdbdb87d1f5234ee5debd09e7d4d`.
No installed artifact or native/runtime test is inferred from these results.

Fixtures cover profile thresholds; every nonempty overlay combination; configuration/default/invalid values;
housing schema, ordering and coverage; complete/missing/null owner paths; temporary-mode write ownership;
tap identity; proxy retirement eligibility and failed-disable readback; exact native backing arrays, custody
failures and dictionary coverage/count mutation. They do not execute Unity layout or native hit testing.

Retained Windows client273 evidence is hash-bound to dump
`3c019dd684ee5e1062e758831c00be719fb53c94b4859c95ba2cc46189dc8621` and GameAssembly
`a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
Housing dataset SHA-256: `7f6d7a3af53eb44ad74ea818d5e6eaa7abd76c4bbd6553280d03c7085acda49c`
(554 positive IDs; 2,599 covered IDs).
`retained-windows-entry-extents.json` records the detoured native entries, including the 35-byte HUD wrapper.
These are retained-source extents, not a live installation compatibility claim. macOS uses CheckedEntry;
no macOS build or runtime qualification is claimed here.

### Focused in-game acceptance

1. With existing-user TOML and optional settings disabled, normal Galaxy taps, labels, zoom, previews and System
   fleet labels retain native/configured behavior. Test a camera/scene replacement and repeated Galaxy returns.
2. Enable far selection: tap several minor systems at Far zoom, dismiss/reopen previews, pan/zoom, and inspect
   overlapping candidates. A tap must dispatch once. Exercise more than 64 distinct candidate IDs; if owners
   cannot be established or all slots are pinned, native fallback is expected, not forced retirement.
3. Enable multi-selection: exercise all 15 nonempty combinations, including unvisited/range-limited systems,
   zero-level hostiles, hazards and inactive-parent reuse. Default restores after last deselection. Check native
   highlights, saved flags, reload/restart, single-layer reload and disable-to-native restoration.
4. Check major/minor profiles independently at threshold endpoints and inside tiers; switch Native/Always/
   Threshold and return to Native. Minor culling and visible names must agree. Do not use Always on dense minor
   systems as a general performance acceptance baseline.
5. Check a known housing-positive, covered-negative and unknown system; compare the native home sprite,
   disabled state, hidden name eligibility and pooled rebind/release. Verify no residual marker/layout shifts.
6. Failure-injection/runtime qualification still needed: native object creation/setters, incomplete layout
   rollback, reentrant pooled release, pending POI cleanup, native recognizer correlation and dictionary ownership.
   Pure policy tests do not substitute for these paths. Full native Settings controls remain a separate gap.

## Sources

- `src/mod/features/galaxy.cc`, `galaxy_selection.cc`, `galaxy.h`: behavior and shared feature exports.
- `src/mod/game/galaxy.h`: checked native observations, node/value layouts, owner scanning access.
- `src/mod/galaxy_policy.h`, `galaxy_selection_policy.h`, `galaxy_selection_pins.h`,
  `galaxy_selection_pin_metadata.h`, `galaxy_housing_data.h`: game-free policies/data qualification.
- `src/mod/features/camera.cc`, `fleet_labels.cc`: shared camera/LOD hook ownership.
- `assets/data/station_housing.json`: byte-identical retained PLAY dataset.
- `tests/galaxy_*`, `tests/il2cpp_bindings.cc`, `tests/mod_logic.cc`: local qualification.

Direct tools and independent read-only collaboration were used. No Runner-governed execution, CI campaign,
PR maintenance, push, merge, game deployment or cycle is claimed by this checkpoint.


## First native trial and correction

The first authorized trial deployed `25188969` after building. Labels/housing installed, but far selection
declined because `TKTapRecognizer` was incorrectly bound to Assembly-CSharp. Retained273 puts type26076
in TouchKit (image78, types26064–26077). `efb49039` corrects that binding and adds an assembly-sensitive
controlled-runtime regression. The previous fake image adapter treated every assembly as the same image;
the new case refuses the recognizer in the wrong image. The correction passed 213/213 local build steps
and the Windows PE checks before deployment.

Corrected installed DLL SHA-256:
`ffafb23dacd95c194b37e3d6e0d73e4a9da217ac47b3d3f9bc88e9551b26b308`.
The exact installed bytes match the built artifact. The fresh ordinary Windows273 session reports all
62 groups installed, including GalaxyLabels/GalaxySelection, and FrameTick ready with no startup errors.
This is startup smoke, not evidence of correct Galaxy gameplay or macOS behavior. Player tests remain pending.

A trial-configuration edit incorrectly overlooked the existing quoted Galaxy keys and inserted two duplicate
bare keys. Parsing failed and the intermediate session used defaults; the saved user settings were not
reset. Removing exactly those two inserted lines restored valid TOML. The repaired file reloaded, then
the game restarted to restore startup-only switches and clear fallback-session live intents. Effective
Galaxy options and representative saved scale/zoom values were independently compared with the original
file. All match after restart. No intended configuration changes remain. Do not reuse the failed regex
edit approach: parse and validate the complete TOML before any future configuration write.

Evidence remains under `.ax/galaxy-parity-20261025`: `galaxy-deploy.json`/`galaxy-cycle.json` for the
superseded first candidate; `touchkit-deploy.json`, `touchkit-cycle.json`, `touchkit-build.log`,
`touchkit-startup.log`, `touchkit-native-receipt.json`, `config-repair.json` and
`config-repair-restart.json` for the correction, incident and final session. Deployment wrappers report
the canonical checkout's branch as routing metadata; the explicit artifact/source checkpoint in these
receipts identifies the isolated migration candidate. No canonical branch switch or build occurred.


### Player confirmation on corrected candidate

Guff confirmed **yes** to all three requested checks on the corrected installed candidate: far-zoom clicks
open systems; the four native layers can remain selected together; housing markers appear. The full original
configuration was restored and startup values read back before the request. These are direct player checks,
not inferences from installation counts. They do not qualify every15-layer combination, retirement pressure,
all pooled/reentry failures, separate major/minor thresholds, macOS, or the still-missing Settings controls.
The four Galaxy dispositions therefore remain Partial with explicit verified-subset evidence.

The next queued gameplay gap is officer-preset ordering/native save. Continue from the existing pure policy
and retained native oracle rather than repeating discovery or activating the parked prototype. Native save
updates canonical `_data[OrderId]` and independently resolves a displayed receiver, so a data-only pointer
swap remains disproven. The canonical-source/receiver-map transition and nested-save case require their own
synthetic adapter qualification before activation. Existing preset data/stores remain untouched.
