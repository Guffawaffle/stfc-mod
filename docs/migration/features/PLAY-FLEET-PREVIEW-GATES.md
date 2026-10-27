# PLAY-FLEET-PREVIEW-GATES — preview and Escape precedence

Contract v1, 2026-10-27. [Cohort inputs and qualification](../FEATURE-COHORT-20261027-01.md).
Read full frozen `hotkeys.cc` dispatch/CanHideViewer/CanHideViewers/DidHideViewers
and ExecuteSpaceAction, key/mapkey, native visibility/parent/rewards headers,
preview settings and Settings input capture. Target: `features/hotkeys.cc`,
`fleet_actions.cc`, `game/ui.h`, `game/viewers.h`, current input/capture owners.

| Obligation | Mapping | Evidence / limit |
| --- | --- | --- |
| P1 | No focused-text action; Settings capture owns input before actions/mode switches. Native hotkey mode/disabled mod mode leave ScreenManager.Update running. | Existing dispatch/capture implementation retained; existing production matching/capture regression runs. |
| P2 | A target is visible in Show/Visible; primary can additionally require its parent shown. | Typed visibility/parent access matches source; native transitions require player trial. |
| P3 | Hide every shown viewer of the first matching type only, preserving source type precedence. | Actual ViewerList helper with controlled native cards verifies mining is dismissed before underlying pre-scan. |
| P4 | Preview-locate/recall preferences are separate from primary visibility and native cargo behavior. | Source/control/read/write comparison; no config defaults changed. |
| P5 | Golden rewards dismiss on primary or Escape; held Escape must not repeatedly navigate as the panel closes. | Existing edge-based target safety difference retained, rather than restoring source held-key behavior. Native golden panel trial pending. |
| P6 | Discard delayed source actions on focus/context/scene changes rather than invoking another card. | Shared fleet retry ownership; focused dispatch consumes pending work, skipped dispatch expires by the existing frame owner. No global input/pooling redesign. |

Native input, timing and full-screen transitions remain distinct from fixture
helpers and successful startup. This contract does not claim all shortcuts or
every nested game viewer has been player-verified on the new artifact.
