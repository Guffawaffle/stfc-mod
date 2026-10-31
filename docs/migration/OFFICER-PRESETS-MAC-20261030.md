# Officer presets — Mac feedback and post-save source correction

Received2026-10-30. Source base: play-dev-ripper6c2f8eca6f0c8182423d6c0ab3b8e9dd2fe5d67a.
Frozen source remains1baa55fedf1758b19f843cc59707b8901a0b4998. No source baseline
or release was advanced. This is not a verified Mac fix.

## What the report actually establishes

Guff supplied Mcohall's Discord report. Exact build IDs, Mac architecture,
settings, logs, screenshot and native row identities are unavailable.

- **Save/display:** after saving a crew, the visible list appears unsaved until
  backing out and reopening. Mcohall reports this does not occur with the old
  release/latest build. Treat this as a reported migration regression requiring
  source-identified confirmation, not an inferred failure to persist crew data.
- **Moving rows:** presets, possibly just a newly created one, cannot be moved.
  Mcohall subsequently reports moving also fails with the old non-upgraded-SPUD
  build. This does not establish a migration-specific key-binding regression.
- Transcript times10:42–13:20 are operator-supplied; their date/timezone was not
  independently established. No message was sent to Mcohall by this agent.

Both frozen and migrated source use Shift+Edit upward, Command+Edit downward on
Mac, and Control+Edit downward on Windows. They read the same Unity key codes
and GetKey internal call. No Control/Command change is justified by this report.
The setting defaults off; enabled state, successful installation and re-opening
after enabling remain necessary observations, not assumed explanations.

## Complete relevant source and retained native evidence

Read the complete frozen officer_preset_reorder.cc, its existing contract,
native feature/bindings, pure order/save protocol, production consumer fixtures,
configuration/registration references, and Unity modifier access. The old source
re-renders local order after native save. The target instead synchronized the
replacement row into its existing display permutation without requesting a
fresh ordering plan.

The retained Windows client273 oracle is separately identified by
`.ax/ripper-gameplay-sync-20261025/officer-preset-study/native-oracle/receipt.json`.
Dump SHA2563c019dd684ee5e1062e758831c00be719fb53c94b4859c95ba2cc46189dc8621
and save-success.asm SHA25612e825349b68eedfa363ecdd4874e32754c2250041c03f7ce803a23bd48cc385
were rechecked. This is archived Windows evidence, not a Mac process or a
current-client qualification. No native addresses are embedded in the feature.

The dump establishes PresentationType FirstItem=0, MiddleItem=1, LastItem=2.
The save body generates a replacement with MiddleItem, writes canonical
_data[OrderId], then looks up and rebinds a receiver. Presentation is positional;
it does not mean saved/unsaved. An earlier source comment claiming generation
uses the cached item's Presentation was incorrect and is corrected here.

## Two independently reproduced omissions

1. A fixed empty slot becomes a sortable filled slot after saving. Keeping the
   old permutation leaves its visible position inconsistent with the order model.
   A move can then be treated as an end boundary even though it looks movable.
   The old post-save render accounted for changed membership; the target did not.
2. Adopting the generated row's Presentation overwrote the cached canonical
   first/last positional annotation with MiddleItem. Frozen source preserves the
   positional presentation map across same-size saves. This is a separate visual
   correctness gap, not an explanation of a native saved/unsaved state.

Actual production-consumer fixture modes preset-save-new and preset-save-edge
both failed against unchanged production source. Their before logs are retained.
The newly saved crew scenario is a controlled fixture, not a capture of Mcohall's
particular row. Prior basic Windows acceptance exercised rename/reorder/reopen;
it did not qualify creating a crew in an empty slot.

## Bounded correction

- Keep the existing scoped canonical save and exact receiver translation.
- After successful native callback return, request one local re-projection in
  the existing frame callback; do not rebuild inside save or retry native save.
- Reconcile qualified empty-to-filled slot headers and append the new identity
  to session order from the existing order policy. Do not rewrite native IDs,
  crew contents, persistence formats or save receipts. Filled-slot identity
  changes remain refused instead of being silently adopted.
- Preserve the canonical positional presentation map across row replacement.
- Prevent reentrant frame work from consuming the queued request while any
  native save scope is active. Existing owner/generation checks reject closed
  or rebound views. Pending native rebuilds keep their ticket and rooted source
  until actual completion; they are not repeatedly regenerated.
- Add an event-bound development trace, save-reconciliation-queued, alongside
  existing initialization/projection traces. No hook, timer, worker or separate
  persistence owner was introduced. Modifier dispatch is unchanged.

## Qualification and limits

Evidence: D:/dev/stfc-workspace/artifacts/mac-officer-presets-20261030/.
regression-1.log and regression-final.log passed426/426 assertion-enabled Windows
steps. The final run includes reentrant frame work during nested save and
deferred post-save rebuild completion. Exact results, source hashes and any
subsequent Windows native build are recorded in qualification.json there.
Native build evidence must not be substituted for Mac rendering or interaction.

Signed implementation checkpoint: bfedc29437d1b50e648e8a5fa1bcc614ca074c49.
AX built that clean, stable source and verified the x64 proxy's16 exports,
static CRT and full provenance marker. Windows DLL SHA256:
53303e759400dff854be90396d6e06d6d829786bfa179e6f2b10c425ed7d714b.
AAI stays pinned to clean4bece2e5f1afa7c7343f009e83398742a0f86114.
Subsequent changes in this qualification record/inventory are documentation-only;
the manifest binds the compiled files independently. No game cycle or deployment.

The existing native original-once, lookup identity, async initialization,
same-source rebind, throwing/nested save, cancellation-by-release, canonical
identity and state-receipt tests remain. Original raw evidence and quiet/no-op
paths remain available; no player presets or configuration were edited.

Mac compilation requires CI in this workflow; Mac runtime requires Mcohall or
another actual Mac session. Neither was run here. No push/deploy/restart is
implied by the source correction. Actual crew creation, replacement and deletion
must not be relabeled accepted from these controlled fixtures.

## Focused Mac trial

Record candidate/build and arm64 versus Intel, current client, installed hook
status, and the real allow_officer_preset_reordering setting. Close/reopen the
preset view after enabling. Use an operator-chosen disposable crew, not an
automated overwrite of player presets.

1. Save to an empty slot. Without backing out, verify the new crew is visible,
   its name/crew are correct, and local ordering is coherent.
2. Move that new row upward with Shift+Edit and downward with Command+Edit.
   Recognized reorder clicks must not open rename. Check a pre-existing row too.
3. Plain Edit should rename the intended row without changing its crew. Check
   first/last row styling; close/reopen and verify crew and local order separately.
4. If it fails, preserve the installed build identity and event-bound logs.
   Distinguish option/installation/input readiness, native save result, source
   adoption and ordering projection before changing modifier keys or mechanics.

The old-release comparison still needs its own exact identity. Do not attribute
both reports to one defect or claim Mac parity until the trial supports it.

## LexSona consumption

The connected scoped MCP accepted explicit preservation-first_migration@1 for
this investigation; the full receipt is retained. The same five duties were
explicitly read. They informed the distinction between two reports, source
omission and inherited/unknown behavior, and reproductions versus Mac evidence.
This does not prove causal benefit or automatic learning. Source/fixture/native
provenance remains separate from the behavioral receipt. No Runner execution,
tooling redesign or skill retirement occurred.
