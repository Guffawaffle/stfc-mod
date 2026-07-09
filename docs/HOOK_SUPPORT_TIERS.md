# Hook Support Tiers

`manifests/hook_support_tiers.json` is the source of truth for hook support
tier decisions.

## Tiers

- `production`: user-ready and release-supported behavior.
- `science`: active investigation or development-only behavior.
- `dormant`: retained history or compatibility code that must not be
  runtime-installed.
- `internal`: implementation plumbing or shared fan-out that is not
  user-facing.

## Release Config Rule

`example_community_patch_settings.toml` is release-facing. It must advertise
only production/user-ready settings.

Science and dormant settings belong in `example_science_patch_settings.toml`
or in focused investigation docs with explicit warnings. The blocking review
contract runs:

```powershell
py scripts\validate_hook_support_tiers.py --format json
```

That validator reads `manifests/hook_support_tiers.json` and fails if any
manifested non-production config surface with `release_example_allowed = false`
appears in `example_community_patch_settings.toml`.

## Kir'Shara

Kir'Shara queue repair is currently `dormant`.

The repair and marker detours remain represented in
`mods/src/patches/parts/action_queue_repair.cc` as dormant hook descriptors, but
runtime installation is force-disabled. The preserved config surfaces are:

- `advanced.kirshara_queue`
- `advanced.diagnostics.kirshara_queue`

Those surfaces are kept in `example_science_patch_settings.toml` only.

## Promotion Process

To promote a hook or config surface from `science` or `dormant` to
`production`:

1. Update `manifests/hook_support_tiers.json` with the new tier and rationale.
2. Provide runtime evidence. For user-facing behavior, smoke a production build
   or document why runtime validation is not applicable.
3. Move or add release config examples only after the manifest tier is
   production.
4. Run the AXF review contract.
5. Call out the promotion in the PR summary.

The point is to keep experimentation cheap while making release support
explicit.
