# Claim filtering and confirmation targeting

Source: existing `play-dev-ripper` in Ripper Downstream, starting clean at signed
`828095b945d08dd0ac32aa618a471450efe057e5`. Implementation: signed
`604091667c45d9b41244773789342823ed2163e9`. Frozen behavior remains PLAY
`cad98ef7990dd66167eb64ea5dda525c1fe2c296` and the previously selected play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998`. No branch/worktree change.

## Source comparison and bounded corrections

Gift-drawer opening, faction filtering, icons and the once-per-session default
toggle were already present and assessed in the earlier shop/claims cohort.
The current drawer still checks its weak landing/revision/context and marks an
attempt before the native click. This pass leaves that owner unchanged and
reruns its actual-consumer fixture.

The daily filter inspected a reward, then removed that index without confirming
the list still contained the same daily. A controlled reward observation that
replaces the excluded daily with a selected one causes the unchanged filter to
remove the replacement. The corrected hook roots the original native list and
inspected daily, distinguishes unavailable count/item observations, and checks
expected count and row identity before removal. Current preferences are read
again before mutation. Unknown/changed observations stop further filtering.
Earlier successful removals remain applied; there is no rollback or substitute
claim collection. This edge is not established as a new migration regression
or a live user-reported failure.

The frozen FT source checked the callback target's declaring owner and the
RequestAction method name. The migrated predicate instead checked the method's
qualified owner name without confirming its closed receiver. A matching method
on an unrelated target therefore passed. The correction requires the actual
target class to match the method's declaring closure before the existing owner/
method-name predicate can authorize bypass. The same guard covers the existing
alliance-contribution path; answers remain FT Confirm1 and alliance Yes3.
Null, unknown, sibling or unrelated targets retain the original popup/callback.
Managed callback failures retain the existing native fallback; a callback may
already have had partial effects before throwing, as recorded previously.

No new hook group, feature/default, automatic claim, reward eligibility rule,
purchase policy, diagnostic probe or native action is introduced. Reveal/warp
confirmation owners are unchanged and remain part of the remaining P19 sweep.

## Verification

New explicit modes in the existing host call the actual production hooks.
`shop-claims-filter` fails on the base at the replacement-row assertion;
`confirmation-targeting` independently fails at the unrelated-receiver popup
assertion (both exit1). Corrected modes pass replacement/unknown-count/live-filter
changes, list/row roots, stable multiple removals, unrelated/sibling/unknown
closure receivers, and preserved FT/alliance answers/fallbacks. The existing
`shop-claims-native` lifecycle/toggle/HUD/confirmation cases also pass. All native
reward, popup and action boundaries in these fixtures are counted substitutes.

Existing `test-il2cpp test-mod` passes 312/312 build/run steps; clean/stable
Windows releasedbg passes 41/41 steps and proxy verification at the implementation
commit. Candidate DLL SHA256: `81CF2AFB7C2D358690163DC5715B16959935AC589686294AE31DC861DF188FD9`.
Step counts are not test counts; no CI/build-target registration changed.

Evidence: `D:/dev/stfc-workspace/artifacts/claim-confirm-targeting-20261101/`,
including baseline fixture, command/cwd/base/duration/exit/executable hashes,
exact final inputs, signed checkpoints, native receipt and preserved DLL/PDB.

## Qualification and continuation

This candidate is local and undeployed. No game input, cycle, TOML edit, reward
claim, upgrade, purchase or ability action was performed. The installed combined
Settings candidate and published prerelease are unchanged by this pass.
Exact-artifact live workflow and Mac build/runtime qualification remain open.

PLAY-CLAIM and PLAY-CONFIRMATIONS retain their overall classifications. P19 is
now explicitly in progress; its remaining HUD/cargo/reveal/warp and platform
work is not closed by these bounded corrections. Settings passes remain stopped.
