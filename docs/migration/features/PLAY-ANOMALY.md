# PLAY-ANOMALY — Galactic Anomaly countdown

Source reconciliation, 2026-10-31. Frozen play-dev remains `1baa55fedf1758b19f843cc59707b8901a0b4998`;
historical play remains `cad98ef7990dd66167eb64ea5dda525c1fe2c296`. Target is the existing
`play-dev-ripper` checkout, base `5c279fab7b0f578debf2c3614de0ce76dd752aa9`.

Read the complete frozen `galactic_anomaly_timer.cc`, its settings/registration consumers,
the current typed timer, UI/HUD bindings and upstream correction #350 (`4f403a90`).
Frozen source explicitly discovers navigation and the alliance drawer independently because
neither must descend from the top interaction canvas. The migration lost that behavior.

| Obligation | Preserved behavior and typed mapping |
| --- | --- |
| A1 | Restart gate `patches.galacticanomalytimerhooks`, live `graphics.galactic_anomaly_timer`, default false. Existing descriptor/frame owner and Settings registration remain. |
| A2 | Only system navigation and the native `CanShowGalacticAnomaly` gate admit a countdown. Leaving the section or disabling releases the panel/navigation owner. |
| A3 | Resolve navigation and drawer through their separate registered, visible canvas contexts. `ScreenManager::RegisteredController` reads the existing singleton, roots the registry during traversal, rejects unavailable counts and uses the existing non-generic `GetComponentInChildren(Type,bool)` binding. No global scene scan or new detour. |
| A4 | Retain a live active navigation owner; retry missing/inactive owners at most once per second. Missing owners remain retryable and do not create a native singleton. |
| A5 | Read the viewed system's existing anomaly and remaining native timer; require an active anomaly and positive ticks. No new local countdown clock or inferred fleet system. |
| A6 | Keep the 198x38 pink frame/dark fill, native system font, optional rounded sprite and centered text below Help All/Engage/Claim. No native button is moved. |
| A7 | Preserve 250ms polling, ceiling seconds, hour/minute formatting and existing displayed-second write cache. Expiry/missing clock hides the panel. |
| A8 | Missing panel resources produce the existing warning/backoff. Unity liveness and rooted owner cleanup remain separate from native clock availability. |

The owner regression cases execute the production binding against a controlled registry:
an unrelated overlay, a hidden matching canvas, separate navigation/drawer canvases,
the exact Type/bool overload, failed count, absent singleton and temporary root release.
They do not render TMP or simulate a real Mac scene. Native qualification must check an
active countdown, expiry, system changes, drawer collapse, return/reload and disable.
Build and runtime outcomes belong to the exact candidate receipt, not this contract.

No new feature, key, default, store format, hook or global discovery service is introduced.
