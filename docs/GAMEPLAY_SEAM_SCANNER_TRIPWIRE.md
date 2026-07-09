# Gameplay Seam Scanner Tripwire

Date: 2026-06-10
Branch: `audit/gameplay-seam-scanner-tripwire`

## Purpose

This branch adds the first mechanical tripwire for gameplay seam governance. It does not migrate hooks, install a runtime `GameplaySeamRegistry`, or change runtime behavior.

The scanner makes unmanaged hook-like behavior visible:

- existing unmanaged raw hook sites are accepted by a deterministic baseline
- new unmanaged raw hook-like additions fail the scanner
- stale baseline entries fail the scanner so baseline changes remain explicit
- registry-backed or internal wrapper sites are reported as approved/internal, not violations

## Command

Normal AXF review contract:

```text
global.stfc-mod-private.review-contract
```

This is the blocking command agents should run before reporting code or hook-adjacent branches as review-ready.
It runs hook support-tier validation, the gameplay seam scanner, `git diff --check`, `git diff --cached --check`,
and AX `pure-tests`.

Hook support-tier validation:

```powershell
py scripts\validate_hook_support_tiers.py --format json
```

This gate reads `manifests/hook_support_tiers.json`, verifies registry-backed hook module coverage, and blocks
science/dormant config surfaces from `example_community_patch_settings.toml`.

Windows:

```powershell
py scripts\scan_gameplay_seams.py
```

Portable form:

```bash
python3 scripts/scan_gameplay_seams.py
```

JSON output:

```powershell
py scripts\scan_gameplay_seams.py --format json
```

Acceptance proof:

```powershell
py scripts\scan_gameplay_seams.py --self-test
```

The self-test creates a temporary fixture containing one registry-backed detour, one commented-out raw detour, and one unmanaged raw `SPUD_STATIC_DETOUR`. It passes only if the unmanaged raw fixture is detected as a new violation and a generated baseline then accepts it.

## Baseline

Baseline file:

`manifests/gameplay_seam_unmanaged_baseline.json`

Current scanner result:

```text
unmanaged findings: 93
approved/internal findings: 13
new unmanaged findings: 0
stale baseline entries: 0
```

The unmanaged findings are grandfathered legacy sites from the manual gameplay seam baseline. They are not endorsed as well-owned seams; they are accepted temporarily so future work can fail only net-new unmanaged hook-like calls.

The approved/internal count currently includes:

- `HOOK_REGISTRY_SPUD_STATIC_DETOUR` call sites
- the internal raw `SPUD_STATIC_DETOUR` used inside the registry wrapper macro

The scanner does not expand macro families. For example, `RefineryDiagnosticsHooks` has many runtime hook installs behind `INSTALL_REFINERY_DIAG_HOOK(...)`, but the static scanner sees the wrapper call site, not each macro expansion.

## What Fails

The scanner returns a non-zero exit code when:

- a new unmanaged raw `SPUD_STATIC_DETOUR` site appears outside the accepted baseline
- a new raw MinHook call such as `MH_CreateHook(...)` or `MH_EnableHook(...)` appears outside the accepted baseline
- an accepted baseline entry no longer exists in scanned source

These failures are review gates, not automatic proof of a bug. The expected response is one of:

- convert the new hook site to an approved registry wrapper
- reject the hook
- add a reviewed temporary exception to the baseline with rationale
- update the baseline after intentionally removing or migrating a legacy site

In `global.stfc-mod-private.review-contract`, scanner failures are blocking. The standalone scanner is also blocking
by exit code, but it is scoped to this tripwire only and does not replace the full review contract.

## Boundaries

Allowed on this branch:

- scanner script
- deterministic baseline
- scanner/tripwire docs

Not allowed on this branch:

- runtime `GameplaySeamRegistry` wiring
- hook install wrapper changes
- detour migration
- queue, sidecar, notification, battle log, or config behavior changes

## Follow-Up

Recommended next step:

`refactor/gameplay-dispatch-ownership`

Possible later scanner hardening:

- add CI wiring
- add richer ownership categories to baseline entries
- include hook descriptor metadata checks
- add scanner coverage for additional hook-like APIs if the repo adopts another detour backend
