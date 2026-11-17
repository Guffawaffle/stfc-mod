# Retained ordinary system-warp preservation

This cohort continues the [Haven checkpoint](M8-HAVEN-VISUALS.md) in the existing
isolated migration worktree. The behavior oracle is downstream
`45d1deaffddc3706e5c8232cf746ca46ef67be91`, specifically
`mods/src/patches/planned_system_warp.cc` and its two hidden-popup primary-action
branches in `mods/src/patches/parts/hotkeys.cc`.

Tash's confirmation, relayed by Guff, resolves the SPUD access question: its
repository is private and the supplied snapshot is the most recent supplied
source. The migration continues using its fixed vendored snapshot; no dynamic
pull, dependency update or claim of independently reading private HEAD is made.

## Preserved player behavior

After a galaxy destination popup closes, the primary action may resume the
selected fleet's unused ordinary system course. The old fork does this only
after checking that the retained course still belongs to that fleet and
destination. It consumes this hidden-popup action even when recovery refuses;
it does not fall through to a hidden native action handler.

The foundation had no equivalent retained-course qualification. With the
visibility preference off (its default), it could call the hidden viewer's
native action; with that preference on, it could decline the action instead.
This is a source-verified difference. No new wrong-destination outcome was
observed in the running game during this investigation.

Visible target actions, secondary actions, system-view movement and armada
actions retain their existing owners. No setting, shortcut, hook transaction,
frame callback, persistence owner or dependency is added for this recovery.
The game's movement handler continues to own confirmations and network work.

## Qualification boundary

Recovery requires known observations of the current galaxy scope, selected
fleet, unused ordinary warp course, valid path and ordinary system target.
Recall, wormhole, toll and other special travel retain their native flows.
The course's start must match the selected fleet's current system and instance.
Its path object must match the interaction context's retained path, and its
target galaxy position must match the retained world position after the
native coordinate conversion. A surviving tree node must match the target;
an observed null tree node is permitted by the old recovery contract.

Missing values are unavailable, not observed false, zero or a successful match.
Finite positions and the old strict 0.01-per-axis projection tolerance remain
requirements. The native navigation validator must return a qualified true
before requesting movement with the explicit target node, fleet and position.

## Managed action boundary

An ordinary typed `Nullable<Vector3>` signature is insufficient here: the
generic binding's value-kind and size checks do not establish the nullable's
payload and field layout. This action needs narrow validation of the actual
nullable class, Boolean flag, Vector3 payload and their offsets. The runtime
invoker handles the aggregate argument rather than assuming a platform C++ ABI.

The new action's scalar qualifiers also retain the old exact signed I4/I8 and
Boolean requirements, including enum storage. The foundation's general
size-compatible integer matching remains unchanged. Equal-sized unsigned
values must not alias a signed action sentinel. New recovery getters and calls
must have usable invocation entries; a present metadata record alone is not a
callable method. These checks belong to this bounded action boundary, not a
rewrite of all existing bindings.

Managed custody is local to the synchronous action. No borrowed course,
context, helper, path or target pointer becomes retained application state.
The feature continues using game bindings rather than raw runtime APIs.

Viewer admission, secondary-input priority and primary-action routing remain
separate. An ineligible stale star viewer leaves the remaining viewer chain
available. Inside an eligible viewer, secondary input wins a simultaneous
press. Only a hidden galaxy primary action takes the retained-course path, and
that branch consumes refusal. Re-reading the same old widget is insufficient:
the captured source must still be current and live before its context is used.

## Verification status

An independent read-only check matched 50 expected declarations across 19
classes in the hash-verified retained client 273 dump. This supports named
binding selection; it does not qualify the installed client or native ABI.
The open generic nullable declaration does not establish the closed Vector3
payload's storage. That remains an actual-metadata check before invocation.

The completed focused Windows Zig 0.16 gate passed 67/67 steps, including the
native mod build, existing IL2CPP checks, the shared policy and all 20 isolated
metadata-refusal modes. Separate processes prevent a previous successful
lookup from hiding a first-resolution failure. Tests exercise the actual named
bindings and runtime-invoker channel over a controlled runtime; they do not
execute the game.

Independent source review found no remaining confirmed defect. It checked the
final eligibility and fallback flow, current/live source fences, exact scalar
guards, managed custody, layouts and missing invocation entries. Earlier
development runs remain separate; they do not qualify later corrections.

The complete native `ActOnViewer`/`TryPlannedSystemWarp` UI episode is
source-reviewed, not executed by the pure policy fixture. Replacing or closing
a live widget, confirmations, real nullable metadata, actual movement and
current-client/platform compatibility remain in-game qualification needs.
Clean candidate-bound regression/build results are a separate gate; the
preceding Haven results do not validate this cohort.

### Qualified source checkpoint

Product source: **`43d6c7b732be72b0d0ed225adb733249121360e2`**.

| Exact clean source lane | Result |
| --- | --- |
| Windows x64 Zig 0.16, Profiles host | 234/234 steps; 8/8 reported Zig tests; 93 artifacts |
| Windows x64 Zig 0.17, default off | 212/212 steps; 79 artifacts |
| WSL/Linux x64 Zig 0.16 | 195/195 steps; 8/8 reported Zig tests; 38 artifacts |

All three completed gates returned zero, retained unchanged source before and
after execution, and observed the successful binding marker plus all 20
isolated refusal markers. Both Windows lanes exercised the production uploader
against owned temporary loopback listeners with synthetic data. The host lane
also ran three inert Profiles controls. It did not run the account, catalog or
preferences lifecycle fixtures in the known-limited developer environment.
Negative build-option controls were not repeated in this cohort.

Independent readback checked **508 unique file facts with zero mismatches**:
source, tools, logs, artifacts, retained metadata, actual uploader executable
bytes, PE source/build identities, static CRT, 18 versus 16 exports, one TOML
provider and the expected Profiles dependency closure. Default off contains no
Profiles libraries. Static inspection did not load the product DLLs.

| DLL | SHA-256 |
| --- | --- |
| Host 0.16 | `3d98199ecfcbecf115b2137397e2aafd80e4a57d964c1bf34ae4e139e7420be2` |
| Default off 0.17 | `eb02b64b8e6f055ec3af04198366befd8b5088a80930a097ea871123e58e2718` |

Evidence root:
`D:/dev/stfc-workspace/.ax/ripper-warp-20261025/candidates/43d6c7b732be72b0d0ed225adb733249121360e2`.
Proof `independent-warp-readback.json`, SHA-256
`b45c2fd203e1ddf7984454190103b4360e0ad0193dba7f6e8c1bcbdc67b7611e`.

No deployment, game cycle, signing or macOS qualification occurred. Direct
collaboration and scoped continuity are not Runner-governed execution. This is
an offline source/build checkpoint, not full gameplay parity or promotion.

## Later in-game qualification

Attach any trial to its exact source, artifact, client and selected installation
and session. An approved cycle still requires a correctly targeted lifecycle
route; this cohort does not change deployment or profile admission.

- Select an ordinary reachable system destination, dismiss its popup and use
  the primary action. Confirm the selected fleet and destination remain correct
  and that normal native confirmations still work.
- Repeat with the visibility preference both enabled and disabled. Check visible
  destination, secondary-action, system-view and armada paths separately.
- Switch fleet or destination while planning, and try a stale or invalid path.
  Refused recovery must not invoke a hidden native fallback.
- Exercise special travel through its native popup flow. This helper must not
  supply a substitute ordinary course for recall, wormhole or toll travel.
- Confirm missing/drifted native observations refuse rather than manufacturing
  zero coordinates, false conditions or a default destination. Fixture failures
  can establish this boundary offline; actual live refusal and user feedback
  remain runtime qualification.

These checks are pending, not a claim of gameplay parity. Other remaining
gameplay cohorts, macOS and signing retain their separate qualification needs.

The [Haven routing limitation](M8-HAVEN-VISUALS.md#runtime-routing) still applies.
Further read-only inspection found useful Profiles/Bridge primitives but no
supported complete isolated-Zig cycle route. Keep source and artifact identities,
physical installation, requested profile and exact process generation distinct;
do not coerce the old Xmake/first-process route or reviewed-release authority.
The user has authorized cycling. Correct targeting and exclusion, including the
known Profiles execution-environment limitation, still need an appropriate
development adapter before this session can responsibly perform the trial.
