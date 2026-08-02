# Windows DLL identity contract

The Windows proxy DLL publishes descriptive build provenance through its PE
version resource. Consumers can inspect the resource without loading or
executing the DLL.

The version resource contains individual `STFC*` fields and a compact
`Comments` value with this form:

```text
stfc-identity-v1;distribution=<id>;source=<source-state>;base=<commit>;build=<invocation>;mode=<mode>;channel=<channel>
```

Values are restricted to printable identifier characters and never contain
usernames, workstation paths, branch names, changed-file lists, tokens, or
source bodies. Schema 1 defines:

- `STFCDistributionId`: the self-declared distribution lineage.
- `STFCSourceStateId`: `git:<commit>` for clean source,
  `dirty-sha256:<fingerprint>` for AX dirty-source builds, or `unknown` when a
  direct build does not provide provenance.
- `STFCBaseCommit`: the full source commit, or `unknown`.
- `STFCBuildInvocationId`: correlation ID for the build/AX cycle.
- `STFCBuildMode`: XMake mode.
- `STFCBuildChannel`: `release`, `ci`, or `local`.

The marker is descriptive and is not an authenticity proof. Official release
classification still requires a recognized SHA-256 and the expected valid
Authenticode signature. Unknown or locally built DLLs remain valid custom
installations and must not be replaced without an explicit user update action.

AX generates one build invocation ID per build/deploy/cycle and verifies that
the built (and, for deploy/cycle, deployed) DLL reports the same identity. At
runtime the mod logs that embedded build ID and a separate random runtime
launch ID. The former identifies the binary/cycle; the latter distinguishes
individual game-process launches.

The Windows AX source mirror omits the clean, commit-verified macOS-only
`macos-launcher/deps/PLzmaSDK` submodule because its test corpus contains
Unicode-normalization-equivalent names. The omission is explicit in
`sourceProvenance.excludedSubmodules`; a dirty or mismatched submodule still
fails closed before exclusion.
