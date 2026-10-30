# PLAY-GAME-CONFIG — native settings/assets endpoint overrides

Source scan: frozen1baa55fe, full relevant testing.cc implementation including
its embedded Model/AppConfig bindings, config reads and patches.cc registration.
Target: features/game_config.cc, game/app.h, config.h/.cc and the existing feature
registry. Exact refs and evidence: [cohort03](../FEATURE-COHORT-20261029-03.md).

| Obligation | Preserved behavior / adaptation | Qualification |
| --- | --- | --- |
| G1 | LoadConfigs original runs once before app-config acquisition | Actual hook consumer with controlled native original |
| G2 | Empty settings_url/assets_url_override independently preserve native values | Empty, each-only and both supplied cases |
| G3 | Set settings first; reacquire current configuration before the assets decision, as frozen source does | Reentrant native setter changes Config provider; before fixture fails, after passes |
| G4 | No app config means no override | Original removes config; no setter called |
| G5 | Preserve exact supplied strings; no URL normalization, network request or persistence mutation | Fixture strings only; no real endpoint tested |
| G6 | Keep managed receiver/config/text alive across native calls and decline a stale second write after model rebind | Operation-local GC handles, replacement fixture, roots released |

Startup [patches].testpatches defaults true. Existing [config] settings_url and
assets_url_override default empty. This is a configuration hook, not a new
in-game control. Registered with GameConfigOverrides; no extra hook registry.
The legacy detour's unused pointer return is not carried into the target's
typed void LoadConfigs contract. The source's broad TestPatches bucket also
contains cursor and queue behavior; those are distinct consumers, not gaps in
this capability.

Migration implementation qualified in controlled Windows host. In-game endpoint
use, malformed/unavailable live bindings, native reload reentrancy and macOS
runtime remain unverified. Ordinary startup smoke cannot qualify server routing.
No credentials or production endpoint values are logged by the consumer.
