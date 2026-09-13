# Settings polish delivery checklist

The six accepted retrospective improvements are tracked on the shortcut feature
branch. Complete means implementation, review and appropriate evidence; native
checks remain pending until observed. No play promotion is part of this work.

- [ ] 1. Honest unavailable reasons and a quiet persistence-failure notice.
- [ ] 5. Page-owned editor lifetime; native scrolling, Back and focus transitions.
- [x] 4. Native adapter concern separation and feature-owned disabled wording.
- [ ] 6. Measure construction/refresh costs; consolidate current documentation.
- [ ] 2. Task-based navigation and concise heading/action summaries.
- [ ] 3. Shortcut explanations, inspectable overlaps and per-action defaults.

Item 4 shipped in `13e68131`; Windows build, seven fixtures, three independent
review lanes and positive general user smoke recorded. The remaining items follow
the order above.

## Current candidate

Implementation covers items 1, 5, 2 and 3; item 6 has opt-in timing boundaries and
a single [current architecture document](MOD_SETTINGS.md). Unchecked items await
the independent review and native evidence below; implementation alone is not a
completion claim.

- Pure settings fixtures: passed, including out-of-range preservation, default
  drafts, stale restoration and focused release after Alt-Tab cancellation.
- Config writer/adapter fixtures: passed, including failed A / successful B /
  recovered A, failure-only UI notifications and existing quit/force-close paths.
- Windows releasedbg build: passed. No macOS native support claim.
- Native pending: scroll a draft out of view and back; Back discards it;
  conditional row changes and fold/unfold preserve the visit; hold keys through
  Alt-Tab and release after return; inspect all overlaps and stage/cancel defaults.
- Native pending: read summaries/new placement, notice visibility and text fit,
  opening/refresh timing samples. Optimize only after measurements justify it.
