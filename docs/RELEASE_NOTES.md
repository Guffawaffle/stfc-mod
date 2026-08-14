# Release Notes

Release notes are player-facing product communication, not a raw commit or pull-request inventory. The release body uses two layers:

1. A required curated fragment explains the release in player terms.
2. The generator appends assets, merged pull requests, fixed issues, fork links, and the full comparison.

## Curated Fragment

Every tag matched by the release workflow (`v*`) requires a curated fragment, including stable, RC, alpha, beta,
and legacy tag formats. Before creating the tag, add:

```text
docs/release-notes/<tag>.highlights.md
```

Start the file with `## Highlights`. Use the section order from `docs/release-notes/TEMPLATE.highlights.md`, omitting sections that do not apply.

The fragment should answer:

- What changed in behavior a player can observe?
- Why does the change matter?
- Does the player need to change configuration or installation steps?
- What intentionally stayed unchanged?
- Are there known limitations or support boundaries?

Write concise outcome-oriented bullets. Internal refactors, test machinery, and implementation details belong under `## Technical Notes` only when they affect compatibility, provenance, support, or release risk.

Do not repeat the release title, disclaimer, asset inventory, merged pull requests, issue list, fork README link, or compare link. Those sections are generator-owned.

## Release Flow

1. Prepare and review the curated fragment before tagging.
2. Run `python -m pip install --require-hashes --requirement scripts/requirements-release.txt`, then run
   `scripts/generate_release_notes.py` locally with `--require-curated-notes`.
3. Smoke the exact successful production artifact and acknowledge it through release preflight.
4. Push the tag. The release workflow fails if the curated fragment is missing or malformed.
5. After publication, verify uploaded hashes, signatures, and manifests.
6. Update the live `## Verification` section with the exact release artifact hash and smoke result when those facts were not available before tagging.

The generated pull-request and issue inventory is supporting traceability. It is not a substitute for the curated player-facing explanation.
