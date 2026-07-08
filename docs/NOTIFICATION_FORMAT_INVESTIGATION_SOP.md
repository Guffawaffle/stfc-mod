# Notification Format Investigation SOP

Purpose: make notification placeholder/rich-text regressions evidence-first. Use this when a Windows notification shows raw localization tokens such as `{1}`, `{5.000}`, or TMP/Unity rich text tags.

## 1. Preserve The Symptom

- Capture the notification screenshot or exact body text.
- Note the toast title, approximate local time, and action that produced it.
- Do not make another formatting guess until runtime evidence identifies the failing rung.

## 2. Check Static Evidence

Use AX research before editing:

```powershell
ax research-search "started an Armada" -Compact
ax research-search "HUD.Toast kind=class" -Compact
```

Record the localization template, expected placeholder indexes, and the static `Toast` field layout.

## 3. Add Bounded Runtime Evidence

Use temporary `[NotifyProbe]` log markers in `community_patch.log`, and keep them scoped to the active investigation.

Startup evidence should answer:

- Which `LanguageManager.Localize` overloads exist.
- Which overload was selected for `LocaleTextContext`.
- Whether an overload exists for `LocaleTextContext + object[]`.
- What fields the live `Digit.Prime.HUD.Toast` class exposes.

Leak evidence should answer:

- Toast state/title.
- Toast and `LocaleTextContext` runtime class names.
- Parameter source: toast field, toast property, LTC `_textParameters`, LTC `_identifierParameters`, LTC property, or none.
- Parameter count and bounded parameter value preview.
- Localized template before formatting and formatted body after stripping.

Keep leak probes budgeted; a handful of samples should be enough, and they should not ship in the final patch.

## 4. Interpret Before Patching

- If no parameter source is found, inspect live toast/LTC fields and properties. For `LocaleTextContext`, check both
  `_textParameters` and `_identifierParameters`; Armada toast templates have been observed with `_textParameters` null.
- If parameters exist but no 3-argument `Localize` overload exists, fix manual substitution.
- If the 3-argument overload exists but returns placeholders, verify parameter ordering and token syntax.
- If parameter values are placeholders themselves, inspect nested `LocaleTextContext` resolution.
- If tags remain but placeholders are resolved, isolate rich-text stripping.

## 5. Close The Loop

After the next smoke test:

- Copy the relevant `[NotifyProbe]` lines into the task notes.
- Remove probes that are no longer needed before final validation.
- Keep a pure test for every formatter rule learned from the evidence.
- Rebuild, deploy, cycle, and verify boot before asking for another live smoke test.
