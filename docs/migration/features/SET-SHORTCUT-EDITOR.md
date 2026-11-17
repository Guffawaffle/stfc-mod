# SET-SHORTCUT-EDITOR — capture, proposal, confirmation and Undo

Contract version: 1. Status: frozen workflow mapped and current Windows
controller/dispatcher fixtures pass; native UX and Mac remain open.
Exact revisions, source diffs and receipts: [reset/editor cohort](../FEATURE-COHORT-20261101-RESET-SHORTCUTS.md).

Frozen source: mods/src/settings/shortcut_settings.cc, shortcut_draft.h,
shortcut_undo.h, value_settings.h and shortcut_capture.h. Target base2195077f
retains draft/Undo/value algorithms byte-identically. Existing shortcut_settings.cc
adapts actual MapKey bindings to Shortcut's generation-owned snapshots and the
shared typed native text-entry/page/capture owners. BuildModPages registers it;
the old inventory description "Parser/dispatch only" is superseded.

| ID | Obligation / mapping | Evidence |
| --- | --- | --- |
| E1 | Registered actions are discoverable with labels/groups/defaults, including unbound actions; unavailable hooks/tiers/native mode stay unavailable | Existing catalog/editor/controller tests and actual FeatureInstallations predicate |
| E2 | Recording drains opening input, uses existing parser/key identity/modifier families, supports Record again and stages without mutation | Actual Recording request exercised by config_settings_test; pure capture/release guard tests |
| E3 | Exact entry and recording share strict chord parsing; duplicates do not become new alternatives; overlap warnings inform explicit confirmation | Existing production controller tests and draft semantic-comparator cases; source mapping of Overlaps |
| E4 | Apply requires current generation/page/model; unrelated global replacement or a departed page revokes pending intent | Existing global generation case and new staged-proposal/page-leave/delayed-confirm case |
| E5 | Explicit cancel and capture/page/focus loss retain input ownership through release and do not save | New proposal-cancel and existing delayed-record commit tests; native TextEntry frame/cleanup and capture guard fixtures |
| E6 | Remove/default/unbind use the same verified owner and writer; Undo restores exact order/sides/duplicates but refuses later edits | Existing real controller/dispatcher and pure Undo tests; explicit NONE serialization |
| E7 | Bounded native row presentation does not truncate existing long TOML lists or authorize growth; exact deletion/Undo may preserve them | Existing oversized/duplicate controller and row budget cases |
| E8 | Disk receipt and session binding are distinct; observers update status on the owner thread | Existing production editor invokes registered-key save/status adapter; separate real-file persistence integration qualifies writer boundary |

The target's already-present Stage -> page Confirm flow differs from the source
popup's proposal/Confirm presentation. This assessment introduces no redesign and
does not claim identical native interaction. Global generation guards and page
session invalidation retain the existing target improvements. Exact entry/unbind
remain existing target functionality; no new UI feature was added here.

Native text entry requires actual frame delivery, current page visit, invoking
widget custody, focus and shared capture availability. Available editor metadata
does not prove the native surface can open. Controlled fixtures replace native
surfaces/callback delivery and persistence endpoints; actual clicks, key hardware,
focus transitions and final rendering remain unqualified. Source and target do
not add a dedicated shortcut save-retry row in this slice; failure text is not
evidence that a retry was performed. System orbit remains parked.
