# PLAY-AAI — Native Battle Analysis host integration

Contract v1, 2026-10-27. Source-preserving port, not an analytics or UI redesign.

| Input | Exact revision |
| --- | --- |
| Frozen play-dev host | `1baa55fedf1758b19f843cc59707b8901a0b4998` |
| Its immutable AAI dependency, including Kit workshop | `4bece2e5f1afa7c7343f009e83398742a0f86114` |
| Target `play-dev-ripper` before this batch | `f0ad3838865f006c14f3d9b1d0b97842ec3f8e58` |

The separate AAI development checkout has advanced to `f1421907`; that later
web/decay work is not silently substituted for the frozen product dependency.
Use the existing clean pinned checkout at `D:/dev/stfc-mod/third_party/stfc-aai`.
No source copy, new checkout, dependency pin advance or parallel analytics engine.

## Complete host workflow and obligations

The frozen host files are `xmake/dependencies/aai.lua`, `xmake/stfc-aai.pin.lua`,
`xmake/options.lua`, `mods/xmake.lua`,
`mods/src/patches/parts/battle_log_summary.cc`, `patches.cc`,
`screen_update_hook.{h,cc}`, `spud_install.{h,cc}`, config/defaults and the AAI
entry in `mods/src/settings/upstream_settings.cc`. Their consumers are the pinned
AAI `overlay.h`, `hook_installation.h`, native embed rule and implementation.

| ID | Preserved contract | Narrow adaptation / verification |
| --- | --- | --- |
| A1 | Explicit source identity; disabled builds do not inspect private source. Build selection and live use are distinct. | Extend existing Zig producer with explicit host selection, matching revision/dirty checks and source markers. Check both enabled and disabled builds. |
| A2 | `patches.battlelogsummaryhooks` is restart-only; `ui.battle_log_summary_preview` defaults true and is live. | Existing nested Config, preserving TOML writer and Mod Settings owner. Test default/read/reload/save availability. |
| A3 | AAI owns report `AboutToShow` and Back detours, each attempted once with independent status. | Preserve its metadata/signature validation and callbacks; route only its hook-consuming TU through the host HookSet policy. Record both targets centrally. |
| A4 | Indeterminate patch state stops startup, including when AAI's compatibility latch contains an exception. | Preserve process safety latch and host InstallGuard; test the contained-error boundary. Never report a swallowed unsafe result as an optional skip. |
| A5 | Exactly one host `ScreenManager.Update`; registration and actual frame readiness are distinct. | Use existing feature frame registration, with independent hook capability rows. Retain report-only bounded discovery fallback. |
| A6 | Native Overview, participant selection, source capture, prepared worker reuse, on-demand Crew & Effects and Tactical Console retain behavior. | Compile the immutable native implementation. Run its existing pure/worker/evidence fixtures, retained replay comparison and an exact-client UI trial. |
| A7 | Managed acquisition/UI/localization stay on the game thread; worker inputs remain owned; close/cancel/source replacement reject stale publication. | Preserve existing runtime and worker implementations and their tests; no rewrite into convenience getters that collapse unavailable into zero. |
| A8 | Native Back closes AAI while retaining the report; next Back resumes native navigation. Missing observer retains bounded fallback; hook failures remain visible. | Preserve AAI callback logic; qualify degraded installation independently from frame service availability. Actual Back/fallback behavior requires native testing. |
| A9 | Ship/officer/buff art and names, exact observations, coverage, resource/capacity ownership and native controls stay intact. | Preserve all native embed files and dependencies. Offline results do not qualify artwork or visual behavior. |
| A10 | Optional UI Lab/Kit workshop retains explicit development gating. No automatic probes in ordinary builds. | Separate host and UI-Lab build choices; preserve original `STFC_AAI_UI_LAB` / `STFC_AAI_LAB_PROBES` pairing. |

## Implementation boundary

AAI's checked raw-runtime adapter is already independently qualified. It is not
replaced by the host's default-return typed getters. This port uses the new
transactional hook owner, existing frame services, nested config and Settings
adapters at the host boundary. The analytical engine, decoder, native rendering,
worker and public AAI interface remain unchanged at the dependency pin.

The two AAI installation call sites need their actual SPUD trampoline after a
successful transaction. A narrowly retained staged-handle query can provide it
without bypassing HookSet, inventing an installation token, or copying detour
ownership. Existing clients retain their ordinary `Stage` interface. Safe refusal
and indeterminate custody remain unchanged and require regression tests.

## Qualification plan and current status

The host integration is implemented and qualified offline. Native deployment
and player interaction are not established by the offline results below.

- Exercise the actual host adapter with complete, degraded and indeterminate
  installation reports; feature callback delivery remains separately qualified.
- Run the pinned AAI fixture suite and existing host regression fixtures.
- Build the complete Windows DLL with source identity and the explicit pin; check
  disabled builds still avoid private-source inspection.
- Compare replay/query results for retained 1v1, Krenim and additional available
  reports without changing their capture bytes or fixture export semantics.
- Native trial: report entry, immediate Overview, ship switching, on-demand Crew
  & Effects, Tactical Console, Back/close/reopen, live toggle and workshop where
  explicitly enabled. Preserve the player's TOML and stores.
- macOS is implemented as shared code; actual CI build and Mcohall runtime checks
  remain separate. No local macOS build or gameplay claim.

No SQLite, Python/MCP, new analytics, UI redesign, general persistence work,
progressive loading or Kit expansion is part of this migration.

## Implemented host boundary and offline evidence

- [`features/aai.cc`](../../../src/mod/features/aai.cc) preserves the enable callback
  and pinned `InstallHooks`/`Update` calls. Central feature rows independently
  report `AAIReportObserver`, `AAIBack` and the `BattleLogSummary` frame service.
  Safe missing detours preserve AAI's bounded fallback; indeterminate state still
  stops startup after AAI's own exception containment.
- [`aai_host_detour.h`](../../../src/mod/aai_host_detour.h) is forced into only
  `battle_log_analysis.cc`. Its two existing call sites use host `HookSet`
  transactions and platform entry checks. `StageRetained` permits querying the
  real trampoline after commit while retaining the existing rollback/quarantine
  ownership. Existing `Stage` callers are unchanged.
- [`aai_native_platform.h`](../../../src/mod/aai_native_platform.h) restores the
  old host's Windows compilation prerequisites for that one native consumer:
  Windows declarations before the optional lab's `psapi.h`, and its CRT warning
  policy. The first UI-Lab-enabled full DLL build exposed these implicit
  dependencies; initial build failures were not deployed. No private AAI source
  change, field-layout change or analytical fallback was used to fix compilation.
- [`zig/aai.zig`](../../../zig/aai.zig) keeps producer-only targets and adds
  `-Daai-host=true -Daai-source=<explicit clean pinned checkout>` for actual
  linkage. `-Daai-ui-lab=true` preserves the optional workshop/probes; it remains
  separate from ordinary AAI use. Thin Windows/macOS targets share the same
  native sources; universal children forward the selected AAI inputs.
- Original TOML keys/defaults and existing preserving writer are retained. The
  restored Settings control requires actual frame readiness, not registration
  alone. Its position/help follow the frozen Interface & Scale page. A disabled
  live value still reaches AAI's own update/teardown path.

Tests and measurements are separate claims:

| Check | Result / scope |
| --- | --- |
| Combined pinned AAI, host and foundation suite | **319/319 build/run steps**; all **42** unchanged AAI fixtures, host catalog/persistence, existing presets and foundation tests. |
| Final host containment delta | **10/10 steps** after strengthening the fixture: an actual pinned AAI `Attempt` contains a synthetic unsafe installation exception, and the production host adapter still rejects startup. |
| Final Settings source-order delta | **150/150 test-mod steps**, with AAI disabled and a nonexistent private root; live control/writer availability checks still pass. |
| Real detour fixture | The actual shim, HookSet and SPUD patch a synthetic function in the test process; callback/original execute once, the returned pointer is the real trampoline, repeated install/refusal stays latched, and destruction retains permanent custody. No game signatures or gameplay claimed. |
| Retained reports | **96** whole-JSON/exit-status comparisons against the independently retained M1b pinned tools, across 1v1, seven-round Krenim, 11-round victory, nine-round defeat, critical-prevention and decay reports. Summary, snapshots, ledger, every participant's crew/timeline and Full Fight/Round/Round Start/Subround queries match. Source hashes unchanged. |
| Private source gates | Six cases: disabled/missing private root, contradictory host selection, relative root, wrong development revision, exact pin and producer-only selection. Correct refusal/success; no private probe on disabled path. |
| Complete Windows native build | **41/41 steps**, UI Lab enabled; x86_64, all 16 proxy exports, static CRT and full embedded identity verified. Pinned source and host inputs stable across build. |
| Complete disabled Windows build | **37/37 steps** with `-Daai=false` and a deliberately nonexistent private root. The private native implementation marker is absent from that DLL. |
| macOS | Shared implementation and forwarded build inputs; **no local build or native qualification**. CI/Mcohall remain required evidence. |

Current pre-deployment Windows artifact SHA-256:
`c31a1c9104eeb5591d0520b368efa0838b5c5335f51c541d04b45b88a50d5dd5`.
It is bound to worktree content identity
`7e9ddbdb30ad81b6f2e93c78184a50d9a0c70d162b7f7ebc05888ae02ddb1821`
over base `f0ad3838`; it is not described as a clean-commit build. A later native
trial must record its own exact artifact and source identity.

Evidence stays machine-local, outside Git:

- `D:/dev/stfc-workspace/.ax/aai-migration-tests-20261027-v3.log`.
- `D:/dev/stfc-workspace/.ax/aai-migration-host-final-20261027.log`.
- `D:/dev/stfc-workspace/.ax/aai-migration-build-20261027-v3.json`.
- `D:/dev/stfc-workspace/artifacts/aai-migration-20261027/replay-parity/receipt.json`
  (SHA-256 `b83d0815f346b9d027e089566742bb3654835b579775483247ef8abd6b720c6a`).
- `D:/dev/stfc-workspace/artifacts/aai-migration-20261027/source-gates.json`.

The existing ignored AX Zig binding explicitly selects this AAI pin and UI Lab
for daily cycles. It checks the clean private checkout before/after building and
records its revision/options. Normal Xmake routing is unchanged. Original adapter
files and the completed route are recoverable under
`D:/dev/stfc-workspace/artifacts/aai-migration-20261027/ax-aai/`.
Compiler stdout/stderr are retained even on failed builds; no game downtime was
needed to discover the initial compile-environment gaps.

The frozen fork defaulted its Xmake AAI option on. The foundation's general Zig
build retains its existing private-source-free default; the daily AX Zig binding
now selects AAI explicitly, so ordinary cycles here keep it included. A direct
Zig or CI build must supply the documented host/source flags. A macOS CI build
without that selection is not evidence that AAI compiled. Private-source
provisioning and both Mac architectures still need that future qualification;
no CI/release infrastructure was changed in this batch.
