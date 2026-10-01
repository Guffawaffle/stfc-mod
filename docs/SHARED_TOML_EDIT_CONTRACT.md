# Shared source-preserving TOML editing contract

The community mod owns this reusable engine. Bridge consumes a pinned source and
bundles an offline native adapter; neither the game nor a provider DLL must run
for editing. The component has no file, process, network or game-state access.
The mod runtime and Bridge use the same preparation engine. Host code continues
to own scheduling, conflict admission, backups and atomic file replacement.

## Meaning and source

Parse complete UTF-8 TOML with the pinned toml++ parser. Valid quoted, Unicode,
literal-dot, dotted, inline, multiline and array-of-table syntax elsewhere in a
document does not disable editing. Malformed input and duplicate definitions
are rejected without changed bytes. Keys are arrays of decoded literal segments;
a segment containing a dot is distinct from several dotted segments. Canonical
path strings quote non-bare segments unambiguously. Schema and Sync target-name
policies remain separate from TOML validity.

Read returns raw full source value spans, separately normalized semantic value
rendering, decoded path segments and safe line metadata. Do not silently replace
raw source spelling with normalized text. Containers and array elements retain
type and order; int64, floating-point, date/time and string semantics are not
flattened through JSON numbers.

Prepare supports setting/removing a value and removing/renaming a table subtree.
A supplied TOML value must parse as exactly one value, not injected assignments.
Paths cannot silently traverse arrays of tables without an explicit indexed
operation. Such a requested target can be unsupported while unrelated edits in
that same document work. Removal includes the whole selected assignment and its
inline comment; table operations include owned descendant declarations while
preserving unrelated interleaved sections. Empty implicit parents are pruned
when their last syntax disappears; explicitly declared empty tables remain.

Every candidate is reparsed and compared with the complete intended semantic
document. Failure produces no candidate bytes. Preserve every source byte outside
the selected value/assignment/declaration edits, including comments, ordering,
whitespace, UTF-8 BOM and line endings. Never normalize the whole document.
A document-validity check is distinct from whether one requested edit can be
proved. The engine must not retain the old conservative grammar as a fallback.

## Offline C ABI v1

`stfc_toml_abi_version()` returns 1. `stfc_toml_execute(request, request_length,
response_out, response_length_out)` accepts length-delimited UTF-8 JSON and returns
0 only when a response was allocated. `stfc_toml_free(response)` frees that buffer
in its allocating module. No exceptions cross the ABI; no borrowed storage survives
the call. The host checks ABI and exact pinned library bytes before use.

Requests contain `operation`, `text` and, where relevant, `path` (decoded string
array), `value` (one TOML value), and `destination` (decoded string array).
Operations are `validate`, `read`, `set`, `remove`, `remove_table`, `rename_table`,
`normalize_value`, `decode_string` and `parse_path`. Responses contain `ok`; failures contain a fixed safe
`error.code` and optional positive `error.line`, never source snippets. Successful
edits return `text`; reads return `overrides` entries `{path, canonicalPath, value, semanticValue,
line}` and `tables` entries `{path, canonicalPath, line}`. Normalization returns `value` in a
semantic spelling that existing typed consumers can read, without writing it to
the user's file. `decode_string` requires exactly one TOML string value and
returns its decoded Unicode contents as a JSON string in `value`. Normalized
TOML string spelling must not be treated as JSON string syntax. `parse_path`
takes a TOML dotted-key expression in `value`,
requires exactly one decoded target without injected statements, and returns
`path` and canonical rendered path in `value`. Read entries carry
`canonicalPath` rendered by the native TOML key encoder. Error codes distinguish `InvalidDocument`, `DuplicateTarget`,
`InvalidUtf8`, `InvalidPath`, `InvalidValue`, `UnsupportedTarget` and `InternalError`.

Bridge preserves its staged Save/Discard, immutable profile/runtime binding,
revision comparisons, verified backup receipts, mutation admission and atomic
promotion. A native prepare result grants no file-write authority. Read-only
browsing and help remain available when a particular write is blocked.

## Evidence and delivery

Build the minimal component independently of game dependencies, with immutable
parser/adapter dependency recipes and license attribution. Windows and both Mac
architectures remain build targets for the shared engine; Bridge's app is Windows.
Use synthetic fixtures for valid unusual syntax, semantic identity collisions,
source preservation, injection refusal and stale-file transactions. Never copy
account preferences into fixtures. Release pins, native byte hashes, package
allowlists, signing order and source/recipe SBOM subjects must match the actual
bundled adapter. Development overrides must be explicit. No player binary release
or live game qualification follows merely from these builds.
