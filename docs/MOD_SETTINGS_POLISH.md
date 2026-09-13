# Settings polish delivery checklist

The six accepted retrospective improvements are tracked on the shortcut feature
branch. Completion requires implementation, review and appropriate evidence;
fixture and native observations are distinguished below. No play promotion is
part of this work.

- [ ] 1. Honest unavailable reasons and a quiet persistence-failure notice.
- [x] 5. Page-owned editor lifetime; native scrolling, Back and focus transitions.
- [x] 4. Native adapter concern separation and feature-owned disabled wording.
- [x] 6. Measure construction/refresh costs; consolidate current documentation.
- [x] 2. Task-based navigation and concise heading/action summaries.
- [x] 3. Shortcut explanations, inspectable overlaps and per-action defaults.

Item 4 was complete at `13e68131`, with a Windows build, seven fixtures, three
independent review lanes and a positive user smoke check. The remaining
implementation is in `37243db0` and its correction `802b1b18`.

## Validation on 2026-09-13

The [current architecture document](MOD_SETTINGS.md) records present contracts;
the original foundation document is explicitly historical. General, hostile and
independent review lanes cleared the final implementation at `802b1b18` after
corrections to failure-observer registration, notice-only page pruning and
thread-start failure recovery.

- Seven settings fixtures passed, including out-of-range preservation, default
  drafts, stale restoration and focused key release after Alt-Tab cancellation.
- Config writer/adapter fixtures passed, including failed A / successful B /
  recovered A, thread-start failure and retry, observer delivery when persistence
  is unavailable, source preservation/conflicts and existing quit/force-close paths.
- The Windows releasedbg build and `git diff --check` passed. Three-lane review
  covered the final code delta. No macOS build, native run or CI result is claimed
  for this polish delta.

Native observations used the AX-deployed Windows x64 artifact from `802b1b18`:
DLL SHA-256 `4BF575617E6FE780D8F3524CB48D2D385B9AEAA6CCE6FB65A23D5B52EAFF4075`,
client GameAssembly SHA-256
`487AF4BB9C697C353BE9714359A97DDDCECE5DAB872622A6C498A27BBFC44F40`.
Startup registered 123 pages. That is an observation of this enabled feature
set, not a fixed count or supported limit.

| Check | Observation |
| --- | --- |
| Navigation and summaries | Camera, Fleet Labels and Map & Travel revisited; current binding summaries and Restore control confirmed. |
| Draft lifetime | Staged changes survived scrolling and folding; Back and reopening discarded the draft. |
| Recording and focus | Holding Shift across Alt-Tab and releasing after return canceled recording without changing a binding or triggering a game shortcut; a fresh Change then Esc also canceled. |
| Overlap inspection | A Ctrl+Alt+C draft in Pan left let Next cycle between Clear action queue and Open side chat 1; the draft was canceled. |
| Conditional rows | Cargo target rows hid and reappeared when Automatically open cargo changed OFF/ON. |
| Save failure | A deliberate external edit of one camera key caused a real writer conflict. The live slider remained usable and the amber failure notice fit without truncation. The temporary file value was restored through a guarded edit of that key only. |

Native confirmation that a later successful save removes the notice is pending.
The loaded out-of-range message and preservation have fixture evidence; no native
out-of-range screenshot is claimed. Restore staging/cancellation was exercised
natively; replacement of the complete default list is covered by guarded draft
fixtures. These checks do not establish every game profile or pooling transition.

## Bounded timing capture

Opt-in measurements on the same artifact recorded the following existing
settings operations during the checks above. Means are weighted by sample count
from the log's rounded aggregates.

| Operation | Samples | Mean (ms) | Maximum (ms) |
| --- | ---: | ---: | ---: |
| Build settings tree | 2 | 12.30 | 13.01 |
| Show/filter/bind page | 119 | 3.60 | 10.10 |
| Refresh action rows | 45 | 3.54 | 9.69 |

These are inclusive operation durations on one client, with nested work; they
cannot be added together, treated as whole-frame times or used to establish p95.
There is no before/after baseline or attribution to individual binding copies.
This sample does not justify a binding snapshot cache, so authoritative reads
remain. Timing is opt-in and disabled for normal launches; no additional timing
hook or frame callback was introduced.
