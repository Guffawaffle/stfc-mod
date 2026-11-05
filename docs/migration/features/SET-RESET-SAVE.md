# SET-RESET-SAVE — defaults, reset and explicit save retry

Contract version: 1. Status: preserved source/controller and Windows fixture
qualification complete; actual native interaction and Mac acceptance remain open.
Exact revisions, branch observations and receipts: [reset/editor cohort](../FEATURE-COHORT-20261101-RESET-SHORTCUTS.md).

The later [action ownership continuation](../FEATURE-COHORT-20261101-ACTION-OWNERSHIP.md)
corrects stale command dispatch after reentrant ownership observations. Controlled
production action/choice/slider cases pass; controller policy is unchanged. No
native interaction or reset/save acceptance is added while Guff retains game control.

The [action cleanup continuation](../FEATURE-COHORT-20261101-ACTION-CLEANUP.md)
qualifies component ownership, retry progress and deferred release in controlled
consumers. Live pooling/editor and Mac qualification remain open.

The [deferred rebind continuation](../FEATURE-COHORT-20261101-ACTION-REBIND.md)
recovers a fresh rendered action view after cleanup waits for a callback. It
qualifies retry and stale-request cancellation in controlled consumers; native
Windows interaction and Mac qualification remain open.

The [rendering continuation](../FEATURE-COHORT-20261101-ACTION-RENDER.md)
checks attached components after native reads and records an exact-artifact
combined Windows smoke of sliders, choices, editor cancellation and reset/help
actions. Timing-specific races and Mac qualification remain open.

Frozen source: mods/src/settings/maintenance.h and value_settings.h; original
native page_navigation.cc registers maintenance pages. Target base2195077f keeps
both headers byte-identical and registers through BuildModPages ->
RegisterMaintenanceTools -> RegisterMaintenancePages. The existing native
text-entry owner supplies page/capture custody; model owners supply reset guards.
No production change was required.

| ID | Obligation / target mapping | Evidence |
| --- | --- | --- |
| R1 | Browsing/default/help/registration are read-only; metadata aliases deduplicate by TOML key and exact numeric owner takes precedence over slider | Existing maintenance/controller tests, including full-range value outside slider bounds |
| R2 | Reset prepares a confirmation against current known model values; page reset captures all eligible plans, reports skipped controls and validates all before its first mutation | Existing page reset/value/session/unavailable cases; production helper unchanged |
| R3 | Cancel and stale session/value/availability reject before writing | New real-file canceled/stale reset checks plus existing page/individual reset tests; native text-entry session/page guard fixtures |
| R4 | Reset uses the declared model default and existing apply/persist policy; restart/reopen timing stays explicit | Existing canonical model and maintenance tests; current native text-entry request/readback source |
| R5 | Verified live application is separate from persistence; external-file conflict stays Conflict, pending intent survives reload and explicit retry does not reapply the feature | New real-file reset -> conflict -> retry/reopen, prior runtime reload coverage and per-key controller tests |
| R6 | Failure notice/refresh and unavailable editor report real state rather than reset success | Existing maintenance page and native text-entry/controller fixtures |

Page reset is a sequence of individually guarded settings, not a transactional
multi-setting rollback: a failure/reentry during application can leave earlier
verified settings applied. Pre-confirmation staleness is checked across all plans
before any application. Retry does not silently rebase an external conflict.
Shortcut pages own their separate defaults/draft workflow and are not duplicated
by maintenance. Native frame/editor/callback services are prerequisites, not
established by a substituted OpenTextEntry function. Windows fixtures do not
qualify live clicks, rendered confirmations, unavailable real hosts or Mac.
