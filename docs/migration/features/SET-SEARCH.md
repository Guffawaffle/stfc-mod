# SET-SEARCH — metadata search and contextual return

Contract version:1. Status: search source/model and bounded controller correction
qualified locally; native interaction and full SET-NAV acceptance remain open.
Exact inputs, observations and receipts: [search cohort](../FEATURE-COHORT-20261101-SEARCH.md).

Frozen1baa55fe retains metadata-only indexing, inline root input, deferred results,
query retention and contextual Back. Target base d2851fee reuses the exact search
model, maintenance catalog and typed search/page owners. The redundant permanent
focus row remains removed per the earlier operator decision. No new surface.

| ID | Obligation / mapping | Evidence and limits |
| --- | --- | --- |
| S1 | Build an index from labels/location/descriptions/real TOML keys without reading preferences; deduplicate multiple controls and expose shortcut model pages once | Existing pure model/controller tests; source model byte-identical |
| S2 | Preserve separators, case-insensitive word matching, stable exact-match ranking, result cap and hidden-section discoverability | Existing SettingsSearch tests and unchanged model; actual native discovery remains unqualified |
| S3 | Retain complete native UTF-8 query for field restoration; native input and model matching limits stay distinct | Initial byte-split failure; final2/3/4-byte controller tests, repeated revision and ASCII cases; extra target byte resize removed |
| S4 | Result click defers page/item navigation beyond its pooled-row callback, checks current same-tree visit and exposes hidden target section without reparenting | Production source trace and controller QueueSettingsPage fixture; live click/scroll target not qualified |
| S5 | Only the current search-opened visit's parent Back transition returns to its retained search context | Frozen/target navigation source mapped; exact native Back remains unqualified |
| S6 | Focus/typing suppresses gameplay and native Back, with cleanup through held-input release; focus recovery retains query | Existing capture/input-release owners and native source trace; real keyboard/Alt-Tab/caret behavior remains open |
| S7 | Clear restores normal categories; unavailable field or route reports failure and allows Dismiss without writes | Existing actual maintenance controller tests, including dialog/route failure paths |
| S8 | Destroyed/replaced controller, stale token or unavailable custody cannot authorize a queued transition | Target weak visit/target and current/publication checks; typed adapter fixtures are controlled runtime only |

The correction restores frozen full-text retention. It does not add a byte-prefix
decoder or change the native character limit or frozen model's matching prefix.
Query input comes from the native field; SetSettingsSearchQuery is not a separate
external input/validation API. Page tree publication and typed field/navigation
services must actually be available before this workflow can work. Boot or source
presence does not establish those rendered/interactive conditions. Full native
SET-NAV staging/menu validation, search typing/result/Back acceptance and current
Mac execution remain separate obligations; no new player-test gate is imposed.
