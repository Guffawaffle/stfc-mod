# PLAT-MAC-ENTITLEMENTS — configured launch target

Source contract v1, 2026-11-03, starting at typed head
`80bc425c7c06b36d22566d6e863531c3740fbbc5`. Frozen
`1baa55fedf1758b19f843cc59707b8901a0b4998` and current PatchEntitlements.swift
inspect the configured game's signature before launching the loader. This is
separate from signing the distributed mod/launcher through the trusted controller.

The three required loader entitlements remain unchanged. An executable already
carrying them needs no authorization panel or signature change. Otherwise the
existing panel describes the executable and requested changes, lets the user
cancel, and obtains access to the selected bundle. The current implementation's
checked tool errors, balanced selected-resource access and main-executable-only
signing remain intact.

## Corrected target mismatch

Frozen allowed another bundle selection but still signed the original path.
The typed starting implementation correctly used the selected resource's scope
and executable, but merely warned about another selected app. The C++ loader
continues to launch the configured GAME_PATH. Selecting another STFC copy could
therefore sign that copy and then launch the unchanged configured game.

The corrected flow obtains and balances selected-resource access before its path
checks, resolves symlinks/standardized paths, and refuses a bundle different from
the configured one before calling codesign. The selected bundle must contain the
expected executable. Launcher settings are reread after the panel; a changed or
unavailable configured target refuses the operation. Running-app state is checked
again after the panel, with the same canonical path comparison. Directory-URL
hints do not decide equality: comparisons use canonical path strings.

The panel now asks for the configured bundle explicitly. Using another
installation requires cancelling and updating the existing Xsolla setting first;
this slice does not silently rewrite launcher settings or route another game
through a newly introduced argument. There is no filesystem/process lock or
atomic launch/signature transaction implied by these checks.

## Qualification

This is a source-level correction, not a native entitlement test. Swift/AppKit
and the Mac SDK/Xcode are unavailable on this Windows host. No panel, security
scope, running-app query, codesign operation, installed executable or game launch
is exercised here. Native cancellation, same-target/symlink selection, refusal of
another installation, settings/running-state changes during the panel and
signature verification remain required platform evidence. Source review and
the unrelated Linux rsync fixture do not qualify those operations.

The [launcher cohort](../FEATURE-COHORT-20261103-SWIFT-LAUNCHER.md) binds the input
hashes, source comparison, build failures and review. AAI, new upstream features
and real installation changes remain outside this source slice.
