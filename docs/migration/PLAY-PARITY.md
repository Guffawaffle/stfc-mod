# Downstream play preservation target

## Active functional reconciliation — 2026-10-25

`play` at `cad98ef7990dd66167eb64ea5dda525c1fe2c296` remains the behavioral
oracle. The current isolated candidate began this pass at
`5c921b65e2b35d0d1ba008f7b6dcbb25f93e3430`. Earlier batch inventories below
are historical discovery records, not completion claims. The task roadmap's
former `inventory-complete` label has been corrected to reconciliation in progress.

The independent source pass followed producers, callbacks, consumers, cleanup
and persistence. It found additional missing native Settings workflows (including
FC confirmation recovery), Galaxy functionality, preset ordering, fleet/incoming
alerts and separate debug diagnostic surfaces. Registered keys and retained pure
models do not restore those workflows.

Current source changes restore conservative queue observation and engage-result
handling; substantive chat-title binding; weak fleet-label custody and reentry
checks; Haven history swap roots; the Enter-assignment installation gate; the
ordinary macOS logging fallback; and the legacy cross-section focus-search alias.
Notification restoration reconnects all seven fleet alerts, own-ship incoming
player alerts and macOS desktop delivery. Source review exposed and corrected
independent depletion availability and fleet-only macOS authorization dependencies.
These changes have **not** been deployed or player-qualified in this pass.

Local Windows `mods test-mod test-il2cpp` passed at the safety source point
(183 build/run steps), the initial notification source point (183), and the
notification policy fixture source point (189). These are build graph counts,
not independent feature counts. Later edits require their own checks. Logs are
in `D:/dev/stfc-workspace/.ax/play-functional-parity-20261025/`.

The subsequent Profiles-enabled Windows DLL and assertion-enabled fixtures passed
`install test-mod test-il2cpp` (201/201 build/run steps). The PE checker verified
18 proxy exports, static CRT and Profiles-host presence. Candidate DLL SHA-256:
`9c3b9c81ebc3eadcdf8cd86fde9fdce48fbaa74fdd937cdad6eb3e1d0238c687`.
`restoration-local-receipt.json` records the source-file hashes and exact artifact.
The machine-readable [workflow inventory](play-functional-parity.json) is the
current reconciliation ledger; its 95 entries remain an in-progress inventory,
not an assertion of comprehensive discovery or runtime equivalence.

CI, PR conflicts and integration infrastructure are deliberately parked. No
workflow changes, merging or rebasing are part of this phase. PR #374 is only a
review checkpoint. Eventual wholesale replacement of `play` does not authorize
promotion before this functional work is complete.

### Newly exposed contracts that must remain visible

- Unreadable queue counts, engagement latches, recall/removal state or address
  components must not become permission to process, resume or preserve a queue.
- Old fleet-label generations must not affect replacement pooled widgets.
- Fleet alerts require startup/re-enable seeding, journey/OPC continuity and
  independent polling/depletion availability. Incoming dedup suppresses side
  effects, never the native banner's independent visibility decision.
- Native Settings requires full navigation, exact editing, reset/save recovery,
  shortcut capture/undo, media pickers and preference invalidation, not a starter
  page. FC confirmation reads must not construct an account singleton to render.
- Preset save must always use canonical source indices, including repaint,
  disable, release and reentrant old/new projection transitions. The parked P15
  prototype has a confirmed transition-save hole and remains inactive.
- Discovery tow has demonstrably drifted PLAY bindings: retained client272/273
  have seven-argument PlanCourse and yielding iterators. Restoring the legacy
  tight MoveNext loop would not establish native coroutine scheduling.
- Debug overlay, diagnostic source bus, science-error JSONL recorder, Fleet
  Watch probe and keyboard-vars diagnostics are separate capabilities.
- Legacy sent-battle IDs record queue admission, not delivery. The previously
  accepted per-target ACK semantics must not infer delivery from that old file.

Implementation completion, local automated qualification and player-confirmed
parity remain separate. Overall functional parity is **not yet established**.

---

2026-10-25. This is a source inventory, not a declaration of completed gameplay
parity. The primary target is the existing downstream **play** experience; later
play-dev, AAI and STFCKitUI additions follow it. An incremental Settings port is
reasonable, but a smaller starter page is not the completed Settings target.

## Exact inputs and scope

| Input | Exact revision | Role |
| --- | --- | --- |
| Primary downstream play | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Source-verified behavior oracle; local commit is PR #372, Swap Ship pin double-click. |
| Later downstream play-dev | `e4be4bda7ea472af4369cd63e458d9e948a6c427` | Secondary additions; not the minimum play acceptance target. |
| Earlier broad preservation input | `45d1deaffddc3706e5c8232cf746ca46ef67be91` | Includes play-dev work; retain qualified improvements, but do not mistake every extra for missing play behavior. |
| Migration inventory checkpoint | `dce32e0f5864608ab148943e25f658ce5b58008f` | Current committed source when this inventory began; chat corrections and subsequent source changes need their own checks. |

All comparisons used local Git objects; no branch movement, runtime inspection,
account/store access or source implementation was performed for this document.
The parked development-install scripts/build wiring were already dirty and were
left untouched. A concurrently modified binding fixture is not silently counted
as a qualified part of the committed checkpoint.

Earlier documents retain their historical gates against `45d1deaf`. They remain
evidence for those exact slices, not proof of complete play parity. Stronger
qualified transaction, identity, source-custody, persistence, session-isolation
and observation guards must not be reverted merely to reproduce older bugs.

## Full Mod Settings already belongs to play

The pinned play source already includes the full native Settings experience.
`mods/src/settings/mod_pages.cc::RegisterModPages` calls the upstream catalog,
shortcut pages and audio-alert pages. Native `page_navigation.cc::InstallPages`
also registers settings search and maintenance/default/help pages.
`patches.nativesettingshooks` defaults **true** in play. Its absence from the
migration is a missing product surface, not a newly optional play-dev feature.

Compared with play, the earlier `45d1deaf` input changes only seven Settings
files: image-file validation/picker behavior, text-entry/value-row details and
the later AAI visibility control. The established page catalog, controls,
shortcut editor and audio pages are overwhelmingly already play code. This
distinction prevents the full Settings target from being incorrectly parked
alongside AAI/Kit expansion.

### Page and feature preservation matrix

Every Settings page below is currently **absent as a native UI**. The migration
column describes source counterparts or gaps in the underlying feature owners;
an existing config member or similarly named feature is not full parity proof.

| Play page/surface | Play capabilities to preserve | Migration owner status / material dependencies |
| --- | --- | --- |
| Controls | Mod/native/extended shortcut modes, queue/protections, visible-target preference, selection interval. | Hotkey/queue owners and registered gameplay saving exist. Full control definitions, requested-versus-active restart reporting and native controls are not ported. |
| Camera | System/Haven/interior zoom, keyboard zoom speed, pan glide, default/preset zoom, last-preset behavior. | Source counterparts in `camera.cc` and qualified Haven cohort. Current qualified retention/restoration guards remain. Numerical/exact editors and the complete page are absent. |
| Interface & Scale | Interface/viewer/ship/backdrop scale, scale step, Windows cursor/resize/fullscreen, Escape/double-tap, movement, first-popup preference, focus-search. | Source counterparts exist across scale/window/hotkeys/shop; original/close lifecycle and exact per-control availability still need acceptance checks. |
| Map & Travel / Ship Filters | Anomaly timer, instant-warp choice, three editable ship-name filters including wildcard semantics. | Anomaly/instant-warp/filter config and guarded planned-system warp have counterparts. Full filter editor and live UI adapter are absent; not every navigation behavior is thereby qualified. |
| Previews & Cargo | Preview Locate/Recall rules, cargo expansion/type switches, instant counter, formatting/precision, base-card retention, OPC highlight/ETA, ship-tech icons/backgrounds. | Cargo and preview counterparts exist. `keep_base_card_open`, OPC/ETA and ship-tech indicator controls/owners have no equivalent current config/feature integration. Forbidden-tech equipped-ship naming is a different capability. |
| Claims & Confirmations | Bulk-claim drawer/default toggle, faction multi-select, reveal/FT/Excelsior choices, Exchange All visibility, chest/donation limits. | Native counterparts exist in missions/shop/confirmations. Full faction/limit controls and their exact defaults/refresh semantics are absent; similar hooks alone do not establish limit/owned-value or popup lifecycle parity. |
| Officers & Ships | Double-click assignment, Below Deck sort, ship selection/dock synchronization, out-of-dock power, Haven help order, officer-preset reordering. | Sort/assignment/dock/power/Haven counterparts exist. Native officer reordering remains inactive: only its pure policy is qualified. Stable-ID pins and their qualified receipt semantics remain intact. |
| Chat | Titles and galaxy/Veil chat choices; preserve play's actual avatar visibility behavior even where not represented by a separate page control. | Chat/title counterparts exist; avatar behavior was visibly reported wrong during the trial and is being corrected separately. No success is asserted here for that unqualified delta. |
| Notifications | Hidden/desktop-notifying banner type editors, preserving parse/format and platform restrictions. | Toast filtering/Windows notification counterparts exist; native text entry and the page are absent. |
| HUD Buttons | Auto/Always/Never for Q Trials, Field Training, Outposts and Missions. | Qualified baseline restoration/Refresh owner exists. Its native choice controls, reset and status presentation are absent. |
| Fleet Labels | Player/non-player Native/Expanded/Compact/Threshold modes and exact thresholds; ship shortcut badges. | Fleet-label source counterpart exists. Play's configurable ship shortcut badge owner is not established merely by the foundation's native keybind-hint relabeling. |
| Loading Screens | Background/transition/black/tip toggles, logo scale, image path, file picker, exact-entry fallback and restart semantics. | Runtime custom-image/fallback source exists in `loading_screens.cc`. UI path/file selection and page are absent. Preserve later qualified validation improvements without making those additions the play baseline. |
| Advanced & Configuration | Scoped config path, truthful save/help information, TOML-only advanced settings guidance; do not display credentials. | File/config/save ownership exists. Native help/info presentation is absent. |
| Galaxy Labels | Extended selection, station housing, multi-overlay selection, all four overlay modes, major/minor detail choices and threshold editors. | These play controls and associated galaxy owner/config fields are not present in the current migration. System fleet-label detail is not an equivalent. |
| Shortcuts | Grouped discoverable actions, binding list, add/replace/remove, recording popup, overlaps, cancellation, release gating, undo and restore-default workflow. | New shortcut parser/dispatcher has source counterparts and aliases, but there is no interactive editor. Current public API exposes whole-set `Set` and descriptions, not the old per-action edit/default/undo contract. Adapt the existing owner; do not add a second parser or dispatcher. |
| Audio Alerts | Coalescing choices; built-in/custom audio, preview and asynchronous picker; incoming-player desktop option; victory/defeat/armada cues and seven fleet-event pages. | Current audio owner supplies built-in generated tones and five toast cues. Custom-file cue representation, coalescing, incoming-player notification owner and seven fleet-event routes are missing. Existing old custom paths remaining in TOML is not semantic support for those paths. |
| Search / Defaults & Help | Search results returning to the same context; per-page reset, per-setting details/defaults/retry, failed-save notice. | No native implementation. These are existing play behaviors, not optional new framework work. |

Play's audio pages include arrival in system/destination, mining start, node
depletion, dock, repair completion and miner-over-protected-cargo cues. Its
shortcut catalog includes Camera, Chat, Client, Fleet Controls, Game Screens,
Interface Controls, Map & Travel, Previews & Cargo, Uncategorized and Diagnostics.
Generated pages and feature availability make a static total row/page count
misleading; the registered definitions are the preservation authority.

### Shared Settings contracts

The full port must preserve these existing capabilities rather than only its
visible labels:

- Boolean, choice/multi-choice, slider, exact-number/text entry and action rows;
  bounds/formatting must not round the stored value or convert unavailable into
  false/zero. Slider limits do not erase meaningful custom values.
- Page navigation, conditional sections, scroll/bind/release ownership and
  native Back/Escape behavior. A pooled row cannot retain a previous page's
  pending action or callback.
- Setting metadata drives application timing, defaults, search, reset and
  persistence. Restart controls report requested and active behavior honestly.
- The existing preserving TOML worker remains the writer. Register keys once;
  preserve startup comparison bases, external-edit conflicts, retries, per-key
  coalescing, pending/session intents and unrelated comments/unknown values.
- Page reset prepares and validates its whole action set before applying it.
  Shortcut drafts/capture own cancellation, undo and release boundaries.
- Settings session invalidation matters: play hooks preference registration,
  session start and cloud-load boundaries to invalidate row/callback state
  before native originals. Do not silently drop those guarantees because the
  new writer is independent of game preferences; replace them only with an
  explicitly qualified lifecycle equivalent.
- Preserve account/profile routing and native preference/cloud ownership. A
  Settings rendering port does not authorize moving files, changing schemas,
  rewriting game preferences or exposing sync credentials.

## Implementation dependencies, without reducing the target

1. **Existing pure models.** Preserve the play `settings/{boolean_settings,
   boolean_view,value_settings,value_view,choice_setting,slider_setting,
   action_setting,page_catalog,persisted_setting,page_sections,maintenance}`
   contracts and fixtures. Current `setting_metadata.h` and the qualified
   RuntimeConfigWriter/TomlEditor are reusable; avoid duplicate models/writers.
2. **Native adapter foundation.** Port the play category/value/action/text/search
   adapters into current typed bindings and GC handles. Use M2 named hook
   transactions and existing frame ownership. Installing metadata may resolve
   APIs but must not invoke live engine state during startup, as the
   [frame-counter correction](STARTUP-FRAME-PREFLIGHT.md) demonstrated.
3. **Bound callbacks.** Foundation `il2cpp::NewDelegate` currently accepts static
   targets; play Settings uses owned instance schemas with replaced direct,
   virtual and runtime-invoker paths. Earn a bounded compatible bridge with
   controlled callback tests. Never disguise an instance schema as static or
   create another invocation engine.
4. **Complete play catalog.** Adapt play's real `RegisterUpstreamSettings`,
   shortcut and audio registrations to nested current Config/feature owners.
   Access current `Config::Get()` on each operation; do not retain field
   references across reload. Each absent owner above needs a tracked port or an
   explicit held limitation. A page may appear incrementally without claiming
   completion or silently treating absent owners as restored functionality.
5. **Current libraries and toolkit.** Reuse native settings assets, uGUI/TMP,
   root/lifecycle helpers and the existing button dispatcher where actually
   needed. STFCKitUI remains a later independent component integration: its
   present native host is inside AAI and it does not supply the complete form
   control family. Full Settings does not require restoring AAI or inventing a
   new rendering framework first.

Important play sources are
[catalog registration](https://github.com/Guffawaffle/stfc-mod/blob/cad98ef7990dd66167eb64ea5dda525c1fe2c296/mods/src/settings/upstream_settings.cc),
[native navigation](https://github.com/Guffawaffle/stfc-mod/blob/cad98ef7990dd66167eb64ea5dda525c1fe2c296/mods/src/settings/native/page_navigation.cc),
[native value rows](https://github.com/Guffawaffle/stfc-mod/blob/cad98ef7990dd66167eb64ea5dda525c1fe2c296/mods/src/settings/native/value_widgets.cc),
[shortcut editor](https://github.com/Guffawaffle/stfc-mod/blob/cad98ef7990dd66167eb64ea5dda525c1fe2c296/mods/src/settings/shortcut_settings.cc)
and [audio pages](https://github.com/Guffawaffle/stfc-mod/blob/cad98ef7990dd66167eb64ea5dda525c1fe2c296/mods/src/settings/audio_alerts.cc).
The older `RegisterUpstreamFeaturePages` helper is defined but has no call site
in pinned play; it is not a substitute for the actual registered full catalog.

## Other play gaps and established improvements

Native officer-preset reordering remains a specific safety boundary. SlotId zero
is valid; empty rows stay fixed; Shift moves up and Ctrl/Command moves down via
the rename control. Native save updates canonical data and a separate renderer
index map. A data-pointer swap alone was disproved by retained native evidence;
the inactive pure policy must not be presented as restored native behavior.
See [the preserved safety finding](M8-PRESENTATION-PRESERVATION.md#officer-presets-policy-qualified-separately-from-native-activation).

Navigation/orbit and galaxy additions, base-card retention, OPC/ETA, ship-tech
and badge consumers, custom fleet/incoming notifications and full Settings are
play preservation work. Their presence in `45d1deaf` did not make them
play-dev-only. Conversely, later AAI visibility, claim science/recovery probes,
sync diagnostics and development experiments must not be activated wholesale to
satisfy the narrower play target.

Retain the qualified Profiles/bootstrap, preserving settings/state, stable-ID
pin receipts, sync fencing/acknowledgements, HUD/toast, Haven and guarded planned
warp improvements. Their existing source/binary receipts remain attached to
their own exact candidates. The corrected ordinary startup completed all 48
groups and FrameTick; that establishes startup, not complete gameplay behavior
or full Mod Settings restoration.

## Settings restoration checkpoint

The existing play settings models are now restored under `src/mod/settings`:
value/Boolean state and views, choices, sliders, actions, page catalog/sections,
input release, native-view state, maintenance, search and text-entry contracts.
The existing process-owned callback schema is restored unchanged. This is a
port of the established Settings implementation, not a second toolkit or a new
starter-page model.

Independent source readback confirmed the thirteen model headers and seven
original fixtures match pinned play after line-ending normalization. The only
text-entry change is an explicit fixture-host declaration switch, allowing the
existing maintenance test to supply its editor on unsupported test hosts without
changing the production fallback. The current metadata header is compatible
and remains unchanged. No native page or feature is activated by these headers.

Windows x64 Zig 0.16 `test-settings-models` passed **17/17 build steps**, executing
eight original fixture programs with assertions enabled. They exercise value
readback/failures, refresh/reentry/generation/thread ownership, exact choice and
slider behavior, search, catalog lifetime, reset/conflict/retry behavior and
owned callback direct/virtual/runtime-invoker dispatch. All original assertions
remain. Callback-schema tests do not qualify actual managed closed-delegate
construction or Unity page binding.

Evidence: `D:/dev/stfc-workspace/.ax/ripper-play-settings-models-20261025/build.log`.
This is a Windows component qualification, not a complete product, macOS,
native Settings UI or new deployed-build claim. The running build remains the
separately qualified [chat retention candidate](CHAT-AVATAR-RETENTION.md).

The next boundary is adapting the existing native category/value/action/search
adapters and their instance-delegate construction onto the current checked
bindings and shared frame lifecycle. Full catalog, shortcut and audio feature
owners still need their explicit integrations. A passing model suite does not
make any missing control functional or restore the Settings entry point.

## Acceptance and remaining qualification

- Restore and independently exercise the preserved pure settings, metadata,
  page catalog/search/defaults, shortcut capture/draft/undo and audio/file-task
  fixtures. Adapt fixtures to the actual new owners rather than testing copied
  presentation labels or assumptions about old flat Config fields.
- Exercise actual native adapter callback/ownership seams with controlled
  objects: foreign rows stay native, stale/released rows cannot publish, page
  changes cancel old interactions, unavailable APIs do not become values,
  startup preflight invokes no engine, and original calls remain correct.
- Run candidate-bound Windows builds and portable tests; retain macOS build,
  delegate ABI, picker, signing and live native qualification as distinct claims.
- In game, reach Mod Settings through the native settings flow and shortcut;
  check every offered control family, page navigation/search/Back, live and
  restart timing, exact text/numeric entry, real save status/conflicts/retry,
  shortcut recording/undo and custom audio selection/preview. Missing feature
  owners remain explicit until their own actual behavior is qualified.
- User-visible chat-avatar feedback is direct usability evidence. It deserves
  its own correction/trial and cannot be dismissed by passing offline tests or
  an all-hooks startup count.

No new test/build/runtime qualification is claimed by this inventory. Its next
step is a dependency-aware full-play implementation sequence, with Settings and
chat visible regressions prioritized by the coordinator and later extras kept
separate.
