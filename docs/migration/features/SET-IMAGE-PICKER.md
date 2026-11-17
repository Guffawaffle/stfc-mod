# SET-IMAGE-PICKER — Loading image selection

Contract version: 1. Status: implementation and offline qualification.

## Inputs and scope

- Source: `frozen/play-dev-20261026`, `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Target base: `076c85a84ee5d126b02582f55936ccfc4a066a29`.
- Branch: `feature/migrate-play-feature-pilots-20261026`.
- Existing inventory ID: SET-IMAGE-PICKER, historically consumer-only.
- Scope: preserve image-path setting, native picker, bounded validation, exact
  fallback, registered persistence and restart behavior. No image copying, decoder
  redesign, audio picker UI, Settings redesign, or game deployment.

## Source workflow and ownership

Pinned source paths:

- `mods/src/settings/upstream_settings.cc:RegisterUpstreamSettings` builds the
  graphics.loader_image ValueSetting and exact action, then decorates it with
  ImageFileSetting when the existing frame anchor accepts polling. Otherwise it
  uses exact path entry with the same asynchronous validator.
- `mods/src/settings/image_file_setting.h` owns the observed revision, picker
  future, status, reset generation, and validation ticket. Only its UI-thread Poll
  applies a verified explicit setting intent.
- `mods/src/settings/image_path_validator.h` owns one filesystem worker, one
  active operation and one replaceable/debounced pending request. Workers own
  UTF-8 paths, never Config or native UI. Process service intentionally outlives
  potentially stalled filesystem work; fixture owners explicitly join.
- `mods/src/audio_file_picker_windows.cc` owns COM STA/IFileOpenDialog, uses an
  unowned dialog, regular-file flags, image filters, UTF-8 conversion and RAII.
  A packaged task avoids std::async future destruction joining the game thread.
- `mods/src/audio_file_picker_mac.mm` posts NSOpenPanel to the main queue and
  resolves an owned promise from its completion callback.
- `mods/src/settings/value_settings.h`, `action_setting.h`, text-entry owner,
  persisted definitions and runtime writer provide observed-write conflict/reset
  semantics and preserving TOML writes. Target equivalents already exist.
- `tests/image_file_setting_test.cc`, `image_path_validator_test.cc` characterize
  actual source models with synthetic picker/owned filesystem probes.

## Behavioral obligations

| ID | Source behavior | Target mapping / adaptation | Verification |
| --- | --- | --- | --- |
| O1 | graphics.loader_image defaults empty; loading or transition owner required; restart-only requested value persists independently of active image. | Existing Config/renderer and MakeConfigDefinition writer, new catalog action. | Production catalog test: no registration I/O, missing owner, key save, requested vs active. |
| O2 | Asynchronous native image picker, owned UTF-8 result; cancellation preserves setting. No filesystem work or joins on UI. | Preserve platform dialog operations and packaged-task policy; image-only adapter avoids unrelated audio migration. | Model fixture; Windows compilation; native dialog interaction pending. |
| O3 | Only one picker per action; explicit choice obtains an observed snapshot; missing/exception/oversized paths do not mutate. | Reuse ImageFileSetting. | Preserved source fixture plus overlong/result ownership cases. |
| O4 | One active filesystem probe, latest pending, debounce, generation cancellation; worker never touches model or game. | Reuse ImagePathValidator unchanged. | Preserved source bounded-worker/debounce/failure fixture. |
| O5 | Concurrent setting edit or default reset invalidates old picker/validation intent, including reset already at default. | Reuse model revision/generation logic and prepared-reset callbacks. | Preserved source stale edit/reset/in-flight probe fixture. |
| O6 | Exact-entry fallback uses the same validator, trimming, empty fallback, changed-validation notification, captured revision, cancellation. | Existing native text-entry owner plus preserved fallback logic; add only its missing validation-notification seam. | Production catalog checks with synthetic editor; native fallback is possible only when that editor's own frame delivery is ready. |
| O7 | Shared frame polling updates presentation; no new hook owner. | Existing FrameDeliveryAvailable/RegisterFrameCallback; lazy service admission avoids catalog registration touching the game. | Synthetic frame admission/delivery tests; native frame/cancellation interaction pending. |
| O8 | Closing/replacing native exact entry revokes page visit and validation; delayed callback cannot overwrite a newer intent. | Existing page-visit/native editor owner, checked again in commit. Picker owns no borrowed view and keeps source's process-level intent semantics. | Delayed commit after departure test; native close/reopen pending. |
| O9 | File is referenced, never moved/copied; regular-file validation does not prove image decoding. | Existing loading renderer remains unchanged. | Source/target renderer comparison; malformed image/fallback trial pending. |

## Integration decisions

Pure models and existing fixtures are copied from pinned source, not reimplemented.
Flat Config becomes nested Config only at the existing catalog/writer adapter.
Polling admission is deferred until explicit invocation: target catalog construction
has a qualified no-game-access contract. Native text-entry already owns GC custody,
Back/Escape, page invalidation and shortcut capture; those owners are retained.

The source decorates a process-lifetime model. Closing a Settings page does not
cancel an external picker by itself; it may still apply the explicitly requested
restart preference if the observed model is unchanged. Preserve and document that
behavior. Do not falsely claim view-bound cancellation or keep borrowed Unity data
inside the picker. Exact-entry is page-bound and must reject stale commits.

The absent-anchor fixture verifies fallback routing with a synthetic editor, not
an actual usable editor without frame delivery. In game, the native editor itself
needs the shared anchor. Rejected picker-callback admission while editor delivery
remains healthy is the meaningful native fallback case; absent shared delivery
means unavailable interaction. Neither case is runtime-qualified yet.

Validation means an available regular file, not a decoded PNG/JPEG. Empty exact
entry uses embedded fallback. Overlong paths and failed probes remain unavailable.

## Validation and recovery

Checkpoints, candidate-bound commands/artifacts, native trial and remaining gaps:
[MIGRATE-PLAY-FEATURE-PILOTS.md](../MIGRATE-PLAY-FEATURE-PILOTS.md).

Guff subsequently confirmed both pilot workflows worked on Windows using the
installed pilot artifact. That basic acceptance does not establish every pending
reset, malformed-file, fallback or native-dialog case. See the linked pilot record.

## Deferred file-selector Clear action

Operator idea2026-10-28: loading-image and sound file selectors should expose a
clearly labeled **Clear** action. Whether it needs confirmation remains an open
UX question. This is deferred, not a change to existing picker behavior. Preserve
each consumer's fallback, save outcome and pending-selection cancellation rules
when designing it. Shared inventory follow-up: `SET-FILE-SELECTION-CLEAR`.
