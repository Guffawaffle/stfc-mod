# Default-on AAI and the native settings control

2026-10-25. Downstream `play-dev`, implementation
`c91d8771eb7b63299ca29520085cad2f515d5a87`.
AAI immutable pin and existing submodule advance together to
`4bece2e5f1afa7c7343f009e83398742a0f86114` (UI implementation `995911e`).

## Behavior

- `stfc_aai` defaults to true in this branch's build options. Explicit
  `--stfc_aai=n` still avoids inspecting/loading the private source dependency.
- `[ui] battle_log_summary_preview` now defaults to true. Its legacy name is
  retained so existing TOML remains compatible. Explicit saved false is preserved.
- Mod Settings -> Interface & Scale -> Battle Analysis (AAI) supplies the live
  persisted Boolean control. Defaults restores on. Disabling clears active AAI
  state on the next update; enabling requires reopening the battle report.
- The control is unavailable in a build without AAI or with its startup hook
  preference disabled. Hook changes themselves remain restart-only TOML settings.
- UI Lab remains default-off as a separate repository build option. This local
  development build enables it to qualify the component workshop. No ordinary
  default automatically requests Crew & Effects, capture fixtures or probes.
- AAI's dispatcher/feature/liveness checks remain outside battle reports. A
  failed observer can use bounded fallback discovery. This is not zero work or
  a new performance measurement; analytical processing remains report-driven.

The pinned dependency was previously `07b7631`, whose public interface did not
have the host's current InstallHooks/result contract. Simply changing the build
Boolean would have exposed that mismatch. The existing submodule received the
qualified local AAI commit without a new clone/worktree; both gitlink and pin
now match. **Publish AAI 4bece2e before any later push of this mod checkpoint.**
It is not yet in AAI's recorded origin/main; no push was authorized or performed.

## Validation

- Full local `tests/run-settings.ps1` suite passed: generic settings, actual
  102-key catalog with AAI absent, persistence/reload, permissions and shutdown.
- The actual 102-key catalog also passed separately with STFC_AAI_ENABLED.
  Tests cover the existing TOML key, disable/save, explicit off preservation,
  Defaults restoring on and unavailable control rejecting writes.
- Existing asynchronous loading-image validation required a fixture-only,
  bounded wait for Pending before continuing catalog tests. No loader or sync
  production behavior changed.
- Python settings inventory audit: 4/6 pass; the remaining pre-existing gaps are
  `sync.diagnostics` and `sync.targets.<target>.diagnostics`. The two AAI inventory
  omissions are fixed. Unrelated sync entries/behavior remain untouched.
- AAI opt-out configured successfully with an intentionally nonexistent source
  path. Final configuration uses the pinned source, no development override and
  no dirty-source permission: AAI=true, source="", allow_dirty=false, UI Lab=true.
- Full Windows x64 releasedbg AX build passed in 125.641 seconds from clean
  `c91d8771`, using pinned AAI 4bece2e. PE is valid x64 DLL, 17,284,608 bytes,
  SHA-256 `a484c719744b08f3d7813660f4f9d5897343ee6dd9946e6a6b4d9c251050b896`.
  Byte inspection confirms the exact AAI revision, pinned state, workshop and
  Battle Analysis settings label. An AAI-disabled host build is not substituted
  for this qualification.

No live game TOML/environment was edited; only local Xmake build configuration
was changed. No deployment, process cycle, runtime UI trial or push occurred.
Shared code supports Windows/macOS; no macOS build or new runtime hook-fit claim.

## Recovery and next trial

Private native receipt/artifact: `D:/dev/stfc-workspace/.ax/aai-defaults-20261025/`.
Settings receipts:
`D:/dev/stfc-workspace/.ax/aai-default-on-20261025/settings-audit/`:
`off-receipt.json` SHA-256
`1b4bb45f99416b18721eac2ef965efc00804baa1f6c40de9121eb25f984ac71f`,
and `on/receipt.json` SHA-256
`48de4e3b33df893efd884f728fef3571fbc2716ea232d6861c5ed2b51a07bca7`.

After explicit deployment/cycle permission, use the owning AX lifecycle and
explicit runtime target observation. Recheck source/config/artifact identity.
Verify setting visibility, off/save, on/reopen-report and default reset, then
Group padding, independent corners, full-height bar ticks and contrasting inside
labels in UI Lab. Existing saved false is intentionally not overwritten; use
the in-game control to change it. Preserve Overview/Crew on-demand behavior.
