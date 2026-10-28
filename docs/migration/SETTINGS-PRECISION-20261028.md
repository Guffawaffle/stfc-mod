# Settings precision correction — 2026-10-28

## Report and source comparison

Guff's screenshots on deployed `d61b383d` show Edit readouts `0.519999981`
and `0.25999999`, plus a transient slider label `0.3592861` while dragging
before the normal refresh settles it. These are two presentation regressions,
not evidence that persistence changed a requested value.

Frozen source `1baa55fedf1758b19f843cc59707b8901a0b4998` already handles both:

- `mods/src/settings/upstream_settings.cc::Format<T>` uses shortest decimal
  text and suppresses residue near a nonzero two-decimal value within four
  relative float epsilons. Meaningful custom precision/tiny values survive.
- `mods/src/settings/native/value_widgets.cc::SliderValueLabelHook` intercepts
  the native label listener separately from the change listener. It renders
  the applied snapshot instead of the listener's unsnapped drag argument.

The migration used `max_digits10` for every numeric readout and omitted the
separate label hook. Its periodic refresh explains why the drag label later
settled. No new slider model, writer or UI framework is needed.

## Narrow adaptation

[config_settings.cc](../../src/mod/settings/config_settings.cc) restores the
source readout/default policy. Exact-entry initial text uses shortest round-trip
formatting without approximate rounding. An unchanged submission still supplies
the original snapshot value. Integer digits, tiny values and very large values
remain inspectable. The existing explicit-save/retry behavior on an unchanged
submission is preserved.

[settings_values.cc](../../src/mod/features/settings_values.cc) stages the
typed `UpdateValueLabel(float)` entry in the existing `NativeSettingsValues`
transaction. The adapter passes `displayNumber()` only to the original label
renderer, once. It does not change the native slider, model or persistence.
Thread, page, view, context, callback ownership and managed custody are checked;
identity is rechecked after potentially reentrant native observations. Unknown,
released, replaced or foreign rows take native label behavior.

No changes to steps, ranges, serialization, save ownership, settings defaults,
configuration files, gameplay, AAI or Profiles isolation are part of this fix.

## Hook evidence and limitations

Exact Windows client273 `GameAssembly.dll` SHA256:
`a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
Matching retained `dump.cs`, `script.json` and provenance establish private
instance `Void(Single)`, receiver/float/trailing MethodInfo ABI (`vifi`).
`SliderOptionWidget.UpdateValueLabel` RVA `0x1034780`, file offset `0x1033580`;
PE `.pdata` extent `[0x1034780,0x103494a)` is 458 bytes. Complete initial
instructions reach 20 bytes without a forwarding jump. Only one script name
uses this entry; it does not alias the existing Settings hook targets. These
are static fit findings, separate from successful SPUD installation.

macOS uses the shared typed adapter and existing `CheckedEntry` runtime guard.
Current Mac bytes/build/runtime qualification are unavailable in this Windows
pass; earlier CI does not qualify this changed candidate.

## Verification

- [config_settings_test.cc](../../tests/config_settings_test.cc) exercises the
  actual catalog's clean `0.52`/`0.26` readouts, meaningful precision, zero,
  tiny/maximum floats, exact integer digits, round-trip modal text, unchanged
  submission and unchanged float serialization.
- [settings_values_consumer_cases.h](../../tests/settings_values_consumer_cases.h)
  invokes the actual change and label callbacks over controlled native metadata:
  raw drag → applied readback → late native label, original-call count, no label
  writes, stale/foreign/unknown/context/page/root failure and reentrant lookup.
  It tests post-bind consumers, not Unity rendering or detour execution.
- The first run caught an incorrect new test expectation that unchanged modal
  submission performs no save. Existing `ValueSetting::SetFromUser` deliberately
  persists unchanged explicit requests, including retry. The assertion was
  corrected; that production contract was not altered.

Final candidate, passing log, source manifest, executable hashes and runtime
trial status are recorded in the scoped recovery and qualification receipt at
`D:/dev/stfc-workspace/artifacts/settings-precision-20261028/`.

Focused in-game check: open the affected Settings page; drag through several
values, verify the Edit readout and slider stay clean during/after dragging;
open Edit on a custom-precision value and cancel; leave/reopen the page. Exact
modal values may legitimately retain precision that a concise readout omits.
