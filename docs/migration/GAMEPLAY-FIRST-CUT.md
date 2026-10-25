# First playable migration cut: gameplay and store compatibility

Decision: 2026-10-25, after [M2](M2-HOOKS-AND-FRAME.md).
Guff wants downstream gameplay parity and preservation of existing data before a
normal in-game migration trial. The native Mod Settings screen, AAI and
STFCKitUI may be integrated later. Deferring those interfaces must not defer the
configuration, profile or persistence behavior needed by ordinary gameplay.

This updates integration priorities; it does not activate Profiles, authorize
deployment, or claim that the current foundation is already store-compatible.
No storage-engine/schema/settings-location migration is part of this cut.

## Current state

The qualified first gameplay/sync source checkpoint is
[`f0f9c2db`](M8-GAMEPLAY-AND-SYNC.md#qualified-source-checkpoint). It restores
registered gameplay persistence, stable ship pins and sync boundaries with
source-bound Windows/portable checks. It is not full gameplay parity or a
deployed runtime qualification. The next preservation deltas are listed there.
The separately qualified `5fbb1ec9` follow-on also restores queue-toggle saving;
its narrower gates and remaining qualification are recorded in the same report.
The [presentation preservation checkpoint](M8-PRESENTATION-PRESERVATION.md)
qualifies `5e58d3d7`: native HUD restoration/Refresh, checked toast context
fallback and pure officer ordering. Native officer reorder activation remains
separate from its policy qualification.
The [Haven visual checkpoint](M8-HAVEN-VISUALS.md) qualifies `c2fb9b5a`: water
visibility, reversible road depth and checked native observations. Native scene
qualification and a correctly routed isolated Zig cycle remain outstanding.

At the earlier M2 baseline, `493f669c` was an independently qualified foundation,
not a gameplay-parity build.
Relative to Ripper's pinned final stack, M2 changes hook/startup/frame ownership,
not the gameplay-domain implementations. Many analogous features exist, but
matching names, fixture success or successful compilation do not establish parity
with downstream `45d1deaf`.

Examples still needing reconciliation include stable-ID pins, officer preset
reordering, OPC/ETA and ship-tech indicators, navigation/galaxy/Haven extensions,
claim/shop recovery fixes, notification behavior and sync. Queue protection and
daily faction claim filtering already have foundation implementations; compare
their behavior rather than describing them as absent.

## Source-verified store concerns at the M2 baseline

[M3](M3-PROFILES-HOST.md) subsequently restores the required host lifecycle and
file routing on the isolated branch, with offline source-bound qualification.
Its opt-in host build is still not a gameplay/store-parity deployment candidate.
Configuration/state and sync concerns below remain until their own integration.

- `src/mod/file.cc` understands `-ccm`, not downstream `-stfc-profile` admission.
  Custom filenames do not provide account isolation. The active Profiles
  bootstrap and its installation/session leases must be restored before a
  launcher-managed profile can be used.
- `Config::Get()` invokes legacy cleanup, reads configuration and writes effective
  runtime vars at startup. It creates default primary settings if missing.
  Existing primary TOML is normally read without rewriting, but launch is not
  read-only. Legacy files and logs also have startup mutation paths.
- The foundation's configuration save uses plain `std::ofstream`, not the
  downstream guarded/preserving writer. Ship-pin interaction can rewrite the
  primary TOML, with a parsed-table fallback that loses comments. Unknown settings
  remaining in a file does not mean their gameplay behavior was preserved.
- Ordinary gameplay toggles, scale changes and camera presets need persistence
  reconciliation even without a Settings screen. Current in-memory changes can
  be replaced by automatic reload. Active/restart-only settings reporting also
  needs to remain truthful.
- The downstream `mod_state` owner is absent. Existing
  `community_patch_state.json` / custom `<stem>.state.json` files are not migrated
  or erased by an absent owner, but their settings are not consumed or maintained.
- The new sync implementation uses in-memory/session state instead of consuming
  the existing sent-journal file. Leaving old bytes untouched does not preserve
  restart/delivery semantics. External target behavior requires qualification too.
  Correction to the earlier study: `sync.file` was already deprecated/removed in
  downstream `45d1deaf` (`mods/src/config.cc:1377`), as it is in the foundation.
  It is not a missing capability to restore.

These are code findings, not observations of a deployed candidate. No live
preferences, browser stores, catalog contents or account data were read or changed
to establish them. Detailed source comparisons remain in the private migration
study and M2 preservation notes.

## Dependency-aware priorities

1. **Profiles and bootstrap:** preserve the existing embedded pin, three-phase
   admission, ordinary installation exclusion, explicit-profile fail-closed
   behavior and platform entry contract.
2. **Configuration and persistence core:** restore existing routing, registered-key
   writes, conflict/atomic replacement behavior, state ownership and gameplay
   persistence. Preserve paths, formats, unknown data and malformed-file recovery.
   The Settings UI can stay absent.
3. **Gameplay cohorts:** reconcile input/tracking and queue/confirmation guards,
   then ship/officer/dock/cargo, navigation/galaxy/Haven, claims/shop/HUD/chat and
   audio/notifications. Use the preservation inventory, not hook-group counts,
   as the parity checklist. Do not activate held science probes incidentally.
4. **Sync compatibility:** qualify session isolation, budgets,
   journal acknowledgements/restart behavior and diagnostic extension points.
   Offline work can run alongside gameplay cohorts; live delivery waits for its
   own qualification.
5. **First normal in-game trial:** only after the preceding store and gameplay
   contracts are sufficiently qualified for the explicitly selected installation
   and profile. Preserve recovery copies; verify no unintended migrations,
   resets, cross-account writes or incompatible changes. Normal compatible game
   and settings writes are expected; zero file I/O is not the goal.
6. **Deferred interfaces:** add Mod Settings UI, AAI and STFCKitUI in separately
   qualified slices. Their implementations remain preserved, not discarded.

The original milestone IDs remain useful: M3 and M4's core precede the first
playable cut; independent M8 gameplay cohorts may precede M5/M6 UI integration.
UI-dependent gameplay interactions still need their actual prerequisites. This
is a sequencing change, not a waiver of preservation or runtime acceptance.

The Codex execution-context issue remains separately parked. Its failure does not
justify changing account identities, shared lock roots or safety assertions.
