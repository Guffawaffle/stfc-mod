# PLAY/SPUD development integration

This integrates reviewed PR #375's source `b50ae75686f37895ca234dc01b301ceb8ce5639e`
into existing local `play-dev`, previously
`e4be4bda7ea472af4369cd63e458d9e948a6c427`.
The PR targets PLAY but is not merged into PLAY by this task.

The merge retains existing AAI, Profiles, Kit workshop, performance work,
configuration/state ownership and intended removals. Conflicts in pan, Galaxy
labels and orbit are resolved by preserving the complete play-dev file with
only the SPUD include substitution. The removed game-error probe remains removed.
Bootstrap reporting preserves Profiles' existing abort behavior for requested
isolated launches; ordinary failures gain the same diagnostics as PLAY.

The play-dev-only claim capture now includes the same installation reporter.
AAI remains pinned at `4bece2e5f1afa7c7343f009e83398742a0f86114`.
Its hook-consuming `battle_log_analysis.cc` is explicitly configured with a
per-file forced include of the host installation adapter. This gives its two
existing hook sites the same permanent-hook diagnostics and indeterminate-error
stop policy without editing private source or advancing the pin. Other AAI
translation units, generic analytics, standalone builds and disabled-AAI builds
do not acquire that include. The file is named explicitly in the host embed
adapter so this integration boundary is visible rather than a global flag.

Native build uses the existing clean pinned checkout at
`D:/dev/stfc-mod/third_party/stfc-aai` through the explicit source option; this
is not the independently advanced `D:/dev/stfc-aai` development checkout.
AAI and UI Lab are enabled as in the previous daily development configuration.
Profiles' existing immutable source pin and all game TOML settings are unchanged.

Validation receipts and current source/binary identities are recorded locally in
`D:/dev/stfc-workspace/.ax/play-spud-review-20261026`.
Windows compile/startup, player AAI/Kit interaction and macOS qualification are
separate claims. A successful merge is not runtime qualification.
