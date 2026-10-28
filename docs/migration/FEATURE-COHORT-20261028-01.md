# Fleet presentation and chat — next migration sprint

Planned on 2026-10-28; implementation has not started. The existing
`play-dev-ripper` checkout was fast-forwarded from `4642fdda` to
`d55827023f98243eda4a5fb7af54a9c5d5bceebc`. No gameplay changes, deployment,
game cycle, push or change to `play`, `play-dev` or the frozen source occurred.

## Source and verified starting point

- Frozen feature source: `frozen/play-dev-20261026` at
  `1baa55fedf1758b19f843cc59707b8901a0b4998`.
- Historical PLAY reference: `cad98ef7990dd66167eb64ea5dda525c1fe2c296`.
- Upstream-dev inspection reference: `e884efa7c6ff66d0776b4cd4d6b76ce193296be5`;
  this is a pinned local observation, not a claim about the latest upstream.
- The four adopted commits change CI/test files only: `9e94ee20`, `07ef78ba`,
  `b9157682`, `d5582702`. Production hooks and behavior are unchanged.
- [Build 40742965210](https://github.com/Guffawaffle/stfc-mod/actions/runs/40742965210)
  passed Windows, Linux, both Mac architectures and universal packaging.
  [Signing 40743317365](https://github.com/Guffawaffle/stfc-mod/actions/runs/40743317365)
  completed signing/notarization successfully. Fresh GitHub read-back established
  these results; the saved local monitor still said approval pending.
- Ordinary CI does not enable private AAI or Profiles-host integration. These
  successes do not qualify those optional native hosts or Mac gameplay.
- The last accepted AAI-enabled Windows gameplay artifact remains source
  `4f9a6f3b` / SHA256
  `c1de2877a4604805e9f39974a124dc76c59c64f68c2cbaff20f5c166e6095ff0`.
  Its accepted trial is historical evidence, not a new runtime test of this head.

## Eight capabilities and dependency order

Use the existing [migration procedure](../../.agents/skills/migrate-play-feature/SKILL.md).
Read complete source owners, dependencies, configuration, callbacks and cleanup;
adapt only incompatible boundaries. These are existing implementations to
reconcile, not eight presumed missing ports.

| Group | Inventory IDs | Frozen source → target | Qualification and known issue |
| --- | --- | --- | --- |
| Ship art | PLAY-SHIP-TECH-ART | `parts/ship_tech_indicators.cc` → `features/ship_tech_indicators.cc`, `game/ship_tech.h` | Preserve exact-ship FT/CT art and recycled tile ownership. Reconcile donor lookup before native testing. |
| Fleet presentation | PLAY-FLEET-BADGES, PLAY-FLEET-LABELS, PLAY-FLEET-OPC | `parts/ship_shortcut_badges.cc`, fleet portions of `parts/zoom.cc`, `parts/opc_indicators.cc`, `patches/fleet_opc_sample.cc` → corresponding feature/game owners | Check live hints, own/other labels, detail thresholds, SAFE/OPC/ETA, selected cards, independent toggles, rebind and restoration. |
| Chat | PLAY-CHAT-CHANNELS, PLAY-CHAT-AVATARS, PLAY-CHAT-TITLES, PLAY-CHAT-NAVIGATION | `parts/chat.cc`, `chat_avatar_retention.cc`, `chat_titles.cc`, relevant `hotkeys.cc` routes → `features/chat.cc`, `game/chat.h`, existing dispatcher | Preserve scroll/reopen portraits and navigation/typing precedence. Explicitly disposition channel-redirection differences. |

Frozen paths in this table are relative to `mods/src`; target paths are relative
to `src/mod`. Existing Settings controls already expose these features. Do not
reimplement the Settings catalog or assume a missing control from an old report.

Two source differences deserve specific attention:

1. Frozen `TokenImageSource` first uses Ship Management's existing token widgets,
   weakly caches its donor, retries after five seconds, and attempts a global
   fallback once per selection context. Current `game/ship_tech.h::TokenSource`
   scans `Resources.FindObjectsOfTypeAll` whenever it lacks a usable donor.
   Preserve the original bounded acquisition policy with typed access rather
   than introducing another global scanner. Missing art must remain a valid state.
2. Frozen chat preview redirects broadly to Alliance when either disabled-channel
   preference is set; one callback caches those flags statically. The target
   reads live settings and redirects only a disabled selected channel. Record
   this as a deliberate behavior difference to evaluate, not silent equivalence.
   Do not restore the cached-settings defect merely to obtain matching code.

OPC's one-second shared-frame refresh replaces the original native lifecycle
callback. Its arithmetic already exists; verify view lifetime and cleanup rather
than creating another sampler. No new timing or relevance model is proposed.

## Acceptance and execution

1. Give all eight IDs source-bound contracts/assessments in the existing JSON
   inventory. Preserve older player acceptances and their exact candidates.
2. Fix demonstrated gaps only; test the actual affected adapters, unavailable
   bindings, stale/rebound views, off/on states, release and reentrant callbacks.
3. Run affected assertions-enabled Windows tests and the relevant native build,
   recording candidate, build options and executable identity. Earlier CI success
   does not validate subsequent implementation edits.
4. Build before stopping the game. If runtime work is requested, re-resolve the
   installation/profile/PID with the normal AXF flow and preserve existing stores.
   No purchase, tow or other resource-consuming action is necessary for this batch.
5. One focused trial: open/reopen Swap Ship and inspect different ship equipment;
   inspect own/other fleet labels and an available miner's OPC/ETA; scroll/reopen
   chat, change channels, exercise configured chat shortcuts and typing. Test
   settings only as needed, respecting the player's existing preferences.
6. Keep Mac compilation, hook-fit evidence, optional hosts and player testing
   distinct. Mcohall's eventual Mac trial is a separate acceptance lane.

Direct read-only collaboration informed this plan. No LexRunner Run, Attempt,
governed fanout or execution is claimed. Do not make Runner helper development,
new checkouts, CI expansion or a general UI framework a dependency.

## How far remains

At planning time the inventory contains 110 capabilities: 99 historically
implemented/unverified, five partial, three missing, two intentionally excluded
and one unresolved. Only 38 have the newer source-bound reassessment record;
40 link a contract. These are different measures, not a completion percentage.
There are 72 entries without that reassessment, many sharing already implemented
owners. They are not 72 missing features or 72 required new implementations.

The remaining work after this batch can be grouped as follows; discovery may
change the grouping, and no fixed number of sprints is promised:

- Haven, station/artwork, scale and remaining shop/HUD/claim workflows: compare
  existing owners to frozen source and close genuine omissions.
- Notification/audio and sync producers/consumers: existing implementations need
  complete source contracts and focused native lifecycle/delivery acceptance.
- Settings/config/state/routing: reconcile existing save/reload/quit evidence and
  test outstanding lifecycle cases. Named Profiles isolation needs its own exact
  runtime target; Profiles-enabled universal packaging is explicitly unqualified.
- Bounded remaining corrections: pin backdrop overflow, assignment custody/order
  differences, and Mac donation adaptation. Preserve accepted basic behavior.
- Developer tools: the debug bus and overlay are genuinely absent from the frozen
  source's target equivalent; bus precedes overlay. Their inclusion or explicit
  deferral must be visible before claiming complete developer-tool parity.
- Final Windows and Mac acceptance: exercise remaining workflows on the final
  artifact. Generic green Mac CI does not qualify AAI-enabled Mac builds or
  Profiles host integration. Promotion/replacement is a later explicit action.

Inventory corrections must retain provenance:

- `SET-NAV` and `SET-VALUES` still carry older pre-fix status descriptions despite
  later working Settings trials and demonstrated TOML persistence/restart. Do not
  treat them as entirely missing, or turn basic use into full lifecycle acceptance.
- `DIAG-SCIENCE-ERROR` exists in historical PLAY, but was deliberately removed
  from play-dev by `46307cc2` and is absent from the frozen baseline. Keep that
  hold; do not revive the probe to close an obsolete Missing count.
- The repair interlock is absent from frozen play-dev before this migration.
  Its current-client investigation remains separate, not a missing frozen port.
- `PLAY-FT-SHIP-NAME` has a doubtful historical source pointer; verify provenance
  rather than counting an unlocated source implementation as a lost feature.

Deferred feature ideas remain deferred: file-selector Clear, custom officer
sorts, Settings/editor redesigns, broader Kit work and persistence extraction.
AAI analytics changes, SQLite, new mechanics and launcher work are not migration
completion prerequisites.

Overall functional parity remains false. This plan advances source-preserving
closure without erasing earlier acceptance or representing code presence as
demonstrated equivalence.
