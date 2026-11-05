# SET-WARP-FILTERS — Per-ship route editors

Contract v1; cohort 20261026-01. Source scan: 2026-10-26. Source
`frozen/play-dev-20261026@1baa55fedf1758b19f843cc59707b8901a0b4998`;
target base `05ee3d7f66eec4d9cb39f8924d012a7ca1ec5952`.

Parent: PLAY-WARP-AUTO. Entry: Mod Settings → Map & Travel → Ship route filters.
Windows/macOS. Branch provenance is recorded at exact revisions in the inventory:
upstream dev has consumers only; play/play-dev have these editors; refactor base
has consumers only. This describes inspected local refs, not moving remote heads.

## Source workflow

Complete producer: `mods/src/settings/upstream_settings.cc:Lists`, its shared
ExactEntry/MakeDefinition adapters, `mods/src/config_filters.cc:ParseShipFilter`,
`ship_name_match.h:NormalizeKey/SplitWords/MatchesDisplay`, config reader/defaults,
and `patches/parts/instant_warp_confirmation.cc:ResolveInstantWarpConfirmation`.
Source page registration calls Lists; native text entry and existing preserving
writer own input/commit/persistence. No additional native hook is needed.

| Obligation | Source behavior | Target disposition / check |
| --- | --- | --- |
| W1 | Three separate jump/warp/always-ask lists; empty defaults, existing instant-warp owner. | Restore exact actions over nested Config's existing ShipFilter, registered keys and owner. |
| W2 | Comma lists normalize ship names; standalone `*` means all; mixed wildcard input is rejected by editor. | Use existing target ship-name normalization and matching; startup remains tolerant. Test canonical form, apostrophes/prefixes, empty and mixed wildcard. |
| W3 | Ask wins over jump, then warp, then configured default. | Existing typed instant_warp.cc consumer retained; explain precedence in editor; no route execution rewrite. |
| W4 | Browsing/validation never saves; accepting captures observed revision; cancel/stale edit cannot overwrite newer values. | Reuse source ExactEntry with target page-visit rejection and ValueSetting/writer. Adapter tests. |
| W5 | Defaults/reset/retry/search/help use the same canonical metadata; reopening relevant view may be required. | Source metadata and timing retained; compare registered catalog/writer behavior. |

The source uses normalized joined strings; target already stores word vectors.
Adapt at the text boundary, not by introducing another matching implementation.
Explicitly preserve startup's tolerant manual-config behavior; strict editing
rejects malformed lists before publication. Unchanged presentation must preserve
the observed value and stale-write protection.

Validation and exact candidate/checkpoints: [cohort record](../FEATURE-COHORT-20261026-01.md).
Native text layout, keyboard/Back, and actual course outcomes remain separate
in-game qualification. Existing Ctrl/Command modifier routing is untouched.
