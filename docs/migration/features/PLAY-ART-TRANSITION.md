# PLAY-ART-TRANSITION — Transition artwork, black mode and tips

Contract version: 1. Status: implemented; controlled-consumer checks passed; native qualification pending.

## Inputs and scope

- Frozen source: `Guffawaffle/stfc-mod`, `frozen/play-dev-20261026`,
  `1baa55fedf1758b19f843cc59707b8901a0b4998` (verified, not advanced).
- Target: existing `worktrees/ripper-foundation/Downstream`, `play-dev-ripper`,
  base `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` plus this bounded working delta.
- Inventory: `PLAY-ART-TRANSITION`; depends on the shared assets/overlay helper
  described in [PLAY-ART-LOGIN](PLAY-ART-LOGIN.md).
- Authorized: preserve transition/black/tip/lifecycle behavior, narrow typed
  adapters and focused production-consumer fixtures. No new UI framework,
  slideshow framework, progressive loading, deployment or publishing.
- Source scan: 2026-10-28; shared Windows/macOS path, platform qualification pending.
- Cohort and signed-checkpoint handoff: [Haven, loading artwork and scaling](../FEATURE-COHORT-20261028-02.md).

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Config/gameplay consumer | `mods/src/patches/parts/transition_screen.cc`, `loading_tip.cc`, common image helper present. |
| play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Control registration + config/gameplay consumer | Artwork paths and loading controls in `mods/src/settings/upstream_settings.cc` present; transition/helper differ from frozen source. |
| play-dev / frozen | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Control registration + config/gameplay consumer | Complete implementations and shared lifecycle/config/producer/control paths inspected. |
| refactor | `e5e0d52ff40331072c4e9c008e3bcaee4a7e913c` + working delta | Control registration + config/gameplay consumer | Artwork in `src/mod/features/loading_screens.cc`; existing `settings/config_settings.cc` controls retain restart timing. Registration is not rendered-control qualification. |

## Source workflow and ownership

Restart-only `[patches].transitionscreenhooks` and `loadingtiphooks` both default
to true. Feature settings `[graphics].loader_transition` and `loader_tip_enabled` also default
to true; `loader_transition_black` defaults false and is forced true only when
transition customization is disabled in the frozen source. Settings choose
behavior in hooks; they do not decide hook installation.
The existing native Settings catalog marks these controls for restart; hooks
still read the current Config rather than retaining a configuration reference.

Native Awake runs first, then enabled transition customization resets its cached
assets/state and attempts discovery. AboutToShow calls native first and retries
until customization completes. Discovery prefers `_staticOverride`; otherwise
it finds direct `BGContainer` and its first child's Image. Missing discovery is
not a successful customization.

Black mode preserves the native background, disables its canvas animator and
adds both logos. Without a usable replacement asset, nonblack mode adds logos
only and preserves native animation/background. If an asset exists but overlay
construction fails, native background/animation remain untouched and a later
AboutToShow retries. A successful replacement is first sibling at the canvas
root, hides native background, stretches/straightens the background container,
adds logos and places the native game-logo/tips containers. AboutToHide restores
the animator this customization disabled before calling native hide.

SlideShow.ShowCurrentSlide runs native first, then hides its Image only when a
custom background overlay actually exists, nonblack customization is enabled.
PrepareAllForReload clears scene state and resets tips before native reload.
Scene hierarchy owns published overlays; transient construction owns cleanup.
See the login contract for complete pinned implementation/helper/producers read.

## Behavioral obligations

| ID | Source behavior and evidence | Target mapping / necessary adaptation | Verification method and result |
| --- | --- | --- | --- |
| T1 | `TVC_Awake_Hook`, `TVC_AboutToShow_Hook`: native first, live gate, per-generation reset, deferred discovery retry. | Restore asset reset at enabled Awake; retain typed callbacks. Weak controller identity prevents a different controller inheriting completed/overlay flags when enabled after Awake. | Actual disabled, failure/retry, repeated-show and replacement-controller callback cases passed. |
| T2 | `FindTVCBGImage`: static override, then BGContainer/first child; absence leaves retry pending. | Existing discovery retained; guard a failed RectTransform cast before placement. | Source comparison; native hierarchy trial pending. |
| T3 | `ApplyTransitionCustomization`: no asset means logos only; existing asset + failed overlay keeps native artwork and animator. | Move animator disabling after successful replacement, keep no-asset path, restore provisional native-object cleanup. | Managed component/parent/color failures, retry and no-overlay slideshow production-consumer cases passed. |
| T4 | `DisableTransitionAnimator` records only a successful disable; AboutToHide restores that animator. | Weak handle retains identity without retaining scene; only the owning controller can restore it. | Consumer disabled/enabled-state and original-order assertions passed; real native animation pending. |
| T5 | Successful overlay hides native image, stretches/straightens fallback container, preserves source logos and native logo/tip rectangles. | Existing layout formulas retained; checked overlay placement prevents partial construction being published. | Source comparison and final Image consumer assertions; rendered geometry remains pending. |
| T6 | `SlideShowViewer_ShowCurrentSlide_Hook` requires `g_bgOverlayGO`. | Restore the missing `transition_screen.overlay` guard. | Actual callback before failed replacement kept native alpha; passed in embedded/custom variants. |
| T7 | Reload forgets stale scene references and resets login/tip state before native reload. | Shared terminal `InstallReloadReset` owner is retained; dependent installers now refuse when its installation fails. State contains only weak scene identities; image roots reset at subsequent enabled Awake. | Callback ordering/reset cases passed; real detour/reset availability remains pending. |
| T8 | `loading_tip.cc`: welcome once per reset, then 50% native pass-through, custom weights 15/10/15/50/10, no immediately repeated custom tip even across native passes. | Reuse existing typed TipPicker and identical catalog/versioned welcome; no deterministic RNG-sequence equivalence claim. | Seeded actual tip callback cases passed: native/custom branches, catalog, nonrepeat, disabled, null label and reload welcome. |
| T9 | Config/build background selection leaves runtime `loader_image` usable in original-background builds. | Embedded-only fallback guard plus coordinator source-default, config and producer corrections. | Native-original default, explicit embedded and true-plus-custom build variants passed; integrated config policy checks passed. |

## Design and behavior decisions

This change retains the single feature file, source state-machine order, existing
typed game/Unity bindings, shared images and single reload detour. It adds no
host services or new UI architecture. Weak controller/animator identities replace
unqualified cross-instance flags; they do not keep a former scene alive.

Intentional safety adaptation: overlay creation checks contained constructor,
parenting, layout and Image-setting outcomes, and destroys failed constructed
objects. Missing observations cannot authorize hiding native artwork.

Preserved source limitations: black mode uses native background rather than
painting a new black image; live disable does not restore existing overlays.
The source disables an animator once and re-enables it for hide, without a new
disable after every already-customized show; this cohort does not redesign that
animation policy. Slideshow suppression is source-global after successful
replacement, not newly scoped to a slideshow subtree. Missing logo assets can
leave a source-completed transition without both logos. These are not accidental
typed migration claims or current-client qualifications.

Reentry through Unity component/setter callbacks remains a qualification limit.
The weak controller identity guards outer callback routing; it is not a
generation fence around every managed call in customization. This cohort does
not claim cancellation-safe publication if native reload/rebinding occurs
inside such a call, and does not add a broader publication framework. No current
game failure for that path was established.

## Validation evidence

`tests/loading_artwork_cases.h` includes the production feature and Unity asset
helper; `tests/loading_tips_cases.h` uses its production tip/reload callbacks.
Managed bodies, scene hierarchy, native decoder and File UTF-8 path boundary are
synthetic. These are production-adapter/final-consumer observations, not rendered
Unity tests, actual hook installation, boot smoke or player verification.

The coordinator integrated `loading-transition-consumer`, `loading-login-consumer`
and `loading-tips-consumer` in `il2cpp-bindings-tests`. The frozen source and
current working inputs are bound by `inputs.json` / `source-inputs.json` in
`D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/` against target base
`e5e0d52ff40331072c4e9c008e3bcaee4a7e913c`. Windows MSVC-ABI execution used pinned
Zig 0.16.0. The restored native-original default passed integrated checks in
`tests-3.log` (416/416 build steps). Explicit `-Duse_original_bg=false` passed
`tests-embedded.log` (302/302), and original=true plus explicit `-Dbg_image`
passed `tests-custom-build.log` (302/302). All three production artwork/tip
consumer modes passed in each variant. Exact background/badge blobs and native
default were restored; see the login contract for their identities.

The initial custom invocation failed from PowerShell argument quoting/truncated
filename (`tests-custom-argv.log`); quoting the native argument passed with
unchanged source. This was not a product failure. Lane-owned static
`git diff --check` was clean. No lane build, game cycle or native LexRunner
execution occurred. Current Mac binary/ABI/static extent/runtime, real native
rendering and player evidence are not supplied by these fixtures. The cohort
record is the handoff for the next signed source checkpoint and subsequent
artifact qualification.

Coordinator `tests-2.log` exposed fixture-adapter failures: replacements for
existing instance `GameObject.GetComponent` and `Object.get_name` changed only
`methodPointer`, while typed native instance calls use `virtualMethodPointer`.
The controlled replacements now update both entries, matching the existing
`AddMethod` fixture contract. This correction changes no production behavior or
assertion scope. The corrected adapters passed `tests-3.log` and both explicit
background variants; the failed run remains historical failure evidence.

## Disposition and recovery

- Migration coverage: source workflow and defaults mapped; concrete fallback,
  lifecycle, partial-construction and slideshow regressions corrected; controlled
  production-consumer checks passed across all three background variants.
- Feature correctness: current-client hook fit, native animation/rendering, player
  acceptance and both Mac architectures remain unqualified.
- Trial: black/custom/original modes; missing hierarchy then later discovery;
  show/hide repeatedly; custom decode/construction failure; section changes and
  slideshow visibility; fresh/reloaded welcome and subsequent tips.
- Next: bind the signed source checkpoint in the cohort handoff, then run
  exact-candidate Windows/macOS trials. Keep overall parity incomplete until
  native/platform and player qualification is recorded.
