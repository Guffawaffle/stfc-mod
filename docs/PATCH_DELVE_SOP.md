# Patch Delta Delve SOP

Purpose: turn game-update curiosity into a repeatable private workflow: preserve the patch timeline, identify interesting changes from static evidence, and only then design one safe runtime probe at a time.

This document does not authorize a hook or probe by itself. It defines the path from dump evidence to an approved, documented, default-off experiment.

## 1. Preserve The Patch Interval

After a game update, refresh through AX so the old research corpus is compared before it is advanced:

```powershell
ax dump-refresh -ResearchPatchLabel arcfall
```

Default behavior:

- Rebuild the dump artifacts and SQLite dump index.
- Compare the previous private research corpus against the new dump.
- Write a timestamped report under `.ax-priv/cache/research-diffs/`.
- Append `diff-recorded` to `.ax-priv/cache/research-timeline.jsonl`.
- Only then advance `.ax-priv/cache/research-corpus.db`.
- Append `baseline-advanced` after the baseline refresh.

If the diff report is produced but the timeline write fails, `dump-refresh` must refuse to advance the baseline. That keeps the next patch from erasing the current comparison point.

Use this for the timeline view:

```powershell
ax research-timeline -SummaryOnly -Compact
```

## 2. Triage Static Leads

Start at R0 static review. Do not add runtime code while the question is still "what changed?"

Use a short evidence pack:

- `ax research-timeline -SummaryOnly -Compact`
- `ax research-diff -SummaryOnly -Compact` when the baseline has not yet advanced
- `ax dump-pack <ClassName> -Compact -TokenBudget 1200`
- `ax dump-symbol -Query <term> -Like -Card -Compact`
- `ax dump-string -Query <term> -Like -MaxResults 30`
- `ax dump-script -Query <term> -Like -Compact`

For each candidate, write down:

- What changed: symbol, method, literal, script, or feature config.
- Why it matters: user-visible bug, mod risk, observability gap, or curiosity.
- What is still unknown.
- The lowest risk rung that could answer the next question.

## 3. Human Pick And Probe Question

The human can point at a lead, but the agent owns narrowing it.

Good probe question:

- "Can this exact method be reached when the user opens the Arcfall panel?"
- "Does this one event fire when the player presses the claim button?"
- "Does this one state field change after the takeover UI refreshes?"

Bad probe question:

- "Find out how this whole feature works."
- "Hook the family and see what happens."
- "Log every payload."

## 4. Document Before Code

Before any new runtime hook or probe, create a probe directory entry from `docs/probes/TEMPLATE.md`.

Required minimum:

- Static evidence and exact target.
- Risk class and confidence rung.
- One question.
- Expected log tag or event kind.
- Default-off flag or compile-time guard.
- Crash-safe disable path.
- Human smoke test steps.
- Exit plan: promote, revise, or delete.

For native seams, also add or update `docs/NATIVE_SEAM_LEDGER.md` before runtime work begins.

## 5. Implementation Rules

All new native hooks/probes use the single-owner registry path from the start:

- Define one `HookDescriptor`.
- Install through `HookModuleHealth`.
- Use `HOOK_REGISTRY_SPUD_STATIC_DETOUR`, not raw `SPUD_STATIC_DETOUR`.
- One hook target per probe unless the directory entry explicitly justifies a second target.
- Fail fast on missing class, missing method, duplicate owner, or unexpected signature.
- Default off.
- Log-only unless the approved question is about behavior suppression.
- Do not dereference payloads until the probe entry records payload confidence.
- Do not generalize one member of a generated family to siblings.

Probe code should be removable by deleting one module or one install entry.

## 6. Smoke Test Directive

Each probe entry must include a short human directive:

```text
Goal:
Steps:
Expected log marker/event:
Stop immediately if:
Report back:
```

Prefer this runtime loop:

```powershell
ax preflight
ax deploy -BuildMode releasedbg
ax cycle -BuildMode releasedbg
ax mark -Label "arcfall-probe-start"
ax observe -DurationSec 30 -LogProfile dirty -Mark -Label "arcfall-probe-window"
```

Tailor the commands to the risk class. Do not cycle the game for static review. Do not ask the human to perform multiple unrelated actions in one probe window.

## 7. Close The Loop

After the smoke test:

- Update the probe directory entry with result and evidence.
- Update the seam ledger for native work.
- If it crashed, remove or disable the probe before continuing.
- If it answered the question, either delete it or promote it through a separate implementation plan.
- If it did not answer the question, revise the question before adding another hook.

The default outcome for discovery probes is deletion, not productization.
