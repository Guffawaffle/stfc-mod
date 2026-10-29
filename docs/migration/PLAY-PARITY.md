# Downstream play preservation target

## Shop quantity and popup continuation — 2026-10-29

[Cohort20261029-01](FEATURE-COHORT-20261029-01.md) source-reassesses donation,
chest limits, shared popup/reveal handling and artifact Exchange All. The actual
consumer fixture reproduced and then qualified a stale chest-context write;
the existing popup atomic state and accurate next-view Settings timing were
restored. Keys, defaults, stores and individual game actions remain unchanged.
The assertion-enabled Windows suite passed419/419 steps. Native build/trial and
Mac evidence are kept separate in the cohort; scoped Mac donation behavior
continues to differ intentionally from the old global Slider hook. No overall
functional-parity claim is made.

## Current continuation — 2026-10-28

[Shop/claims/HUD/confirmations cohort03](FEATURE-COHORT-20261028-03.md) reads the
complete frozen implementations and preserves existing consumers. Bounded
gift-drawer and daily-toggle changes reject stale native show/update episodes;
HUD, cooldown-offer and confirmation behavior is exercised without a rewrite.
The Windows controlled-runtime suite passed 418/418 build/run/install steps.
Native build/trial evidence is recorded separately in that cohort. No reward or
purchase was performed by the fixtures; macOS remains separately qualified.

The separately catalogued [Go to Haven shortcut](features/PLAY-HAVEN-ENTRY.md)
was accepted by Guff from Station and another Haven on the already installed
113f13ce build. No Haven code or deployment changed during that retest. The
earlier intermittent missing-UI report remains unreproduced, with no asserted
cause. New Escape/card dismissal and visitor Back behavior remain parked.

[Haven native-event correction](HAVEN-VISUAL-EVENTS-20261028.md), signed source
`113f13ce`, replaces normal timed visual discovery after the player observed road
depth loss during Haven switching. Windows controlled checks passed 417/417
steps and its native build passed 41/41; the main ordinary game launched with all
73 groups. Guff accepted the focused Windows home/visitor/home road trial;
settled logs showed no periodic rediscovery. Failure recovery retries remain
explicit, and later-streamer/macOS coverage stays separate.

`play-dev-ripper` now includes the four CI/test-only commits through
`d55827023f98243eda4a5fb7af54a9c5d5bceebc`, matching the passing published
checkpoint. Both Mac architectures, Windows, Linux, universal packaging and the
separate Mac signing/notarization run succeeded. This did not change gameplay,
deploy a binary or qualify optional AAI/Profiles builds and Mac runtime behavior.

The [next eight-capability sprint](FEATURE-COHORT-20261028-01.md) covers ship
technology artwork, fleet badges/labels/OPC and chat. It records current source
differences, acceptance criteria, inventory drift and remaining completion lanes.
Most catalog entries already have an implementation; their older classifications
do not measure remaining port effort. The JSON inventory remains authoritative,
and overall functional parity remains unclaimed.

## Travel and cargo workflow cohort — 2026-10-27

[Cohort 20261027-02](FEATURE-COHORT-20261027-02.md) reassesses eight travel/cargo
workflows against frozen play-dev using three bounded workers and shared
integration. The current Windows suite passed 347/347 build/run/install steps;
native startup and basic Windows trial passed; macOS and extended edge cases
remain separate. Full source comparison distinguishes
existing retained-course behavior from actual cargo polling/restoration
omissions and inherited delayed-preview defects. LexRunner provided dependency
ordering; worker execution remains direct collaboration because governed native
Windows containment is unavailable. The cohort record tracks final artifacts
and qualification without an overall-parity claim.

## Fleet and queue workflow cohort — 2026-10-27

[Cohort 20261027-01](FEATURE-COHORT-20261027-01.md) reassesses the existing fleet
actions, preview/Escape and two queue recovery workflows against frozen play-dev.
It preserves working policies and repairs observed integration omissions, with
actual adapter tests rather than policy-only claims. The historical repair
interlock was discovered separately and remains a named follow-up, not an
assumed fix in this batch. Native race/input and macOS qualification remain
explicit; this is not whole-product parity.

Implementation `94f4b3e07417430b0bf3f4683e692ec15239e366` passed 335 Windows
build/run/install steps and 14 explicit fixture reruns. The exact native build
launched in the main ordinary installation with all 71 feature groups and frame
delivery ready. Guff reported **“no problems”** for normal queue, scan/engage and
Escape use. Five tracked settings/state files remained byte-identical. Natural
recovery races, extended interaction coverage and macOS remain unqualified.
One contained, rate-limited `GetComponent` exception is recorded without causal
attribution; this is not an error-free runtime claim. The cohort record binds
these observations to the source, installed DLL hash and local receipts.

## Native AAI integration — 2026-10-27

The [AAI host contract](features/PLAY-AAI.md) ports the frozen play-dev dependency
at `4bece2e` without changing its analytics, decoder, worker, native Overview,
Crew & Effects, Tactical Console or optional Kit workshop. The host uses the
existing transactional installation, frame owner and Settings persistence.
All 42 pinned AAI fixtures and 96 retained-report result comparisons pass;
native gameplay and macOS qualification remain distinct from those results.
This is a selected play-dev capability batch, not an overall parity claim.

On 2026-10-27 Guff reported AAI working during normal use. Record this as basic
Windows player confirmation; individual interaction paths and macOS remain
separately qualified in the host contract. No additional cycle was needed.

## Officer and pin workflow cohort — 2026-10-26

[Cohort 20261026-02](FEATURE-COHORT-20261026-02.md) performs the next four complete
frozen-source scans. Below Deck sort construction regains explicit source custody
and field qualification; pin gestures invalidate every native bind episode, even
for unchanged ship/context IDs. Existing durable pin ordering and checked writes
are retained. The later [preset integration and initialization correction](features/PLAY-OFFICER-PRESETS.md#initialization-correction-and-basic-windows-acceptance--2026-10-27)
has basic Windows player acceptance for reorder, correct-row rename and retained
local order. macOS, native crew replacement and extended failure paths remain
unqualified; a config entry or passing pure fixture is not runtime equivalence.

## Active Ripper migration branch — 2026-10-26

`play-dev-ripper` is the active continuation of the long Ripper refactor, including
the source-preserving pilots and the four-feature Settings cohort. It uses the
existing `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream` checkout.
This is a development integration checkpoint: **launch accepted, partial Windows
player testing, Settings UX work deferred**. It is not a release or a whole-product
parity claim. `play` and `play-dev` remain unchanged.

The former `feature/migrate-play-feature-pilots-20261026` branch is renamed to
this active branch without changing history. The obsolete local
`integration/ripper-foundation-m1b-20261025` pointer at
`9a2018affa88915cfbf45803945637a64b95068f` is removed only after verifying it is an
ancestor of the active branch and not checked out elsewhere. Its history remains
in the active branch and study archive. No remote branch is deleted.

Preserve `study/ripper-typed-bindings-20261026` at
`076c85a84ee5d126b02582f55936ccfc4a066a29` as the long-term technical reference,
and `frozen/play-dev-20261026` at `1baa55fedf1758b19f843cc59707b8901a0b4998`
as the immutable feature source. Other study branches are not part of this cleanup.
Earlier branch names in receipts and source observations are historical facts.
The [cohort record](FEATURE-COHORT-20261026-01.md) retains exact artifacts,
qualification limits, the Search-row correction and deferred Settings work.

## Frozen-source feature cohorts — 2026-10-26

The accepted source-preserving procedure now continues with
[four Settings workflows](FEATURE-COHORT-20261026-01.md). The generated
[feature catalog](FEATURE-CATALOG.md) adds pinned upstream dev, play, play-dev and
refactor observations to the existing JSON inventory. Unexamined entries remain
explicitly unassessed against those refs; this is not a whole-product parity claim.
The two initial pilots received basic Windows player confirmation. Detailed edge
cases and macOS qualification remain separate, as recorded in their contracts.

## Bounded frozen-play-dev pilots — 2026-10-26

The [migration skill pilots](MIGRATE-PLAY-FEATURE-PILOTS.md) reassess only
PLAY-DOCK-ASSIGN and SET-IMAGE-PICKER against frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Their contracts and additive inventory
assessments preserve the historical PLAY baseline below. Assignment input
omissions are repaired, but snapshot custody and native interactions remain
unqualified. Image selection is implemented and tested offline, unverified in
game. This is an isolated skill/pilot branch, not whole-product restoration,
deployment, promotion, or a new overall-parity claim.

## Current alignment sprint checkpoint

The [remaining alignment sprint](ALIGNMENT-SPRINT-20261025.md) restores native
Settings page/control adapters, exact numeric editing, shortcut recording and
undo, contextual search/default tools, runtime keyboard diagnostics and scheduled
Discovery towing. The frozen Windows native source and targeted fixtures pass
239/239 build/run steps. This is local qualification, not player-verified parity.
The machine-readable inventory now distinguishes these implementations from the
still-missing native preset integration and debug surfaces. String/list editors,
custom media workflows and macOS/native interaction qualification remain open.

Earlier source/build descriptions below are historical checkpoints. In particular,
the two-row Settings-only description and older queued notification statuses no
longer describe the current source. No release promotion or full-parity claim follows.

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
current reconciliation ledger; its 101 entries remain an in-progress inventory,
not an assertion of comprehensive discovery or runtime equivalence.

Native Settings core restoration now lives in `features/settings.cc` with domain
bindings in `game/settings.h`. The FC/FT core is **now registered in the candidate feature
table**, guarded by the preserved default-on `nativesettingshooks` key. It has
not been deployed or validated in-game. Checked closed delegates validate the exact managed signature,
receiver and both native execution slots; controlled-runtime tests exercise void,
Boolean and enum callbacks. FC preference reads observe existing initialized
singletons and never create account storage merely to render. FT uses the shared
registered-key preserving writer and save-status model.

Native insertion recovery begins before AddToggle: append-then-throw and unknown
count/item/ownership scans retain weak operation custody. Only exact supplied
getter/setter identities authorize cleanup; successful removal calls alone never
prove absence. Provisional rows are recognized by their owned getter before
query installation and remain hidden/uneditable. Hidden and identity-lost views
schedule rebinding, including restoration-triggered context replacement. Native
entry staging now uses CheckedEntry (including macOS fit protection).

`settings-core-build-7.log` passed Windows `mods test-mod test-il2cpp` with
193/193 build/run steps. Row insertion/quarantine tests exercise the production
recovery policies; they do **not** exercise actual game widget rendering. The final registered-core Profiles-enabled Windows DLL and tests passed
`settings-core-registered-build.log` (205/205 steps), including the subsequent
recovery and checked-entry corrections. Full page
navigation/search, exact editing, shortcut capture and media pickers remain
missing. Two confirmation rows do not constitute full Mod Settings.

Normal state-file shutdown now participates in RuntimeConfig's existing sole
quit owner. It closes state admission, skips debounce and waits through frame
polling for active/pending edits to settle, with no main-thread join or file I/O.
A resumed native veto restores both writers together. Failed commits stay failed;
settlement is not success. Disabled/missing RuntimeConfig delivery remains an
explicit qualification gap; forced termination has no normal-drain guarantee.
Worker and actual quit-adapter fixtures passed at `settings-state-build-2.log`
(193/193 steps), and the final registered-core candidate passed 205/205 steps.
The native shutdown interaction still needs an exact-artifact in-game trial.

Retained client273 inspection binds the Settings work to dump SHA-256
`3c019dd684ee5e1062e758831c00be719fb53c94b4859c95ba2cc46189dc8621`
and GameAssembly SHA-256
`a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
`ContainerOptionContext`, not `OptionContext`, owns the read-only Children facade.
Native AddToggle stores supplied delegates in SafeFunc/SafeAction wrappers, then
AddChild appends before calling the child's virtual parent setter. Insertion may
therefore mutate before reporting failure. Cleanup must prove the exact getter
and setter identities, not infer ownership from a child index or matching title.

An apparent extra resolution-scaling gap was rejected: `adjust_scale_res` has no
active PLAY parser/writer and stays false in its zero-initialized Config. F11
borderless activation likewise depends on a PLAY hook that is never installed.
The migration's added frame activation is withheld; the implementation is kept
for separately authorized work. Neither dormant branch is required active parity.

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
- Native Settings still requires full navigation, exact editing, reset/save recovery,
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

## Native Settings hook evidence — retained Windows client273

The `.pdata` extents below come from the exact retained GameAssembly named above.
They establish substantive Windows entries, not installed-client runtime behavior
or macOS qualification. All seven now stage through the checked-entry adapter.

| Native entry | RVA | Unwind extent (bytes) |
| --- | --- | ---: |
| SettingsSectionDirector.AddGeneralSettings | `0x1019CB0` | 1533 |
| ToggleOptionWidget.SetWidgetData | `0x1035050` | 623 |
| ToggleOptionWidget.OnAboutToReleaseContext | `0x10352C0` | 267 |
| ToggleOptionWidget.OnToggleValueChanged | `0x1035450` | 474 |
| PersistentPrefsManager.RegisterEvents | `0xD64440` | 300 |
| PersistentPrefsManager.GameSessionStartedEventHandler | `0xD658C0` | 156 |
| PersistentPrefsManager.LoadPersistentPrefsFromCloud | `0xD65B90` | 367 |

### Next native confirmation qualification

Use the final exact artifact, not a prior startup smoke: open native Settings,
find the confirmation category, verify FC/FT labels and current values, change
and read back each intentionally, close/reopen, and verify FT TOML save status.
Repeat after pooled row reuse and preference/cloud replacement. Unknown account
storage must not appear OFF; provisional/unsupported rows must not accept edits.
Exercise same-value refresh, config reload, rapid close/reopen and disabled
ForbiddenTechConfirm independently. Observe normal quit with a pending state save,
then a genuine native veto, and confirm subsequent saving works. No such runtime
trial, macOS build or macOS native behavior is claimed by the local checks above.


## Galaxy restoration cohort

[Galaxy controls](M8-GALAXY-CONTROLS.md) restores the far-zoom selection, independent overlays,
major/minor visibility profiles and housing marker gameplay paths. The machine inventory retains
Partial dispositions because their native Mod Settings controls and exact-artifact game verification
remain outstanding. Tests/builds are local evidence, not player-verified parity.


## Queued pan momentum follow-up

Guff requested that pan momentum/glide be included in the remaining camera parity work.
The existing control is `graphics.system_pan_momentum_falloff`; higher values retain more movement
after release. No separate Galaxy drag-speed setting was found in the inspected PLAY configuration.

Static comparison found that PLAY's custom-keyboard path can move the camera twice per coasting
frame, while the migration applies one movement path. This may affect feel; it is not a measured
regression or authorization to blindly restore duplicate movement. Compare matched drags/releases
at equal zoom/falloff, hotkey modes, focus/modal/tracking states and representative frame rates
before deciding the implementation. Track under `PLAY-CAMERA-PAN` / P08. No runtime or settings
changes were made for this backlog addition. Guff confirmed Galaxy label visibility works as intended;
do not resume the earlier proposed label-control restoration on the basis of that withdrawn report.

## Current map and camera reconciliation

[Cohort 03](FEATURE-COHORT-20261027-03.md) scans the complete pinned source for
eight camera/Galaxy workflows and preserves their current typed consumers.
The contracts and machine inventory record the narrow availability, ownership,
cleanup-retry and entry-alias corrections. Existing Galaxy Settings controls are
present; M8's earlier missing-control descriptions above are historical.

The integrated Windows controlled-runtime suite passed 358/358 build/run/install
steps with Debug fixture assertions, including previous Settings, preset, queue,
travel/cargo and AAI-host regressions. Candidate `8c306f89` then passed 41/41
native build steps, all 71 installed groups and frame readiness, unchanged stores
after the normal main-game cycle, and Guff's basic map/camera trial ("all seem
good"). Extended native edges and macOS remain separate; exact source, artifact
and receipt references are in the cohort record.
Execution was direct collaboration/local tooling; Runner's native execution
layer is held while its helper is developed separately. No overall parity,
promotion, release or upstream-alignment claim follows from this batch.

## Fleet and navigation reconciliation

[Cohort 04](FEATURE-COHORT-20261027-04.md) preserves and qualifies fleet
selection/repeat-location, expired ability-button recovery, screen navigation
and Discovery towing. Complete frozen-source contracts distinguish existing
consumers from lifecycle/availability corrections. The combined Windows
controlled-runtime suite passed 382/382 build/run/install steps with assertions
enabled. Candidate `b36d2837` passed 41/41 native build steps and the normal
main-game cycle with all71 groups/frame ready and five stores unchanged. The
ordinary selection/screen trial was accepted ("looks good to me"). Live tow,
rare cooldown recovery and macOS remain separate. The contained GetComponent
diagnostic recurred during the trial and remains an explicit unresolved follow-up.
Exact evidence belongs in that cohort.
Runner's native execution stayed held; three workers and integration used direct
collaboration. No overall parity or release claim is made.

## Haven, artwork and scaling reconciliation

[Cohort 20261028-02](FEATURE-COHORT-20261028-02.md) extends the pinned-source
reassessment to nine existing Haven, loading-artwork and scaling capabilities.
Contracts record exact operations and source/default/asset identity corrections;
fixture, native, player and platform qualification remain distinct. See the
cohort for current results, explicit source defects/limits and recovery evidence.
The [warp consecutive-click repair](WARP-CHOICE-RECOVERY-20261028.md) is an
independently accepted runtime correction, not a change to manual-choice intent.
