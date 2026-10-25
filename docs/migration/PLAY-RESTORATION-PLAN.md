# Full downstream play restoration plan

Primary target: **play `cad98ef7990dd66167eb64ea5dda525c1fe2c296`**.
Migration starting checkpoint: `606025e439c84e1512ce33227473246e69febeec`.
Source checkout: `D:/dev/stfc-workspace/worktrees/ripper-foundation/Downstream`.
The active canonical development checkout and released play remain unchanged.

## Execution decisions

Guff requested feature parity first, fast native trials, and renewed LexRunner use.
Restore actual play behavior before wiring the Settings UI. Preserve the already
restored Settings models and all qualified migration improvements. Play-dev, AAI,
STFCKitUI expansion and science probes follow primary play acceptance.

Use short **implement → compile/relevant checks → deploy → boot → player trial**
loops. A working boot is useful evidence and permits the next trial; it is not a
claim that every newly added interaction works. Do not wait for exhaustive
multi-platform/offline qualification before every Windows development build.

Crashes and visible regressions take priority over planned work. Retain the
current crash/log and fix the actual failing path; do not turn a runtime issue
into a tooling initiative. Keep one known working DLL and use the established
targeted deployment/start/stop flows. Store/profile protection, exact native
value/signature observations, unrelated files and existing configuration data
remain protected. Speed does not authorize replacing account identities or
discarding stores.

Cycles are authorized whenever a candidate is ready, unless Guff explicitly
places a hold. Recheck the physical installation, ordinary/profile selection,
PID and creation time under the existing operation exclusion. For the current
Windows trials the intended ordinary installation is
`C:/Games/Star Trek Fleet Command/default/game`. A branch name never selects
the running session. No push, release promotion or active-branch replacement is
implied.

## Ordered work and native checkpoints

| Stage | Tracked steps | Implementation / trial |
| --- | --- | --- |
| 0. Freeze the target and inventory | P00–P01 | Preserve the working migrated build; reconcile actual readers, feature owners and shortcut/UI workflows against pinned play. Already-present equivalents get trials, not duplicate ports. |
| 1. Small visible corrections | P02–P04 | Popup original/close lifecycle, donation semantics, ship shortcut badges and UI wheel ownership. Cycle after the first small runnable batch, then again when wheel ownership is ready. |
| 2. Preview behavior | P05–P06 | Enemy base-card retention including recall routing; occupied-mining-node ActionView cycle. Test fleet/target changes and own-miner fallback. |
| 3. Input/window compatibility | P07–P08 | Physical/layout input, layout-change notification, fallthrough, text/dead-key behavior; actual borderless/resize/scale behavior. Keep current parser and dispatcher. |
| 3a. Discovery towing | P28 | Restore the actual play Shift+ship-number tow action using qualified native deployment/coroutine operations; test available, denied and unavailable cases without copying the old busy loop. |
| 4. Ship information | P09–P10 | OPC/highlight/ETA and Swap Ship FT/CT art/backgrounds. Share native row lifecycles with pins and labels. Test recycled rows and missing values. |
| 5. Orbit | P11–P12 | Haven drag/reset, then system rig/free pan/zoom anchor plus the original environment shell. Cycle after Haven and system stages independently. |
| 6. Galaxy | P13–P14 | Label detail/thresholds, overlays/multi-select, extended selection, station-housing data/markers. Cycle for the first visible labels, then completed interactions. |
| 7. Officer presets | P15 | Existing pure policy + native order/save/renderer handoff. Preserve SlotId0 and empty rows, exact gestures and one native save. A disproven data-only swap cannot be justified by booting successfully. |
| 8. Audio foundation | P18 | Existing prepared built-in/custom-file cue, async playback, preview and coalescing. No disk reads on the playback path. |
| 9. Notifications | P16–P17 | One emission service, validated incoming-player attacks, then the seven owned-fleet event routes. Test startup arming, duplicates and focus rules. Depends on audio foundation. |
| 10. Existing-feature sweep | P19–P20 | Claims/HUD/cargo/confirmations, loading/anomaly/labels/chat. Fix concrete failures in existing owners instead of rewriting them. |
| 11. Data and input-editing closure | P21–P22 | Per-feature saving/reload/defaults, state/profile/sync contracts, shortcut edit/capture/undo APIs. Keep accepted sync restart-resend semantics explicit. |
| 12. Full Settings UI | P23–P24 | Only after the feature owners stabilize: native adapters, owned delegates, page lifecycle, full play catalog, search/default/help/retry, shortcut editor and audio/image pickers. No reduced replacement presented as parity. |
| 13. Platform and final acceptance | P25–P27 | Actual diagnostic/product surfaces, macOS notification port and separately qualified platform behavior; full feature/settings/action coverage and final regression/build checks. Windows trials continue while macOS runtime access is unavailable. |

Dependencies determine which stages can run concurrently. Three independent
collaborators can work in parallel, but shared files such as Config, native hook
registries and shared widget owners are assigned to one writer at a time.
The implementation-ready popup and badge corrections are the first batch.
Further cohorts do not require a new permission prompt between successful trials.

The complete, machine-readable checklist includes each task's substeps,
dependencies, status and focused player trial:
[play-parity-work.json](play-parity-work.json).
The behavioral preservation matrix is [PLAY-PARITY.md](PLAY-PARITY.md).
Neither document counts source presence as runtime acceptance.

## Completion criteria

1. Every actual play feature/reader/action/workflow has an implemented equivalent,
   an explicit accepted behavioral replacement, or a named unresolved limitation.
2. Each experimental DLL has a successful exact-client boot or a retained failure
   leading to a corrective task. Major interactive cohorts have player feedback.
3. Existing configuration/state/profile paths and preservation behavior survive;
   no silent reset, unknown-value loss or unrelated write is introduced.
4. Full Settings controls exercise the restored feature owners and truthful
   save/apply/restart states. Missing features are not made functional by labels.
5. Run the broader appropriate regression and Zig0.16/0.17 build checks at
   consolidation, preserving platform/runtime/signing distinctions. Full
   Windows completion is not silently generalized to macOS.
6. Checkpoint the complete primary play restoration before starting the later
   play-dev/AAI/Kit expansion. A game cycle does not authorize publishing.

Obsolete example-only `hud_daily_goals` and `disable_first_popup` are not
missing features: pinned play has no actual reader/consumer. The faction
filter's empty-vs-three-named default is behaviorally equivalent; it does not
earn an invented correction.

## Actual LexRunner use

Installed CLI/package: **LexRunner2.5.0**.
`merge_order` successfully read [the real dependency plan](play-parity-order.json):
29 items, nine dependency levels, maximum theoretical parallelism10. Actual
collaboration remains bounded to the available workers and exclusive file owners.

That dependency artifact intentionally has no gates. It is a planning artifact,
**not** verification or merge eligibility. Real candidate-specific gate plans
are separate and run actual build/test/artifact commands with explicit repoRoot,
idempotency keys and later artifact readback. Their results establish execution
evidence, not deployment/merge authority.

The current `preflight_attempt_containment` still reports
`unsupported/path_roots_overlap` for this existing checkout. Do not force an
Attempt by creating new checkouts or weakening controls. Independent collaborators
are not Runner-governed workers. Repo-aware ordering/gates remain useful and are
used directly; no tooling implementation project is part of this restoration.

The generic workflow guide remains bound to the shared non-repository container
and recommends unrelated initialization. It is advisory and does not override the
explicit owning repo. Use the demonstrated repo-aware interfaces instead.
Where a Runner tool actually accepts databasePath, use
`D:/dev/stfc-workspace/.smartergpt/runner/coordination.db`; do not invent that
argument on tools without it.

## Native development checkpoints — 25 October, batches 01–03

All candidates were built from HEAD606025e4 plus the recorded task delta. Runner
executed the actual build/fixture/static-PE commands with source-candidate binding;
artifact readback completed with six verified references and zero issues per
passing batch. This is gate execution evidence, not governed Attempt execution.
Evidence is under `D:/dev/stfc-workspace/.ax/ripper-play-restoration-20261025`.

| Batch | DLL SHA256 | Native result |
| --- | --- | --- |
| 01 | `3fcd225bdb98b3260f196c7e85fa2545f932626eea0a6ee8e2bac61e5462b0c5` | 50 groups installed; own-ship shortcut labels and native other-player labels user-confirmed. Popup lifecycle restored; offer/donation interaction remains a later trial. |
| 02 | `cc02aab3a9d4d654711ab8d172e86c0aed5b76bc1f839d120a6396cb87cabefd` | 51 groups installed; side-chat wheel ownership and keyboard zoom user-confirmed. |
| 03 | `d3f878d09112dbee057855281be0c6568a389ba60aa3b8ed3f177ba465b20e57` | Booted PID29128, 53/54 groups installed; BaseCard and DonationLimit installed, ShipTechIndicators unavailable and under investigation. No fresh startup errors. |

Batch03's first build exposed a GC-root pointer adaptation error; it was corrected
and the new batch03b operation passed 121/121 build/test steps. The failed receipt
is retained. Windows donation uses the qualified donation setter; macOS's older
row adapter remains separate and exact macOS parity is pending.

User is away for a nap and authorized successful launches as the initial barrier.
Continue small batches without requiring manual interaction acceptance. Boot-only
qualification must remain distinct from feature acceptance. No stores/configs
were reset or migrated. The parked dev-install tooling is unrelated and untouched.

### Batch04: input, mining, Haven and ship indicators

Candidate: HEAD967f4c7a plus worktree digest
`sha256:4697a10b61e6f8aabc35f55f246f72466d99feac910515393004f5ca32eeb352`.
DLL: `35fb63277600add5b4ccfa3cee065c15057b391c17f83c4c53b515c8c7f4bfd4`
(11,269,632 bytes, `zig-out/play-parity-04/bin/version.dll`).
Runner operation: `batch04c-gates/gate-operation-417c711b8eafb81c101df2aea3f4d38d337b5a74a37b973c70bd49fb775ca38e/operation.json`,
descriptor digest `sha256:69758364e746e873485f10420f13f1ef763ce6e0f83f32cc842b8afc08c2b578`.
All129 build/test steps and static PE checks passed; six artifact references
verified with zero issues. Earlier04/04b failed receipts remain intact: fixture
import-library search path and two managed-string GC-handle conversions were
corrected. No test assertion or product guard was weakened.

The authorized launch started PID62564 at the ordinary C-game installation.
All57 feature groups installed and FrameTick became ready. In particular,
ShipTechIndicators, MiningPreview, OpcIndicators and HavenOrbit all installed;
there were no fresh mod startup errors. The existing configured keyboard-layout
mode resolved `00000409` through the native Input System after startup. That
qualifies the subscription/lookup on this session, not layout switching or every
shortcut. No game TOML changes were made to get this result.

Added fixtures cover physical/layout chord identity, inferred Shift, modifier
ownership/release, unavailable mappings, cache identity/invalidation, strict OPC
thresholds, unknown rates, SAFE and rounded-up ETA. Complete source semantics are
preserved; boot alone does not prove node/cargo cycling, Haven dragging, ship art,
ETA row recycling or F11 visual behavior. Those await player interaction.

F11 now uses the existing FreeResize frame owner and one saved window placement;
there is no global SetWindowLong detour. Modified F11 and focused text are left
alone. A natively borderless window with no owned earlier placement stays native.
Existing resize, cursor, UI scale and camera owners remain; their full visual
qualification is still open. Haven and interior orbit constraints share one
terminal HookSet and call the native constraint method once.

Next: P12 system orbit plus its existing procedural scene enclosure, P18 custom
prepared audio/coalescing, then native notifications and Galaxy/preset cohorts.
P15's disproven pointer-only preset approach remains excluded. Settings UI remains
last, after its feature owners. Stores and unrelated checkouts are unchanged.

### Batch04 player acceptance and batch05 native checkpoint

Guff explicitly reported all pass for Swap Ship technology icons, Haven
right-drag/Alt-right reset, and V cycling node → foreign occupant → cargo → node.
These are player interaction observations beyond the original boot check.
F11/OPC display and different-layout switching remain separate pending trials.

Batch05 candidate: HEAD9a2018af plus worktree digest
`sha256:4ec977538d8e75a3d3eb0d4041a2e023a377d3b3b8faa4a3a784ca3a816799ac`.
DLL `1c288a1c611cc6d606ddc8331c0b7494461e32cc1291ba6fb8e2b139fd59cc0e`,
11,890,688 bytes at `zig-out/play-parity-05/bin/version.dll`.
Runner operation `batch05-gates/gate-operation-3ef6b8e1fd71ea389eeddd69df28876cfd2d903cf666af4736eb4e1abf0c45ce/operation.json`,
descriptor digest `sha256:f5d90848093d96790ecb7cd321881cc8f9e3bcfedd6dd1fdc5350ee87782bf08`.
All135 build/test steps and PE checks passed; six artifact references verified.
Includes real UTF-8 WAV decode, malformed/missing/size/duration boundaries,
shared clip/lifetime checks, mocked playback and pure coalescing. No MP3 sample
was supplied to the optional decoder test; MP3 and macOS runtime remain unqualified.

Authorized cycle used the built artifact: stop exact PID62564/creation
134374359626808884, copy verified DLL, start PID57636. All58 groups installed,
including NavigationOrbit; FrameTick and native US layout resolution are ready,
without fresh mod startup errors. No game TOML/store migration occurred.

P12 ports the existing system orbit/free-pan/anchor/clip behavior and deterministic
scene enclosure. Its scene assets are mod-owned and disposed on reset/source exit;
native backdrop artwork/material/visibility ownership is preserved. The native
camera/sky trial remains pending.

P18 retains one game-independent prepared cue model, original bounds and local
WAV/MP3 decoders, immutable shared buffers and monotonic coalescing. Parsing is
pure; accepted configuration activation prepares custom clips against its admitted
settings location. Existing built-in PCM and lazy cache remain compatible.
Windows Media Foundation is dynamically loaded; Windows N absence disables file
preparation without preventing DLL/built-in startup. Incoming/fleet source settings
are restored; their actual consumers are the next cohort. The P15 settings alone
are not an activated or qualified preset owner.

Operational rule reconfirmed by Guff: build and verify while the game remains up;
only then stop → copy → launch. Compose the existing lifecycle calls closely to
minimize downtime; do not introduce another deployment-tooling project.

### Published review checkpoint: batch06 and scope reset

This branch is an incomplete migration checkpoint for separate review, not a claim
of full PLAY parity. Hook installation, implemented source, offline fixtures and
player behavior remain separate evidence levels. Guff requested publishing the
checkpoint directly without running another review in this session.

Batch06 restores PLAY's Ctrl-click (Command-click on macOS) per-trip choice
intent and the modifier-sensitive native Set Course label. It captures exact
fleet/destination IDs at the manual move edge, survives modifier release during
planning, consumes once, and retains native choice when identity observation is
unavailable. The move hook and popup hook install atomically. Invocation and
hook installation reuse one qualified Nullable<Vector3> move-method binding.
Regression coverage includes replacement/mismatch/unknown/large-ID intent and
the shared checked native binding. This is not yet a player-confirmed Ctrl-click
trial.

The user's existing TOML was always retained. Its blank navigation_orbit_drag
and navigation_orbit_reset were honored as NONE; installed orbit hooks did not
mean enabled orbit behavior. The earlier report conflated those states.
System orbit is now explicitly parked: its source remains for recovery, but its
descriptor is absent from the product registry and its default patch flag is false.
Haven orbit remains enabled and player-confirmed separately.

Tested candidate: HEAD9a2018af plus worktree digest
sha256:dec091a5db50ca44a394be5e165735b6847e529b99aad670af878c4655a18c99.
Runner batch06-gates operation a42feee879153823f070234d51dc0767e3d96564aaf58601b8d6ef63e0a155f4,
descriptor sha256:fc3ba4a9dc7d398d005a68be266a57bbb105375dda8393f385bc5f66e83e22c0.
Windows install + test-mod + test-il2cpp:195/195 build/run steps succeeded;
static PE check passed; six artifact references verified. Step count is not a
count of independent test cases. Built DLL:
da186fd6825433e892fface2211155e5d26b5aefd7b98e04114d304cc8a9f569.
The already-built DLL was deployed and launched as PID14444. All57 registered
groups installed, including InstantWarp, and FrameTick was ready with no fresh
mod startup errors. No game settings or stores were reset. macOS runtime,
code signing/notarization and complete PLAY parity remain unqualified.

Known open cohorts: full native Mod Settings/catalog/editing; Galaxy enhancements;
incoming/fleet notification consumers; native officer-preset reordering. The
private P15 candidate is not imported: a reentrant-save transition gap remains
unresolved. Discovery towing and other legacy limitations are not silently
called complete. AAI/STFCKitUI expansion remains outside this checkpoint.

Unrelated dev-install tooling/build.zig changes remain local and excluded from
this checkpoint. No new tooling system, release promotion or merge is implied.
