# Swift launcher preservation checkpoint

Existing `play-dev-ripper`, starting head
`80bc425c7c06b36d22566d6e863531c3740fbbc5`, frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`. This bounded slice completes the
source mapping of the remaining Mac launcher workflows. It does not establish
native platform parity or release-promotion readiness.

## Restored behavior and source findings

[Launcher UI](features/PLAT-MAC-LAUNCHER-UI.md) restores two proven omissions:
Open TOML and View Mod Folder, using the frozen ordinary mod directory/config
helper. File actions check existing paths and report opening failures without
creating settings. Four 92-point buttons restore the original row width inside
the fixed window; target update/launch admission and error-reporting improvements
are retained.

[Entitlement selection](features/PLAT-MAC-ENTITLEMENTS.md) corrects another
reachable mismatch: the typed selector could sign another STFC copy while the
loader still launched the configured one. Selection must now match the configured
bundle after canonicalization, and configuration/running state is checked again
after the authorization dialog. Selected-resource access remains balanced.
No real executable or signature is changed by this development work.

[Game updater](features/PLAT-MAC-UPDATER.md) preserves staged patching, repeated
patch bases, deferred deletes, executable modes and version-last publication.
No updater product change is needed. Removed progress events were no-ops in the
frozen UI, and its numeric progress was not rendered. Neither version supplies
a cancellable game-update transaction, repair button or atomic rollback. Preserve
current parser/error/lifetime corrections instead of adding those features.
Native responsiveness of the current main-actor file work remains open.

[Packaging](features/PLAT-MAC-PACKAGING.md) retains ordinary bundle resources,
universal layout, archives and DMG structure. Optional signed test delivery remains
on the accepted separate ci/macos-signing-play controller; its absence from this
tree is intentional. The migration tree's generic upstream tag publisher is not
the frozen fork's signed downstream promotion workflow. Release policy must be
reconciled before promotion; no workflow/controller import, tag or publication is
part of this slice.

## Evidence and limits

Source observations compare complete frozen/current owners and record the two
missing actions and the entitlement warning-only branch at the starting head.
These are static observations, not executable Swift/UI failures.

The existing Linux test-launcher fixture passes 6/6 build/run steps, using the
production rsync bridge and real generated deltas, missing/corrupt inputs and
output errors. Its librsync error lines are expected fault cases. It does not
compile Swift, exercise archive extraction, or validate UI/entitlement behavior.
No test target or CI registration is added.

The return to the core audit consolidates [SET-VALUES](features/SET-VALUES.md)
under its actual Boolean, page, choice/slider, exact-entry and persistence owners.
No core implementation change is needed for that mapping. Six existing Windows
consumer/model checks pass, including choice/slider consumers and Boolean,
choice, slider and exact-entry models. Core source/executable hashes from the
preceding camera checkpoint remain identical. SET-VALUES stays Partial; these
checks do not close successful live Apply/save/restart or extended native/Mac
acceptance.

The first Linux attempt failed to rename cache results on the Windows-mounted
drive. Reusing the same source with task-owned /tmp compiler caches passed.
Both logs remain distinct. Ubuntu also reported a failed systemd user session;
the compiler/fixture still completed. No new compiler was installed. Neither the
Windows nor inspected Ubuntu environment supplies swift/swiftc.

Both Mac launcher requests stop before compilation because the configured SDK is
missing; native Swift/Xcode execution is unavailable on this host. SwiftUI/AppKit
type checking, four-button layout, file associations/Finder/alerts, authorization,
same-target refusal, actual updater/failure recovery and signed bundle/runtime
acceptance remain open. Mcohall's earlier pre-release reports qualify only that
artifact, not these local source changes.

Evidence is under `D:/dev/stfc-workspace/artifacts/swift-launcher-20261103/`:
source-observations.json, local-verification.json, both Linux logs and mac-builds.json
bind their respective layers. Whitespace checks pass. Review and signed source
checkpoints are recorded after the final source freeze.

Signed source checkpoint `3b472c18d59489030505c8bd06bfdbee841cc680` has a good
signature (local signer trust unknown). General, hostile and independent reviews
found no actionable findings against base `80bc425c`, diff hash
`c823b3cbb97fbee5f114170ba4e2c92982ab530a`. All21 launcher/related and14 core source
hashes, the captured Linux fixture executable and six Windows case records match.
The final Linux run captures its executable in the same session; that hash binds
the retained binary, while the earlier logs retain their distinct cache outcomes.
Review receipts retain static-persona fallback because their CLI memory lookup
was unavailable; scoped AXF continuity works separately.

No executable Swift/UI verification is inferred from those reviews. The final
documentation checkpoint records this result without changing production or test
source. Continue core donation/dock-assignment observation and failure-path
reconciliation, reproducing any candidate gap before changing behavior.

No real launcher settings, mod TOML, account/catalog data or game files were read
or changed. No app/file opener, game cycle, installation/update, codesign,
notarization, deployment, pre-release replacement or remote write occurred.
AAI and new upstream features remain deferred. Continue the remaining core
migration catalog linkage/coverage audit, retaining these Mac qualification gaps.
