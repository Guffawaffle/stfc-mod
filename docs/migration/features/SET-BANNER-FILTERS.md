# SET-BANNER-FILTERS — Banner visibility and desktop allowlists

Contract v1; cohort 20261026-01. Source scan: 2026-10-26. Source
`frozen/play-dev-20261026@1baa55fedf1758b19f843cc59707b8901a0b4998`;
target base `05ee3d7f66eec4d9cb39f8924d012a7ca1ec5952`.

Parent: PLAY-NOTIFY-BANNERS. Entry: Mod Settings → Notifications.
Hidden banners: Windows/macOS. Desktop allowlist editor: Windows, matching source
scope. Upstream dev has consumers only; play/play-dev have editors; refactor base
has consumers only. Pinned branch evidence is in the inventory.

## Source workflow

`settings/upstream_settings.cc:Lists` produces two ExactEntry actions;
`config_filters.cc:ParseBannerFilter/FormatBannerFilter` validate and canonicalize.
`config.cc` reads defaults/keys. `patches/parts/disable_banners.cc` and
`patches/notification_service.cc` independently suppress banners and deliver
configured readable desktop notifications. Source visibility does not suppress
audio or delivery. Existing target `features/toasts.cc` already follows that order.

| Obligation | Source behavior | Target disposition / check |
| --- | --- | --- |
| B1 | Separate disabled_banner_types and notify_banner_types settings, empty defaults. | Restore existing-key exact editors; platform guard retained. |
| B2 | Case-insensitive names/All, whitespace and duplicates handled; unknown editor names rejected. | Strict adapter reuses target's one existing ToastState catalog; test canonical order, All, empty, invalid and duplicate names. |
| B3 | Source stores All sentinel plus expanded types; target startup expands known types without sentinel. | Retain target catalog representation; All canonicalizes to known names, semantically identical for existing consumers. No new future-state wildcard claim. |
| B4 | Browsing/validation does not mutate; changes are live, persisted through existing preserving writer. | Reuse source ExactEntry and target page visit/observed revision. Test distinct keys, reset, stale and departed commits. |
| B5 | Hidden banner, audio and desktop forwarding are independent consumers. | Keep current typed toast delivery/suppression order; no hook or notification transport change. |

Source configuration startup tolerates unknown names with warnings. Keep that
behavior distinct from strict editor validation. No tooltip/name-based guessing,
new banner IDs or replacement localization system.

Validation/checkpoints: [cohort record](../FEATURE-COHORT-20261026-01.md).
Native rendering and actual notification delivery remain separate qualification.
No synthetic test sends a real desktop notification or external network message.
