# Native Probe Safety

Purpose: define the first practical safety model for native probe work in the STFC community mod. This is a documentation artifact only. It does not add hooks, probes, runtime behavior, feature flags, product code, deployment steps, or game-affecting commands.

Operational patch-delta workflow lives in [Patch Delta Delve SOP](PATCH_DELVE_SOP.md). New exploratory probes and hooks start with a file in [Probe Directory](probes/README.md) before runtime code is added.

The immediate trigger is the recent `Shift+Space` / `ShortcutsManager.OnShipLocateAction` crash RCA. The broader goal is to make future exploration start from static evidence and climb toward runtime confidence one narrow seam at a time.

Living seam-specific evidence belongs in [Native Seam Ledger](NATIVE_SEAM_LEDGER.md). Keep this file focused on policy; keep the ledger focused on per-seam confidence.

## Background

Native probe work is different from normal product work in this repository. The mod runs inside a Unity/IL2CPP game process, and a symbol that looks ordinary in generated headers or an IL2CPP dump can still be unstable as a native hook seam. A probe can crash the game even when it only logs, even when payloads are treated as opaque, and even when the product-level idea is sound.

The default model is:

- Static before runtime.
- Read-only before write.
- Passive observe before active intercept.
- Log-only before behavior change.
- Known managed API before native pointer seam.
- Single hook before hook family.
- One callback or action at a time.
- Feature flag off by default.
- Crash-safe disable path always exists.
- Never install a broad generated guard list without per-callback confidence.
- Do not call original/trampoline or dereference callback payloads unless the seam is proven safe.
- Prefer "can we detect this event?" before "can we alter this event?"
- Separate probe code from product code.
- Every probe has an exit plan: promote, revise, or delete.

## Shift+Space RCA Summary

Observed facts from the recent hotkey triage:

- Plain `Space` worked.
- `Shift+Space` crashed.
- Live config had the mod secondary action bound to `TAB`, not `Shift+Space`, so `Shift+Space` should not have gone through the mod-owned secondary action path.
- Removing logging did not stop the crash.
- Treating the callback payload as opaque did not stop the crash.
- Removing `OnShipLocateAction` from the generated callback-guard list in `mods/src/patches/parts/hotkeys.cc` stopped the crash.

Defensible explanation: `Shift+Space` reached the native Scopely locate callback path, and the mod's detour/guard for that callback was unstable. The crash was tied to the native callback guard seam, not to log formatting and not to payload interpretation alone.

Standing lesson: broad generated native callback guards without per-callback confidence are not safe exploratory development.

## Safe-Probe Ladder

Escalate one rung at a time. Do not skip from dump evidence directly to broad behavior interception.

| Rung | Question | Acceptable evidence | Stop condition |
| --- | --- | --- | --- |
| 1. Static | Does the symbol or relationship exist? | Source, config, generated headers, IL2CPP dump, docs | Stop before runtime if the source of truth is missing or ambiguous. |
| 2. Passive runtime | Can existing behavior be observed without new hooks? | Existing logs, existing hook health, existing AX read-only queries, existing sidecar events | Stop if the event is not visible passively and the next step needs approval. |
| 3. Managed log-only | Can a known safe managed/API seam report the event? | A single known-safe managed surface, no mutation, no native pointer payloads | Stop if it requires native callback payloads or changes behavior. |
| 4. Native canary | Can one native seam be reached safely? | One seam, entry/exit only, no payload assumptions, explicit rollback path | Stop on crash, missing entry/exit confidence, or any need for a family install. |
| 5. Native interpretation | Can payload or trampoline behavior be trusted? | ABI evidence, per-callback runtime evidence, state correlation, repeatable rollback | Stop if original/trampoline or payload reads are not individually proven. |
| 6. Product behavior | Can the mod safely rely on this seam? | Correlated state, understood payload, tests where possible, off-by-default rollout where appropriate | Stop if confidence depends on generated membership or unvalidated siblings. |

The confidence ladder for any specific seam is:

`symbol exists -> static relationship -> runtime observed -> state-correlated -> payload understood -> product-safe`

Being on one rung for one callback does not transfer confidence to another callback with a similar signature.

## Risk Taxonomy

| Class | Name | Definition | Default approval posture |
| --- | --- | --- | --- |
| R0 | Static | Docs, code, config, dump inspection only. No game launch, no runtime writes, no hooks. | Safe for normal documentation and source review. |
| R1 | Passive runtime | Observe existing behavior only. No new hooks, no new writes, no deployment, no action injection. | Needs explicit scope; avoid during documentation-only slices. |
| R2 | Managed log-only | Known safe managed/API surface, log-only, no mutation, no native pointer seam. | Requires a focused branch and a disable path. |
| R3 | Native canary | One native seam, entry/exit only, no payload assumptions. | Requires explicit approval, default-off flag, and rollback instructions. |
| R4 | Native interpretation | Payload reads and/or original/trampoline confidence work. | Requires per-seam ledger evidence; no family assumptions. |
| R5 | Behavioral or broad | Alters behavior, suppresses native behavior, installs generated hook families, or exposes public/product behavior. | Highest risk. Do not use for discovery. Promote only after lower rungs are complete. |

Broad generated guard lists are R5 even if each generated hook body looks small.

## IL2CPP Dump Usage Policy

The IL2CPP dump is a map, not ground truth.

Static symbols prove that something exists in the binary. They do not prove that it is live, safe, stable, intended, reachable, ABI-safe, or semantically reliable.

The dump can support:

- Symbol existence.
- Static class, method, field, and signature relationships.
- Candidate ownership and namespace mapping.
- A hypothesis for wrapper shape or payload layout.

The dump cannot by itself support:

- Calling a method.
- Installing a detour.
- Trusting a trampoline.
- Dereferencing callback payloads.
- Suppressing native behavior.
- Claiming product semantics.
- Generalizing one callback's safety to a generated family.

Every dump-derived claim should be labeled as static evidence until runtime evidence moves it up the confidence ladder.

## Pre-Run Checklist

Use this before any future probe that goes beyond R0 static review.

- The exact repo/worktree is named, and `git status --short --branch` is checked.
- The task owner explicitly approved the risk class for the run.
- The probe answers one question, not a bundle of related questions.
- The lower-risk rung has been attempted or explicitly ruled out.
- Static evidence is linked or recorded, including dump class/method names where relevant.
- The seam has a ledger row before runtime work starts.
- Only one callback/action/native seam is in scope.
- Probe code is separate from product code or is guarded so it cannot ship accidentally.
- Runtime behavior is log-only unless the approved question is specifically about behavior suppression.
- Payloads are not dereferenced unless the ledger already records payload confidence.
- Original/trampoline calls are not added unless the ledger already records ABI confidence for that seam.
- A feature/config/compile flag starts off by default for probe code.
- A crash-safe disable path exists before launch.
- Logs avoid private input traces and unnecessary user/game data.
- The exit plan is written: promote, revise, or delete.

## Post-Run Review Checklist

Use this before keeping, promoting, or repeating a probe.

- Did the game crash, hang, lose input, or need manual recovery?
- Did the disable path work without further code changes?
- Did the probe answer the original question?
- Was the event merely observed, or was it state-correlated?
- Did the probe alter native behavior, even accidentally?
- Did any logging, payload access, or original/trampoline path become part of the evidence?
- Was the evidence from a single seam, or was it blurred across a generated family?
- Does the confidence ladder move up, stay put, or move down?
- Should the probe be promoted, revised, or deleted?
- Was the ledger updated with the result and next action?

If a seam crashes, do not tune around it in the same broad hook family. Disable or delete the seam, record the failure, and restart from a lower-risk rung.

## Rollback And Kill-Switch Policy

Every probe needs a disable path that is known before it runs.

- Probe flags default off.
- Native hook installation must be conditional on the probe flag or a tightly scoped compile-time guard.
- A failed native seam should be removable by deleting one install entry or one probe module, not by untangling product behavior.
- A branch can be abandoned cleanly if the probe crashes.
- Product defaults must not depend on probe code.
- Crash RCA should prefer removal/reversion first, then a smaller isolated experiment if the question still matters.

For callback families, the kill switch must be per-callback when the question is per-callback. A single flag that installs an entire generated list is not a safe discovery mechanism.

## Ownership Boundaries

`stfc-mod` owns native hooks, probe policy, game-process safety, mod config, source wrappers, and the native seam ledger. Native probe code belongs here only when explicitly approved and isolated from product behavior.

`stfc-mod/.ax` is a nested private tooling repository. Treat it as a separate repo. Use it only for approved read-only inspection or declared tooling workflows; do not absorb it into `stfc-mod` and do not modify it during a docs-only slice.

`stfc-mod-sidecar` consumes emitted mod events and owns desktop-side processing, persistence, display, packaging, and protocol validation. It should not justify new native probes. It can document what stable events it needs and can reject or quarantine probe-only payloads.

`majel` owns higher-level data, ingestion, and application workflows. It can consume stable contracts through the sidecar path, but it is not evidence that a native game seam is safe.

The game install is runtime surface only. Do not place repo files, tooling files, probe plans, or documentation artifacts there.

## Native Seam Ledger

Use [Native Seam Ledger](NATIVE_SEAM_LEDGER.md) for the living confidence template, seeded RCA entries, and generated-family risk inventory. Add a ledger entry before any future runtime work that touches a native seam.
