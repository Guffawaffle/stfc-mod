# Runtime Identity

This policy applies to the loaded mod binary and the build metadata embedded in it. Launcher, bundle, installer, and
distribution migration work is outside this policy.

## Canonical record

[`manifests/runtime_identity.v1.json`](../manifests/runtime_identity.v1.json) is the source of truth for the downstream
distribution ID, display name, unofficial status, upstream base, and build-class labels. The approved downstream
identity is:

- Distribution ID: `guffawaffle.stfc-community-mod`
- Display name: `Guffawaffle STFC Mod`
- Status: `Unofficial downstream build`

The build generates `runtime_identity.generated.h` from that record. Runtime logs, diagnostics, loading text, and
Windows `VERSIONINFO` consume the generated values through `runtime_identity::Current()` and `version.h`.

## Build classes

| Class | Purpose | Additional requirements |
| --- | --- | --- |
| `release` | Maintained fork release | Public-release profile, release tag, clean reproducible Git source |
| `test` | Temporary build for a named integration target | Target, expiry condition, and support boundary |
| `development` | CI or maintained development branch build | Exact CI source and base commit when distributed |
| `local` | Local developer build | Source is derived from the worktree and fingerprinted when dirty |

CI must pass `--stfc_source_state_id=git:<sha>` and `--stfc_base_commit=<sha>`. A dirty local build uses a
`dirty-sha256:<fingerprint>` source ID derived from tracked changes and untracked blob hashes. It is explicitly marked
non-reproducible. In a Git checkout, configured source and base identities must match `HEAD`, and a clean identity is
rejected when the worktree is dirty. Source archives without `.git` may provide matching explicit source and base
identities. A release build fails configuration if it cannot prove a clean Git source. A test build fails configuration
unless all three temporary-build fields are set.

The copyable support identity includes the downstream name and version, unofficial status, build class, distribution
ID, exact source state, downstream base commit, upstream base, and reproducibility flag. Test identities also include
their target, expiry, and support boundary.

## Runtime artwork

The default runtime preserves the game's original loading background and does not add a downstream or upstream logo
overlay. The `official-cc-logo.png` asset and its generated header were removed because the mark could imply official
status. The former embedded upstream loading image and generic logo headers were also removed from the distributed
binary path.

The following artwork remains in `assets/` as source history or launcher/installer input, but is not embedded by the
mod runtime: `loadingscreen.png`, `stfc-mod-logo.png`, `stfc-mod-logo-fullsize.png`, and `LoadingScreens/*`. Retention
preserves provenance; it does not communicate endorsement. A builder may explicitly provide `--bg_image=<path>`, and
a user may configure `loader_image`, without changing the build's identity or unofficial status.

## Examples

```powershell
# Local build; source identity is derived from the worktree.
xmake f -p windows -m debug -y

# Temporary integration build.
xmake f -p windows -m release -y `
  --stfc_build_class=test `
  --stfc_source_state_id=git:<sha> `
  --stfc_base_commit=<sha> `
  --stfc_test_target=<repository-or-candidate> `
  --stfc_test_expiry=<expiry-condition> `
  --stfc_support_boundary=<support-channel>
```
