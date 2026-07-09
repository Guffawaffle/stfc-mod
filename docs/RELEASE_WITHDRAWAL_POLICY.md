# Release Withdrawal Policy

This fork defaults to preserving release history. Deleting a release and tag is
reserved for user-impacting danger, a clearly broken artifact, or an artifact
that must not remain downloadable.

## Release States

| State | Meaning | Default action |
| --- | --- | --- |
| Superseded | The release is safe enough to keep, but a newer release should be preferred. | Leave the release and tag in place, mark the release notes/title, and make the replacement release latest. |
| Known-bad | The release is useful for audit/history, but users should not install it. | Leave the release and tag in place, mark the title and notes prominently, mark it prerelease, and make the replacement release latest when available. |
| Yanked | The release artifact is dangerous, clearly broken, or should not remain downloadable. | Delete the GitHub release and remote tag intentionally, after recording the reason and replacement. |

Deletion is not the default cleanup tool. Prefer `superseded` or `known-bad`
unless keeping the artifact available would create user harm or ongoing support
confusion.

## Required Record

Every withdrawal action needs a durable record containing:

- affected tag
- state
- reason
- replacement tag, if one exists
- operator
- timestamp
- original release URL/target commit when available

For `superseded` and `known-bad`, the release notes carry the visible warning
and the repo ledger records the action. For `yanked`, the release notes disappear
with the release, so the repo ledger is mandatory.

The ledger lives at:

```text
docs/release-withdrawals/release-withdrawals.jsonl
```

Commit the ledger update after an executed withdrawal.

Release-withdrawal commands accept only stable fork tags shaped like
`vX.Y.Z-guffa.N` for both the affected tag and replacement tag.

## Command

Use `global.stfc-mod-private.release-withdrawal`, or run the backing script
directly:

```powershell
.\scripts\axf\release-withdrawal.ps1 `
  -Tag v2.1.0-guffa.4 `
  -State known-bad `
  -Reason "Kir'Shara queue regression in production artifact" `
  -ReplacementTag v2.1.0-guffa.5
```

The default mode is dry-run. It prints the planned release edits or destructive
actions without changing GitHub.

To execute after reviewing the dry-run:

```powershell
.\scripts\axf\release-withdrawal.ps1 `
  -Tag v2.1.0-guffa.4 `
  -State known-bad `
  -Reason "Kir'Shara queue regression in production artifact" `
  -ReplacementTag v2.1.0-guffa.5 `
  -Execute
```

For a yanked release, the command writes a `pre-yank` local ledger record before
the destructive GitHub action, prints the exact delete command to stderr before
running it, and then writes a `post-yank` ledger record with the delete outcome:

```powershell
.\scripts\axf\release-withdrawal.ps1 `
  -Tag v2.1.0-guffa.4 `
  -State yanked `
  -Reason "Artifact crashes on boot for confirmed users" `
  -ReplacementTag v2.1.0-guffa.5 `
  -Execute
```

That path runs the equivalent of:

```text
gh release delete <tag> --repo Guffawaffle/stfc-mod --cleanup-tag --yes
```

Use `yanked` only when preserving the artifact is worse than losing the public
release/tag history.
