# Probe: Client-253 central sync observation

- Status: approved
- Owner: `SyncHooks`
- Date: 2026-08-10
- Related patch label: client 253 / M93.3.0
- Related timeline refresh ID: verified client-252 and client-253 immutable snapshots
- Related diff report: local client-252-to-253 IL2CPP comparison
- Native seam ledger entry: `docs/NATIVE_SEAM_LEDGER.md`

## Question

Can `GameServerModelRegistry.ProcessResultInternal(IParsingContext, ServiceResponse)` provide one safe central owner
for the original entity-group type and protobuf bytes after client 253 removed the binary-container hook family?

## Static Evidence

- Symbol: `Digit.PrimeServer.Core.GameServerModelRegistry.ProcessResultInternal`.
- Method signature: `System.Void (Digit.Networking.Core.Parsing.IParsingContext,
  Digit.PrimeServer.Models.ServiceResponse)`.
- String/script/config evidence: the current client retains `ServiceResponse.EntityGroups` and typed message dispatch.
- Old/new diff context: `ParseBinaryObject` / `ParseBinaryObjectsHelper` were removed. The same-name
  `ParseEntitySlotsData` method changed its payload from `EntityGroup` to `EntitySlotsData` and is excluded.
- Why this target is the narrowest candidate: it replaces ten removed container seams with one established central
  owner and preserves the existing private payload builders, scheduler, sidecar, and transport contracts.

## Risk

- Risk class: R4
- Confidence rung: runtime observed in both the Netniv validation checkout and this private integration
- Payload confidence: understood `ServiceResponse` / `EntityGroup` wrapper only; payload bytes are copied before
  asynchronous processing.
- Original/trampoline confidence: prior client-253 validation; private build must still be observed.
- Behavior change expected: no

## Implementation Plan

- Module/file: `mods/src/patches/parts/sync.cc`
- Config or compile guard: existing `SyncPatches` configuration
- Hook descriptor name: `model-registry-process-result`
- Target assembly: `Digit.Client.PrimeLib.Runtime`
- Target namespace: `Digit.PrimeServer.Core`
- Target class: `GameServerModelRegistry`
- Target method: `ProcessResultInternal`
- Install path: exact return/parameter signature gate, process-global single-owner claim, one SPUD detour
- Log tag or event kind: bounded `[HookRegistry] module=SyncHooks` health and opt-in sync transport metadata

Registry requirements:

- Uses `HookDescriptor`.
- Uses `HookModuleHealth`.
- Uses `HOOK_REGISTRY_SPUD_STATIC_DETOUR`.
- Does not use raw `SPUD_STATIC_DETOUR`.

## Disable Path

- Flag or code path to disable: disable the existing sync patch module.
- File/entry to delete if it crashes: remove the `model-registry-process-result` install block.
- Expected boot log when disabled: patch-table skip for `SyncPatches`; no `SyncHooks` installs.

## Human Smoke Test

Goal: prove private-fork boot safety and representative existing receiver contracts on client 253.

Steps:

1. Build and deploy the private Windows release DLL through the repo-local AX lifecycle.
2. Reach the playable station/system view.
3. Require the `SyncHooks` summary to report all active seams installed or explicitly skipped/replaced with zero
   failed.
4. With bounded metadata logging only, observe mission, forbidden-tech, officer, module, trait, research, ship, and
   resource queue/upload results.
5. Confirm identical snapshot/realtime slot state is suppressed by the pure test; observe a live transition if one
   naturally occurs.

Expected log marker/event: one central observation seam installed, supporting session/version/instance/RTC seams
installed or explicitly skipped, removed seams reported as replaced, and receiver HTTP 2xx responses.

Stop immediately if: signature mismatch, duplicate owner, ABI/trampoline error, crash, missing-method storm, payload
body logging, or repeated identical slot events.

Report back: hook-health summary, representative accepted categories, duplicate count, and schema errors.

## Result

- Build/deploy commands: `xmake -y stfc-community-mod`, private AX release deploy/cycle, and a temporary
  `releasedbg` cycle with `[sync].logging=true` and `[sync].debug=true` for bounded metadata only.
- Runtime command: private AX cycle plus filtered observation of `SyncHooks` and `SYNC-UPLOAD` status metadata.
- Human action performed: none required; the client reached its normal authenticated initial data load.
- Observed log/event evidence:
  - `SyncHooks summary installed=6 replaced=11 failed=0 skipped=0 total=17`.
  - `ProcessResultInternal`, both slot RTC parsers, Prime initialization, game-version capture, and instance capture
    installed after exact return/parameter validation.
  - The configured receiver returned HTTP 204 for officer, mission, trait, forbidden-tech, research, and module
    uploads, and HTTP 204/200 for resource uploads.
  - One ship upload returned HTTP 400. This change does not alter the existing ship payload builder; retain this as
    a separate receiver-contract uncertainty rather than evidence against the central observation seam.
- Crash/hang/recovery notes: no crash, ABI failure, duplicate owner, missing-method storm, or new post-boot error.
  The first final release swap lost a race with the STFC launcher auto-restarting the client and locking
  `version.dll`; stopping only the STFC launcher/client processes allowed the normal release cycle to complete.
- Cleanup: `[sync].logging` was restored to `false`, the temporary `debug` key was removed, and the final deployed
  DLL is the release build with a verified build/deploy hash match.
- Answer to the question: yes. The existing central service-response seam safely supplies the entity-group type and
  protobuf bytes needed to restore representative client-253 sync contracts in the private fork.

## Exit Decision

Promote the single central seam for client 253 and keep the removed/changed detours prohibited. Track the isolated
ship HTTP 400 separately if it reproduces with receiver diagnostics; do not change that contract in this repair.

Next action: revalidate the exact signatures and representative receiver categories after the next client update.
