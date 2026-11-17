# PLAY-ART-LOGIN — Login artwork and logos

Contract version: 1. Status: implemented; controlled-consumer checks passed; native qualification pending.

## Inputs and scope

- Frozen source: `Guffawaffle/stfc-mod`, `frozen/play-dev-20261026`,
  `1baa55fedf1758b19f843cc59707b8901a0b4998` (verified, not advanced).
- Target: existing `worktrees/ripper-foundation/Downstream`, `play-dev-ripper`,
  base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` plus this bounded working delta.
- Inventory: `PLAY-ART-LOGIN`; parent artwork domain shared with `PLAY-ART-TRANSITION`.
- Scope: preserve login artwork behavior through typed bindings. No new settings UI,
  image-picker workflow, image assets, deployment, game cycle, or publishing.
- Source scan: 2026-10-28. Windows/macOS share the implementation; no platform runtime
  qualification is inferred from the source comparison.
- Cohort and signed-checkpoint handoff: [Haven, loading artwork and scaling](../FEATURE-COHORT-20261028-02.md).

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Config/gameplay consumer | `mods/src/patches/parts/loading_screen.cc`, `loading_screen_common.h` present; not an in-game control. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Control registration + config/gameplay consumer | Artwork paths and `mods/src/settings/upstream_settings.cc` loading controls present; blobs differ from frozen source. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Control registration + config/gameplay consumer | Complete loading implementation/helper, registration, config/defaults, image producer and relevant settings fixtures inspected. |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` + working delta | Control registration + config/gameplay consumer | Artwork in `src/mod/features/loading_screens.cc`; existing `settings/config_settings.cc` controls retain restart timing. Registration does not qualify rendered control availability. |

## Source workflow and ownership

The restart-only `[patches].loadingscreenhooks` defaults to true. After native
`LoginSequence.Awake`, `[graphics].loader_enabled` (true) gates scene changes.
Enabled Awake forgets old logo references and releases cached image roots, reads
`_mainCanvas`, then searches child Images through depth four. Named backgrounds
win; otherwise the first discovered Image is used.

The runtime UTF-8 `loader_image` path is tried first. An absent/unreadable/failed
custom image falls back to embedded artwork unless `_USE_ORIGINAL_BG` is defined.
Custom runtime images remain allowed in that build. An explicit build-time
`bg_image` overrides `use_original_bg` in the frozen Xmake producer. Both logos
are attempted independently of background availability. `loader_logo_scale`
defaults to 1 and scales their 1920-pixel reference layout.

The frozen Loading settings page also registers toggles, exact logo scale and
an image action with restart timing. Its picker has owned-path validation,
cancel/reset/stale-edit fences and an exact-editor fallback when shared frame
delivery is absent. The target already has this separate control owner in
`src/mod/settings/config_settings.cc`; this lane reads that boundary without
changing it or claiming new rendered Settings coverage.

Pinned implementation: `mods/src/patches/parts/loading_screen.cc` (complete),
`loading_screen_common.h` (complete), `transition_screen.cc` and `loading_tip.cc`
(complete shared lifecycle consumers); `mods/src/patches/patches.cc` registrations,
`mods/src/config.{h,cc}`, `defaultconfig.h`, `mods/xmake.lua` and `xmake/options.lua`
for settings and image producers; the complete loading-control registration and
its picker/fallback path in `mods/src/settings/upstream_settings.cc`, and complete
`image_file_setting.h` / `image_path_validator.h`. The frozen `tests/image_file_setting_test.cc`
and `image_path_validator_test.cc` cover picker admission/cancel/staleness, not
the artwork consumer; no frozen native artwork-consumer test was found.

Assets hold strong managed handles for a generation. The scene canvas owns
published native overlays. Failed constructed overlays must be destroyed, then
their temporary managed roots released. Source generation reset releases roots;
it does not explicitly destroy the old scene's textures or published overlays.
Scene replacement owns that teardown. No artwork preference is written here.

## Behavioral obligations

| ID | Source behavior and evidence | Target mapping / necessary adaptation | Verification method and result |
| --- | --- | --- | --- |
| L1 | `LS_LoginSequence_Awake_Hook`: native Awake runs first; disabled means no custom work. | Existing typed hook retained. | Production-consumer disabled/original-call case passed in all three build-option variants. |
| L2 | Enabled Awake resets cached loading assets for the new login generation. | Restore `screen_images = {}` before discovery. It releases typed `ImageAsset` roots, allowing changed files to be decoded while previous wrappers are still alive. | Repeated-login and changed-file production-consumer cases passed. |
| L3 | `FindLoginBGImage`: known-name preference, first-image fallback, depth four. | Reuse existing recursive typed implementation. | Full source comparison; exact hierarchy/runtime trial pending. |
| L4 | `GetLoadingAsset`: custom file first, embedded fallback, original-background builds omit only embedded fallback. | Place `_USE_ORIGINAL_BG` guard in `Background()`; allow a successfully decoded custom image to reach login in every build. | Unicode custom path, changed file and failed-decode cases passed with native-original, explicit embedded and explicit custom embedding. |
| L5 | `ApplySpriteToImage` sets override sprite, sprite, white color, Simple type, nonpreserved aspect and dirty vertices. | Existing typed Image operations; overlay configuration now observes setter success. | Production callback fixture exercises the final Image; rendered appearance pending. |
| L6 | Both source logo helpers run even without a replacement background; logo anchors use source width/padding/aspect/scale formulas. | Decouple canvas/logo discovery from background loading; preserve layout formulas and image assets supplied by the producer. | Production two-overlay assertions passed across variants; size/resize visual trial pending. |
| L7 | `CreateImageOverlay` roots construction, rejects failed constructor/parent/component/configuration, destroys failed constructed objects. | Typed Allocate plus checked constructor preserves the source distinction: ordinary `New` contains exceptions but returns its allocation. Local provisional guard destroys only successfully constructed unpublished objects. | Actual overlay failure cases passed for managed custody, constructor, parenting, Image creation and configuration. |
| L8 | Assets are shared with transition screens and refreshed on enabled TVC Awake too. | Both consumers reset the same `ScreenImages`; no parallel asset manager. | Transition production-consumer cases passed; native runtime reload pending. |

## Design and behavior decisions

The target keeps the existing Unity `ImageAsset`, `GcHandle`, component lookup,
canvas lookup and typed calls. No decoder, generic UI framework, polling loop or
new background producer was added. Actual decoded texture dimensions replace the
old helper's PNG-header dimensions/fallback constants; valid decoded images retain
their aspect. Unsupported input still takes the existing native decode failure
and fallback path.

Preserved source limits: a live setting change does not remove existing artwork;
the next enabled login/transition generation applies it. Repeated Awake assumes
native scene replacement owns the old overlays. Source stores are not edited.

The starting Zig producer embedded `assets/official-cc-logo.png` as the left
logo, while frozen Xmake embedded `assets/guffawaffle-build-badge.png`. The
coordinator restored the frozen downstream badge producer, the build-option
override interaction, and removed the target-only config coercion to black under
`_USE_ORIGINAL_BG`. Logic coverage alone does not establish rendered asset parity.
The cross-check also found the target's original-background option default and
embedded background blob differed from frozen PLAY. The coordinator restored
the frozen true default and exact `assets/loadingscreen.png` blob
`6766c1761b47c3da49863944f68f0a306bd1044c`. The badge blob is also exactly frozen
`c5ae0c086e85a03447aef6fff4cdcf77f34033da`; both identities were independently
rechecked after restoration.

## Validation evidence

The added `tests/loading_artwork_cases.h` includes the actual production feature
and actual Unity `ImageAsset`. It substitutes managed method bodies, the native
hierarchy and decoder, and the File UTF-8 path helper. It reads a real temporary
Unicode path. Fake byte markers establish which bytes the consumer acquires;
they do not validate Unity's PNG decoder, rendered artwork, native hook extents,
or a real game scene. Shared hook transactions are not installed by these cases.

The coordinator integrated `loading-login-consumer`, `loading-transition-consumer`
and `loading-tips-consumer` into `il2cpp-bindings-tests`. Current working inputs
are recorded in `D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/source-inputs.json`,
bound to base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` and the frozen source.
Windows MSVC-ABI checks used the pinned Zig 0.16.0 toolchain:

| Variant | Coordinator evidence | Result |
| --- | --- | --- |
| Restored native-original default | `tests-3.log` | Integrated checks: 416/416 build steps succeeded; all three artwork/tip modes passed. |
| Explicit `-Duse_original_bg=false` | `tests-embedded.log` | 302/302 build steps succeeded, including all artwork/tip consumer modes. |
| `-Duse_original_bg=true` plus explicit `-Dbg_image` | `tests-custom-build.log` | 302/302 build steps succeeded, including custom-image precedence and consumer modes. |

An initial custom-option invocation in `tests-custom-argv.log` failed because an
unquoted PowerShell native argument truncated the filename. The quoted retry
passed with unchanged source. The earlier stale fixture instance-method entries
were corrected before `tests-3.log`; no passing claim is based on the failed runs.
The lane's owned-file `git diff --check` also passed. Native rendering, a full mod
boot, player behavior and Mac qualification remain separate and pending; none is
implied by these controlled-consumer results. The cohort record is the handoff
for the next signed source checkpoint and subsequent artifact qualification.

## Disposition and recovery

- Migration coverage: source behavior mapped; concrete asset lifetime, fallback
  and provisional-overlay regressions corrected; controlled consumer checks passed
  across native-original, embedded and custom build-option variants.
- Feature correctness: native image decoding, both visual logos, scaling/resizing,
  current-client hook fit and Windows/macOS scene behavior remain unqualified.
- Trial: valid/invalid/missing/Unicode image; original and custom backgrounds;
  repeated login and changed file; both logos at multiple resolutions and scales.
- Next: bind the signed source checkpoint in the cohort handoff, then attach
  exact-client/platform trials before parity completion.
