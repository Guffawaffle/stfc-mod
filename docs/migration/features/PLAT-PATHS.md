# PLAT-PATHS — ordinary, custom and admitted file namespaces

Contract v1, 2026-11-02. Existing `play-dev-ripper`, clean starting head
`f65fa9af1219ea1e5e9adbfc42202d93bafdb9f9`; frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Scope is current mod file routing,
logging and its persistence consumers. AAI and new upstream work remain deferred.

| Inspected retained local ref | Exact revision | Presence |
| --- | --- | --- |
| upstream/dev | e884efa7c6ff66d0776b4cd4d6b76ce193296be5 | Original `mods/src/file.{h,cc}` and config/startup consumers. |
| origin/play | cad98ef7990dd66167eb64ea5dda525c1fe2c296 | Existing ordinary/custom/profile namespace domain. |
| origin/play-dev and frozen/play-dev-20261026 | 1baa55fedf1758b19f843cc59707b8901a0b4998 | Selected File owner, config/state/logging and profile-launch lifecycle. |
| refactor base | f65fa9af1219ea1e5e9adbfc42202d93bafdb9f9 | `src/mod/file.{h,cc}`, arguments/file_macos/profile_launch/bootstrap and config/state/writer consumers. |

These are exact local observations, not fresh remote-head claims.

## Workflow and obligations

Admission parses an explicit immutable profile selector before configuration.
The existing host prepares named storage before File::Init; unavailable or
unsupported named context is refused, not guessed from account/preferences or
converted to ordinary routing. File::Init then chooses admitted paths, otherwise
the custom extension family, otherwise ordinary names. Settings, runtime writer,
state, log and local-media resolution use those selected paths.

| ID | Frozen evidence / retained policy | Typed target mapping and verification |
| --- | --- | --- |
| P1 | `file.h` defaults and `file.cc:195–243`: ordinary config/runtime vars/log/legacy battle-cache names. Windows working directory at115–118. | Same constants and Directory/Path owner. Production File routing fixture; the legacy attempted-ID filename remains an inert compatibility surface, not renewed delivery ACK persistence. |
| P2 | `file.cc:95–112`, folder_manager.mm: ordinary Mac preferences directory. `config.cc:830–875`, startup patches.cc100–104: legacy move/symlink/note before log directory creation. | Existing Foundation-owned path copy and log-first migration retained. Existing synthetic private-Library migration fixture; native Foundation/Mac startup remain distinct. |
| P3 | `file.cc:140–191,205–243`: first complete -ccm occurrence selects `.toml/.vars/.log/.json` names, including an empty first operand selecting ordinary names. | Restore first selection while consuming later operands and retaining later debug/trace scanning. Repeated/empty operand fixtures independently fail before correction. Relative/absolute/Unicode paths retain stronger target conversion. |
| P4 | Frozen admitted path override at file.cc246–259; accepted M3/M4 immutable host layout. | Absolute admitted config.toml/runtime.toml/logs/community-mod.log/logs/battles.log and derived config.state.json override -ccm. Supplied-context tests verify empty/relative refusal and all unactivated namespace fields. Actual catalog/session/preference hooks are not exercised here. |
| P5 | Frozen mod_state.cc35–41/config consumers preserve selected namespace. | Config/read/reload/save, RuntimeConfigWriter, state and relative media use File::Path(File::Config()). Custom state selection uses HasCustomNames, not an inferred account or nonempty display label. Retain existing state/writer behavior and tests. |
| P6 | Frozen config.cc878–884,1630–1635 removes obsolete vars/parsed output. | Keep current cleanup confined to the selected directory. Do not recreate frozen cross-route working-directory deletion. Synthetic custom/named cleanup checks qualify owned legacy removal and unrelated bytes. |
| P7 | Frozen debug/trace startup options and human-readable custom stem. | Owned UTF-8 arguments, trace precedence, empty display stem and custom routing remain separate. Window title consumes the label only; no account identity is inferred. Native argument acquisition/title behavior remain separate from supplied-argument fixtures. |

## Preserved improvements and source limitations

Keep UTF-8 filesystem construction on Windows, corrected relative Mac custom
paths, flags after -ccm and terminal debug/trace flags, explicit custom-routing
identity independent of an empty stem, and selected-directory legacy cleanup.
These avoid source defects; restoring first -ccm selection does not reintroduce
the frozen early loop break that ignored later flags.

The profile selector parser has a retained source ambiguity: a -ccm filename
operand literally spelled `-stfc-profile` can also be seen as a profile selector.
Frozen profile_launch did the same. This is not newly classified as a migration
regression or silently changed to ordinary fallback. Reserved-selector policy
needs its own correction decision; a qualified filename avoids the literal-token
collision. Current malformed/duplicate explicit selectors continue to fail closed.

## Evidence and acceptance

The [cohort](../FEATURE-COHORT-20261102-PATH-PERSISTENCE.md) binds current input
hashes, before/final fixtures and checkpoints. Existing profile bootstrap/hook
inventory tests qualify sequencing and supplied metadata, not current-client
isolation or real profile-store equivalence. This work neither accesses real
catalog/account preferences nor changes a live profile/configuration. Source,
controlled filesystem, actual native startup and platform acceptance stay distinct.
