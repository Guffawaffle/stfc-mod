# PLAY-OFFICER-PRESETS — Officer preset ordering and native save

**2026-10-27 status:** correction `a0565d7e` has basic Windows player acceptance:
opening, Shift/Ctrl reordering without opening rename, plain Edit renaming the
correct row with its crew unchanged, and retained order after closing/reopening.
Eight local-order changes have successful persistence receipts. The earlier blank
view was an initialization-order regression, diagnosed against the actual native
scroller and reproduced in fixtures before correction. macOS and native crew
creation/replacement/deletion remain unqualified. This is not overall parity.
See [the accepted checkpoint](#initialization-correction-and-basic-windows-acceptance--2026-10-27).

Contract v1, scanned 2026-10-26. Frozen source `1baa55fedf1758b19f843cc59707b8901a0b4998`;
target `play-dev-ripper@f4f64180543a9231234cd2b3d3f547dfa580476b`.
Windows/macOS; `patches.officerpresetreorderhooks=true`,
`ui.allow_officer_preset_reordering=false`. Entry: Officer Presets name-edit
button with Shift to move up, Ctrl (Windows) / Command (Mac) to move down.

Source `mods/src/patches/parts/officer_preset_reorder.cc` owns all five native
hooks, canonical validation, separate display order, save/rebind/release,
scroll preservation and state queue. Supporting `mod_state`, config, Settings
availability and native preset/scroller classes are part of the workflow.

The following table records the original scan, before native integration. Current
qualification is recorded in the dated sections below.

| Obligation | Source behavior | Target status at original scan |
| --- | --- | --- |
| P1 | Preserve int64 SlotId including zero; canonical OrderId equals index; refuse ambiguous filled duplicate identities. | Existing `officer_preset_order.h` policy supports this; native acquisition absent. |
| P2 | Empty/all-null crew rows stay fixed, including duplicate pending SlotIds; IsOccupied is not authoritative. | Existing qualified observation/policy fixture supports this. |
| P3 | Reorder only display rows; keep canonical identities/crews unchanged and apply destination Presentation. | No qualified native adapter. Do not replace with a data-only swap. |
| P4 | Modifier name-edit moves one adjacent filled row, skips fixed rows and consumes boundaries; otherwise invoke native edit once. | Native input/binding absent; normal native behavior remains. |
| P5 | Native save runs once on canonical data and matching receivers; then observe any replacement row and reapply view safely. | Actual source/receiver/reentry/restore boundary unqualified; inactive experiment is not product behavior. |
| P6 | Restore native presentations/source on release or disable; never publish into another controller. | Requires rooted native ownership, generation and actual adapter tests. |
| P7 | Read persisted local order; append newly encountered IDs; queue order with original state key and truthful admission/durability. | Existing state worker reusable; no live mutation authorized merely for a test. |

This is the previously recorded missing native integration, not a newly invented
feature. Source preservation cannot justify reintroducing its unqualified raw
controller lifetime, reentrant rendering or original-save fallback hazards.
See [cohort evidence and options](../FEATURE-COHORT-20261026-02.md).
Pure fixtures and native compilation do not establish safe preset saving.

## Operator clarification and next integration boundary — 2026-10-26

Officer preset ordering is **not functioning in the current migration**. Guff
withdrew the proposed error explanation: it was speculation, not evidence. Do
not investigate officer sorting or attribute the separate contained
`GetComponent` exception to this failure. The established blocker is the missing
native preset adapter described above.

The next integration should preserve the complete workflow, including applying
the saved display order, modifier-click handling, canonical native preset saves,
replacement-row adoption and release. Shift+Edit moves up; Ctrl+Edit on Windows
or Command+Edit on macOS moves down. Recognized reorder clicks must consume the
native Edit action, including an end boundary; ordinary Edit remains native
renaming. Preserve the source's both/neither-modifier rule unless explicitly
changed. The source only consumes a move when its reorder handler succeeds;
the requested recognized-click consumption must be accounted for explicitly,
not represented as a qualified behavior of the absent adapter.

Keep three persistence boundaries distinct:

- **Feature settings:** `config_settings.cc` / `ValueSetting` call the registered
  `runtime_config::SaveSetting` path into the selected TOML file.
- **Local display order:** `mod_state::QueueOfficerOrder` persists
  `officer_preset_order` through the existing state worker. Admission is not a
  durable-save receipt. Integrating a caller and loading/applying its result are
  still part of the missing native workflow.
- **Actual preset contents:** the game's native save callback must run exactly
  once against canonical data and the corresponding visible receiver mapping.
  Neither a TOML write nor a local state receipt substitutes for that save.

Further tests and cycles remain on hold. No live configuration/store contents
were inspected or changed for this clarification.

## Native integration — 2026-10-27

Implementation `30eeec8eb8f92c5946adbe95481d4c88aae64bf4` reuses the existing
`officer_preset_order` policy, `officer_preset_handoff` protocol and `mod_state`
owner. The complete frozen source scan is not repeated or replaced by a feature
description. Narrow adaptations are explicit:

- Checked typed bindings in `src/mod/game/officer_presets.h` replace raw layouts
  and cached unmanaged controller pointers. Metadata is resolved before
  projecting; controller/context/scroller/canonical and display/candidate arrays
  retain strong GC custody through saves and re-entry.
- One `OfficerPresetReorder` descriptor uses the existing patch switch and one
  transactional HookSet. The new scoped scroller-lookup hook replaces the source's
  observer-only `OfficerManager.TryGetPresetItemContext` hook. Exact IDs and
  appended/new slots are obtained from the canonical list at binding/move time.
- The canonical list remains unchanged during display sorting. Native saves run
  once with canonical `_data`; only that controller/scroller/save scope translates
  canonical OrderId into the qualified display index. Native row replacements
  propagate through GC-barrier array writes before their matching receiver binds.
- Readiness checks compare source, initialization state and existing receiver
  contexts. A successful rebuild call alone does not qualify the projection.
  Failed/partial transitions retain roots and require canonical repair; receipts
  and readiness can be observed by the feature's normal frame callback.
- Disable, release, same-source rebind, nested saves and original exceptions do
  not bypass a guard or restore an older array into a newer generation. A save
  scope can finish after its view closes without republishing that view.
- Shift up / Ctrl Windows or Command macOS down consumes native Edit on a
  recognized reorder request, including a boundary/declined request. Both/neither
  modifier retains normal native rename. This strengthens the explicit operator
  consumption requirement beyond the old success-only fallback.
- Local moves remain immediately accepted session state, as in the source.
  `QueueOfficerOrder` admission logs pending versus unadmitted; only the exact
  latest successful `Group::OfficerOrder` receipt is logged as persisted. Failure
  retains the accepted session order without claiming a durable save.

The pre-resolved canonical field write is deliberately a synchronous field
operation, not a managed rebuild inside native save. Missing metadata refuses
installation. A contradictory readback of that rooted, pre-resolved write is a
fatal invariant violation rather than permission to call native save against a
display list. This is not how an ordinary file-save failure is handled and must
remain visible in the exact-client/platform qualification. No unsupported write
fallback is represented as success.

Verification includes the existing policy/handoff tests plus
`tests/officer_presets_native_cases.h`, compiled with the **production feature**
and game bindings against the controlled IL2CPP runtime. It covers exact large/zero
IDs, fixed empty rows, metadata enum fields, source/receiver handoff, replacement,
modifier/boundary behavior, pending/failed receipts, failed renderer readiness,
canonical repair, nested/throwing saves, disable, same-source rebind, close during
save and root disposal. Fake native bodies and file-state admissions are identified
as such; these are not in-game observations.

Settings persistence is independently exercised through the real model, runtime
adapter, writer, TOML editor and filesystem in an isolated directory. See
[mechanism differences and the bounded follow-up](../PERSISTENCE-MECHANISMS.md).

### Focused in-game qualification checklist at initial implementation

This checklist predates the trial. The accepted checkpoint below identifies what
has since passed and what remains untested; do not treat this list as a new request.

1. Enable preset reordering in Mod Settings; observe save status and inspect the
   exact TOML value, then close/reopen Settings. Include a controlled return to
   the previous value and a relaunch when the testing hold is released.
2. Open presets with filled and empty rows. Shift+Edit up and Ctrl+Edit down;
   recognized chords must not open rename, including at boundaries. Plain Edit
   must still rename. Verify the selected crew remains the intended crew.
3. Rename/save one operator-chosen disposable preset using native controls.
   Verify correct contents and row, no unrelated preset changed, and reopen the
   preset screen. Inspect local-order pending/persisted receipts separately from
   the game's native save result. Do not automate overwriting player crews.
4. Close/reopen, disable/re-enable, scroll, and quit/relaunch. Confirm saved local
   order, fixed empties, normal native selection and no stale display source.
5. macOS requires its own CI build and Mcohall trial, including Command+Edit.

No new runtime deployment, store edit or player verification is implied by the
offline implementation record.

## Failed enabled trial and diagnostic boundary — 2026-10-27

Historical failure record; superseded by the correction and qualification below.

The main game booted with all 68 groups installed, but opening presets with
reordering enabled produced a blank/blurred view. Mod log reported "bound native
view; local projection not ready". The game's Player.log includes a
`NullReferenceException` stack through `SmartScroller.ReassignItem` →
`SmartScrollerBase.RebindAtStartingLocation` → `ClearAndGenerateContents` →
native `OfficerPresetsViewController.OnDidBindCanvasContext`. Turning the setting
off did not recover that same session. Guff manually restarted and confirmed the
native list works with the setting off. Those facts do not yet prove which step
first damages the renderer.

The operator's Mod Settings change was observed as `false` in the actual TOML
and survived the restart. This qualifies that one real control's off-save/restart
path, not every Settings control or every save failure outcome. The local-state
file hash remained unchanged across the failed trial; no reorder/save receipt was
claimed. Retained logs/screenshot/assessment:
`D:/dev/stfc-workspace/artifacts/preset-settings-20261027/failed-enabled-trial/`.

`8cfd43a15e328164f7ac261f5d8d3220f4baea08` adds `_MODDBG` stage traces at native bind,
projection clear and canonical repair, plus the exact readiness failure reason,
source identity, row count, initializer state and first mismatched receiver. It
does not apply the suspected asynchronous-initialization correction yet. A
successful synchronous fixture had not represented native scroller initialization;
do not use it to dismiss this runtime failure. Logging is event-bound, not a
per-frame dump. The requested diagnostic build retains optimization and symbols
(`ReleaseFast`, `moddbg=true`, identity mode `releasedbg`).

Immediate rebuild/repair during canvas binding is a hypothesis to test against
those traces and the retained native scroller implementation. Further functional
changes should follow that evidence, preserve the existing native save guard,
and add a reproducing controlled-runtime case for the actual failure.

## Initialization correction and basic Windows acceptance — 2026-10-27

Implementation: `a0565d7e7a801679086e56c287d47e7c867bdbb3` on `play-dev-ripper`.
The diagnostic trial (`e0c24c97`, incorporating `8cfd43a1`) established that native
canvas binding returns **before** `SmartScroller.Start()`: live `_data` is null
while the canonical list is held for initialization. The adapter immediately
requested a display rebuild and then forced a canonical repair into that pending
initializer. The retained native implementation confirms the deferred held-source
path. Its GameAssembly SHA-256 matches client 273's installed image:
`a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
This explains the observed blank view; it is not an officer-sort diagnosis.

Binding now acquires roots without rebuilding. The existing frame callback waits
for canonical source adoption, cleared initialization flags and matching receiver
contexts before projecting. Pending display-source adoption retains the candidate
and its ownership ticket rather than forcing repair or repeatedly rebuilding.
Closing before native initialization detaches without manufacturing `_data`.
Canonical save/index translation, exact identities, modifier behavior, state owner
and save receipts are unchanged. No second task system or persistence owner.

The production-adapter fixture now reproduces the initially null source with a
held canonical list, repeated waiting frames, early close before `Start`, and later
asynchronous source adoption. It proves no premature source/presentation mutation
or repeated rebuild. Existing save replacement/receiver, nested and throwing save,
disable/rebind, disposal and failure cases remain in that fixture.

Windows `test-mod test-il2cpp` passed **214/214 build/run steps** at this source.
The production-adapter executable was explicitly rerun with exit 0 and SHA-256
`67ed93b170f1685379d7e07bf36944cc9e3b9b7094c6aa3b02a937a2422c8d27`.
The complete Windows DLL build and x86_64 proxy/export/static-CRT/full-provenance
verification passed separately. Installed `releasedbg` DLL SHA-256:
`ec9339e4cb3748f22a129c7f1873f9b18e299d353af999cdece165aee442dd29`.
The ordinary main game was cycled **after building**, PID 13068 → 48968,
installation `C:/Games/Star Trek Fleet Command/default/game`. The source remained
stable across the build; installed disk bytes matched. This is not an independent
measurement of the in-memory module bytes.

Actual trace: native bind retained the canonical list at 01:31:09.991; native
initialization was ready at 01:31:10.130; projection was ready at 01:31:10.136.
The captured corrected session has zero `SmartScroller.ReassignItem` exception
stack occurrences and successful exact local-order receipts for revisions 1–8.
Guff confirmed “worked”, then “yup all work” for this bounded workflow:

- Presets open with reordering enabled.
- Shift+Edit / Ctrl+Edit move rows and consume the rename action.
- Plain Edit renames the intended disposable preset, keeping its crew unchanged.
- Closing/reopening preserves local order and the renamed row.

Those observations qualify basic Windows interaction, not every native failure
path. Actual crew creation/replacement/deletion, forced-quit/power-loss durability,
long stress sessions and macOS CI/Mcohall testing remain open. The earlier real
Settings OFF-save/restart test qualifies that control's TOML path, not all Settings.
The native rename result and local-order file receipts remain separate evidence.

Retained evidence:

- Accepted logs, executable rerun and source/binary/runtime receipt:
  `D:/dev/stfc-workspace/artifacts/preset-settings-20261027/accepted-windows-basic/qualification.json`
  (SHA-256 `7ff68ebb310aba831812361f5c22551ab9f184a6a0dd165d9fca0302d10b2c6c`).
- Reproduced diagnostic failure:
  `D:/dev/stfc-workspace/artifacts/preset-settings-20261027/reproduced-debug/`.
- Current full-suite log:
  `D:/dev/stfc-workspace/.ax/preset-initialization-fix-tests.log`.
- Exact build/cycle receipt:
  `D:/dev/stfc-workspace/.ax/preset-initialization-fix-cycle.json`.

The user-requested `ax build --zig` / `ax cycle --zig` adapter is machine-local,
under canonical Downstream `.ax-priv`; it is not a new tracked product subsystem.
Its source/binary checks and build-before-downtime flow were exercised for both
diagnostic and corrected builds. Backup, final files, plans and test receipts are
preserved in `D:/dev/stfc-workspace/artifacts/ax-zig-cycle-20261027/`.
Default Xmake routing is unchanged; profile routing and AXF schema expansion were
not added. Logging and shared low-level file-I/O extraction remain deferred.
