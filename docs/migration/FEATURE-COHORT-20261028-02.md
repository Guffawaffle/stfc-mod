# Haven, loading artwork and scaling

Nine existing capabilities reconciled from `play-dev-ripper` `e5e0d52f` in the
existing `ripper-foundation/Downstream` worktree. Frozen source:
`1baa55fedf1758b19f843cc59707b8901a0b4998`; historical PLAY:
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`; upstream comparison:
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`.

The [source-preserving procedure](../../.agents/skills/migrate-play-feature/SKILL.md)
was used by three directly coordinated workers and an integrating coordinator.
No Runner native execution. Complete source, supporting bindings, registration,
configuration, Settings controls, shortcut routes, cleanup and existing fixtures
are accounted for in the contracts below. These were not presumed missing ports.

| Existing capability | Source-to-target contract |
| --- | --- |
| Haven zoom | [PLAY-HAVEN-ZOOM](features/PLAY-HAVEN-ZOOM.md) |
| Road/water visuals | [PLAY-HAVEN-VISUALS](features/PLAY-HAVEN-VISUALS.md) |
| Haven map drag/reset | [PLAY-HAVEN-ORBIT](features/PLAY-HAVEN-ORBIT.md) |
| Help history | [PLAY-HAVEN-HISTORY](features/PLAY-HAVEN-HISTORY.md) |
| Login artwork | [PLAY-ART-LOGIN](features/PLAY-ART-LOGIN.md) |
| Transition artwork/tips | [PLAY-ART-TRANSITION](features/PLAY-ART-TRANSITION.md) |
| Interface scale/DPI | [PLAY-UI-SCALE](features/PLAY-UI-SCALE.md) |
| Object-viewer scale | [PLAY-VIEWER-SCALE](features/PLAY-VIEWER-SCALE.md) |
| Loaded/new ship scale | [PLAY-SHIP-MODEL-SCALE](features/PLAY-SHIP-MODEL-SCALE.md) |

## Corrections and explicit decisions

Haven keeps the native-first callback and original camera formula. Exact declared
field classes, successful geometry reads and curve evaluation qualify writes;
unavailable position cannot become a valid zero. Generic IL2CPP semantics are
unchanged. A source defect accepting a nonunit target quaternion is corrected.
History roots the complete snapshot before writes and stops on setter failure;
partial writes have no atomicity/rollback guarantee. The arbitrary source
2000-entry skip is removed under the target no-cap rule. Existing reversible
road/water/orbit ownership remains. System orbit stays excluded. Haven defaults
remain MOUSE2/ALT-MOUSE2: middle button, despite old right-drag inventory wording.

Artwork restores generation asset resets, independent login logos, runtime custom
image precedence, provisional-object cleanup, successful overlay publication
before hiding native art, animator fallback after failed construction, and
slideshow suppression only when an overlay exists. Existing restart timing is
unchanged. Source limitations remain explicit: repeated-show animator policy,
no live-disable restoration, and unqualified managed-reentry publication.

Build behavior now matches source: native-background default;
`bg_image` overrides that build preference; runtime images remain available;
only disabled transition customization forces black. Restored exact source blobs:

- Background: `6766c1761b47c3da49863944f68f0a306bd1044c`.
- Downstream badge: `c5ae0c086e85a03447aef6fff4cdcf77f34033da`.

Scaling preserves DPI, hotkey/save behavior and new-ship multipliers. Failed
loaded-model scale reads no longer write zero transforms. Valid positive settings
can recover an unusable old multiplier without guessing a loaded-model ratio;
nonfinite ratios are refused. Viewer Show uses the full typed signature. Unused
`adjust_scale_res` remains dormant. No settings files were edited.

## Verification and recovery

New fixtures compile the production Haven, orbit, artwork, tips and scaling
consumers in the existing controlled IL2CPP executable. Existing Haven visual,
Settings, preset, queue, camera, travel, fleet/chat and AAI-host regressions remain
included. This does not render Unity or establish actual gameplay/hook fit.
Native-original, embedded and custom build-option variants need distinct checks.

Evidence: `D:/dev/stfc-workspace/artifacts/haven-artwork-scale-20261028/`.
`inputs.json` records exact refs and execution attribution. Earlier failed logs
remain: first test integration targeted the wrong executable and hit Windows
`far` macro expansion; second exposed stale fixture method-table entries and
inherited Component setup. These are fixture defects, not observed game failures.
Final passing checks and runtime acceptance will be appended after qualification.

No new hooks, UI framework, analytics, persistence, settings redesign or progressive
loading. No push, promotion or release. macOS build/player checks remain separate
from Windows results and historical CI. The prior warp repair is the accepted
runtime until this candidate passes its own build and native trial.

### Integrated offline qualification

With all production and fixture edits settled, Zig 0.16 / Windows x64 MSVC,
ReleaseFast dependencies and Debug fixture assertions passed **416/416**
build/run/install steps (`tests-3.log`). The command included `test-il2cpp`,
`test-mod`, `test-haven-visuals`, `test-aai-host` and `build-tests`, using unchanged
AAI `4bece2e5f1afa7c7343f009e83398742a0f86114`. This is a step count.
The default/native-background executable SHA256 is
`476b3fdcb7d7a5059f415502c5c71013017c8bcd7e3df8f8d8581bddfeff32ee`.

Explicit `-Duse_original_bg=false` passed **302/302** steps, and
`-Duse_original_bg=true -Dbg_image=assets/guffawaffle-build-badge.png` passed
**302/302** (`tests-embedded.log`, `tests-custom-build.log`). The initial custom
command lost its filename extension during PowerShell argument parsing; quoting
the complete argument corrected the invocation without changing source. Its
failed log is retained as `tests-custom-argv.log`.

`source-inputs.json` lists changed compiled inputs and hashes; readback after all
three checks found no source changes. `offline-qualification.json` binds the
passing logs, installed fixture executable and limits. Neither the fake image
decoder nor these metadata fixtures establish native rendering, scene behavior,
hook fit, or player/Mac acceptance. No check is attributed to Runner execution.
