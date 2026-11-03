# SET-VALUES — value controls and exact entry

Umbrella contract v1, 2026-11-03. Status remains **Partial**. This consolidates
the already implemented value owners and their dated evidence; it introduces no
control, setting, callback, test target or product change. Frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`. The current continuation starts at
`80bc425c7c06b36d22566d6e863531c3740fbbc5` on the existing typed branch.

The historical inventory mapping to features/settings_values.cc alone is too
narrow: that file owns choice/slider views, not the entire value workflow.

| Responsibility | Existing typed owner |
| --- | --- |
| Known/unavailable/unsupported observations, generation/value conflicts, apply verification, persistence status and retry | settings/value_settings.h, value_view.h, boolean_settings.h and boolean_view.h |
| Native Boolean row callbacks, pooled controls and native fallback | features/settings.cc, game/settings.h and settings/native_boolean_callback.h |
| Off-screen page publication, Boolean association, page visit and registered value adapters | features/settings_pages.cc and the existing page/catalog owners |
| Single/multiple choice indices/masks and slider range/step/presentation models | settings/choice_setting.h and slider_setting.h |
| Native choice/slider controls, callback identity, rendering suppression, label updates, interactability/group restoration and release | features/settings_values.cc and game/settings_values.h |
| Exact numeric parsing, full original values, canonical configuration keys and registered save callbacks | settings/config_settings.cc |
| One exact-entry request/editor, draft validation, explicit commit/cancel, current page and capture/focus/dismissal custody | settings/native_text_entry.h, features/settings_text_entry.cc and game/settings_text_entry.h |
| Defaults, guarded reset confirmation, explicit save retry and alias deduplication | Existing maintenance owners and [SET-RESET-SAVE](SET-RESET-SAVE.md) |

## Preserved behavior and linked evidence

Boolean and numeric models distinguish a genuine false/zero from an unavailable
observation. A rejected, stale or unverified application cannot claim successful
persistence. Render, bind, refresh and disposal do not authorize writes. Explicit
save retry persists the currently verified value without replaying the feature
mutation; restart timing and external-file conflicts remain visible.

Choices retain canonical native naming/indices, single-select radio semantics
and complete multi-select masks. Sliders retain native snapping only on explicit
input, full-precision model values, and label text derived from the applied
snapshot. Reused/foreign rows, departed pages and reentrant ownership changes
cannot save through an earlier view. These obligations and the reproduced
stale-choice correction are recorded in the
[choice/slider continuation](../FEATURE-COHORT-20261101-CHOICE-SLIDER.md).

The [precision assessment](../SETTINGS-PRECISION-20261028.md) records the
source-preserving readout and late native slider-label correction. Convenience
slider bounds do not narrow the exact value editor's accepted model range.
The [exact-entry assessment](../FEATURE-COHORT-20261101-EXACT-ENTRY.md) maps the
frozen native text-entry owner and qualifies exact original values, invalid/stale
draft refusal, explicit Apply, Cancel/reopen and dismissal-key custody at their
respective model/controlled/native layers. It does not turn unchanged drafts into
rounded slider values.

[SET-RESET-SAVE](SET-RESET-SAVE.md) covers guarded defaults/reset/save retry,
including the later action ownership, cleanup, rebind and render corrections.
Page reset remains a sequence of individually guarded settings rather than
transactional rollback. Shortcut editing and media selection remain with their
own contracts instead of being reimplemented by this umbrella.

## Existing checks and qualification

The existing controlled executable has explicit `settings-choice-consumer` and
`settings-slider-consumer` modes in il2cpp-bindings-tests. They drive production
choice/slider callbacks and native-boundary substitutes; they are not native UI
or hardware interaction. Existing model executables include
boolean_settings-tests, choice_setting-tests, slider_setting-tests,
settings_maintenance-tests, settings_metadata-tests, native_boolean_callback-tests,
native_text_entry-tests and config-settings-tests. Registered source checks
remain in test-mod/test-settings-models as already defined; no registration is
added here. The parent continuation records current reruns separately from the
historical results linked above.

Earlier bounded Windows evidence includes basic precision acceptance, exact
editor invalid feedback/Cancel/reopen/Escape/Back, and the combined action-render
walkthrough linked from SET-RESET-SAVE. Those results stay attached to their
original source/artifact/environment. They do not qualify this whole capability
on the current local source. The historical exact-entry assessment explicitly
left successful live Apply/save/restart open; that sequence still needs its own
evidence before a broader claim. Extended control families, reentrant/pooling and
focus/hardware behavior, and Mac build/runtime qualification remain separate.

The current [Mac launcher slice](../FEATURE-COHORT-20261103-SWIFT-LAUNCHER.md)
restores desktop file actions and addresses entitlement target selection. It is
separate from in-game value controls and does not close this Partial status.
No new feature, AAI integration or parked diagnostic work follows from this
contract consolidation.
