# PLAY-HAVEN-ENTRY — Go to Haven

Discovered as a separate product capability during cohort03; parent
PLAY-SCREEN-ACTIONS. Source entry exists in historical PLAY `cad98ef7`, upstream
`e884efa7` and frozen play-dev `1baa55fe`: hotkeys.cc ShowHaven calls
`prime/PlanetaryBaseManager.h::ViewOwnHaven`. Target base `266a688c` routes
`screens.cc::kScreens` to `game/fleet.h::PlanetaryBaseManager::ViewOwnHaven`.

ALT-H is the default editable `shortcuts.show_haven` binding, displayed as
"Open Haven" in the shortcut catalog. It respects normal hotkey/capture/text
focus gates. The Gateway readiness/message check precedes native section change;
own-Haven navigation uses a null visitor context. The feature has no deferred
mod task, scene owner or separate hook group.

Guff reported arriving without UI, then successfully retested from Station and
another Haven on the already installed113f13ce build. No code/deployment change
occurred between those observations. Basic current Windows use is accepted;
the earlier intermittent report, cold startup and Mac behavior remain distinct.
No UI restoration hack or duplicate keybind was added. Future reproduction must
capture origin/current section, readiness, native acceptance and resulting UI
before assigning a cause. Escape/Back changes remain deferred.

Native/source investigation and exact artifacts:
[cohort03](../FEATURE-COHORT-20261028-03.md). Source parity and native UI
correctness are independent; successful camera arrival alone is not completion.
