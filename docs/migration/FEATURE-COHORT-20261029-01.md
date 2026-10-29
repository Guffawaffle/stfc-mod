# Shop quantities, popups and artifact exchange

Source-preserving reassessment from `play-dev-ripper`
`8fb8802708612ff21217f79b0cde7da63466809e`. Frozen play-dev remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`; historical PLAY is
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`; inspected upstream dev is
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`. These are source-presence
observations, not claims about current remote heads or native behavior.

| Capability | Contract | Scope |
| --- | --- | --- |
| PLAY-DONATION | [Donation quantity](features/PLAY-DONATION.md) | Preserve tagged Windows setter behavior; retain the explicit narrower Mac boundary |
| PLAY-CHEST-LIMIT | [Chest quantity](features/PLAY-CHEST-LIMIT.md) | Native tagged context, upward-only extension,160 ceiling |
| PLAY-POPUPS | [Popup/reveal policy](features/PLAY-POPUPS.md) | Shared process allowance; native-first interstitial close; reveal skip |
| PLAY-ARTIFACT-EXCHANGE-ALL | [Artifact bulk button](features/PLAY-ARTIFACT-EXCHANGE-ALL.md) | Native bind first; hide only the dedicated button |

Complete frozen `parts/misc.cc`, `parts/artifact_exchange.cc`, supporting
InventoryForPopup/InventoryUseRowWidget/Widget/Interstitial bindings,
registration, configuration defaults/aliases and Settings consumers were read.
The current target keeps all these consumers in `features/shop.cc`, using
`game/inventory.h`, existing transactional groups and Settings/TOML owners.
No purchase, donation, reward claim or conversion is authorized by a fixture.

## Qualification boundaries

The old Mac donation implementation globally intercepted Slider.maxValue50/100.
The existing target intentionally confines adjustment to tagged donation rows;
it does not change unrelated sliders. This remains an explicit behavior
difference and a native Mac validation requirement, not silently declared parity.
Windows retains the source's exact nonpositive-cap behavior: do not replace the
initial native limit with the later default50 write. It does not fabricate an
unlimited numerical sentinel.

The chest hook is installed irrespective of the initial configured maximum.
Consequently its Setting should describe next-view binding rather than restart.
No persisted name, default, numeric domain or save owner needs to change.

`shop-limits-native` executes actual production consumers in the existing
controlled IL2CPP host. Its synthetic objects/callbacks do not establish rendered
slider ranges, native purchases, real popup scheduling or Mac hook fit. The
Windows-only claims fixture's dispatcher/build registration also needs to match
its existing Windows-only include; that test wiring is separate from gameplay.

Evidence directory:
`D:/dev/stfc-workspace/artifacts/shop-controls-20261029/`.
Settled-input regression, native build, exact artifact and player results are
recorded below as executed. Direct continuous development only; no Runner execution or
implementation fanout, upstream push, merge, release or overall parity claim.

## Corrections and completed local checks

The original target failed the new reentrant-context fixture: a managed maximum
getter replaced the row's Context, then the hook changed the abandoned context's
cap anyway. `fixture-before.log` records that actual production-path failure.
This is an inherited ownership defect, not a claim that ordinary chest purchases
were observed failing. The row/context now have operation-local GC custody,
checked observations and a current-context check before a write. No reference
into replaceable Config survives a managed callback. Native original still runs
once, including unavailable/context-replaced cases. No frame work/cache is added.

The shared popup flag again uses atomic access as in the source; that alone
does not authorize off-thread UI or Config access. The chest Settings metadata
now says reopen rather than restart. Keys, defaults, persisted values and save
mechanisms are unchanged. Artifact native binding and individual exchanges are
preserved without a production rewrite.

On the scoped non-Windows donation path, unavailable or zero owned amounts leave
native state untouched rather than establishing a positive cap from a default
zero. This is conservative availability handling, not restored global-slider
behavior. It is source-inspected but not executed by the Windows setter fixture.
Mac compilation, hook fit and native donation behavior remain unqualified here.
Mac-only owned-amount/unavailable-amount cases are prepared in the same actual
consumer fixture for a matching runner; they were not executed on Windows.

The final settled-input `tests-2.log` passed **419/419** Windows build/run/install steps with Debug
assertions, including `shop-limits-native`, the preceding claims/HUD fixtures,
Settings, presets, native lifecycle and AAI-host regressions. The shared limits
fixture is registered across targets; the older claims fixture is included and
dispatched only on Windows. No Mac execution follows from that wiring.

Fixture setup failures remain visible: `fixture-build-1.log` contains incorrect
test receiver casts; moving the shop include before missions also exposed a
dependent-name parse issue, so the existing include order was preserved.
`fixture-after.log` initially failed because its synthetic component omitted the
gameObject property metadata. The fixture was corrected to supply the actual
native property, without changing artifact production behavior or weakening the
hide assertion. All these differ from the reproduced production context defect.

## Exact native candidate

Signed implementation `a93e688c5db9acd23ec86d7a6e5641d70b39db02` passed
**41/41** native Windows build steps; verifier confirmed x64,16 proxy exports,
static CRT and the exact provenance marker. AX invocation
`ax-zig-20261029T055232-bf5eb72a` kept source clean/stable, built first, copied
and launched the main ordinary installation. The previous game session had
already closed; no running game was stopped during this cycle.

Built and installed DLL SHA256:
`0612568a5e66dfd89b707001f2e4eaf7f4e008cf5dc0ad8fc0fa17bd4eab59d9`.
Session PID1088, creation FILETIME `134377269329025803`, is a historical identity
after any later restart. All73 groups installed and FrameTick delivery became
ready. No startup error/critical lines were observed in the recorded snapshot.
The five tracked settings/state files stayed byte-identical. AAI `4bece2e5` and
UI Lab remained enabled and unchanged. No live frame-cost, allocation reduction,
native slider accuracy or macOS claim follows from these startup results.

Focused player trial is pending: open/cancel chest and alliance donation
dialogs, inspect their existing limits/controls, and inspect the artifact popup
if the hide setting is normally used. No purchase, donation or conversion is
needed. Popup allowance/reveal edge ordering has controlled callback evidence;
ordinary startup alone does not exercise all those paths.

`source-inputs.json` binds final tested inputs, fixture executable and419-step
log. `cycle.log`, `qualification.json`, `startup-summary.txt`,
`stores-before.json` and `stores-after.json` bind the native candidate. The
canonical deploy wrapper also reports its own checkout; the prepared-build
source, embedded marker and matching DLL hashes identify the worktree artifact.
No push, merge, release or complete-product parity is claimed.
