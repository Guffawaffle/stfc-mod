# Feature Development SOP

Purpose: turn a gameplay symptom or feature idea into a bounded investigation, a safe iterative development loop, and a reviewable release without treating an IL2CPP dump as runtime truth.

This SOP coordinates the existing safety and release documents. Native probe policy remains in [Native Probe Safety](NATIVE_PROBE_SAFETY.md), per-seam confidence remains in [Native Seam Ledger](NATIVE_SEAM_LEDGER.md), and probe experiments start in the [Probe Directory](probes/README.md). Patch-to-patch discovery remains in [Patch Delta Delve SOP](PATCH_DELVE_SOP.md).

## 1. Record The Intent

Create a small investigation record before choosing code:

- User-visible symptom or desired outcome.
- Priority and frequency.
- Reproduction steps, including intermittent or unknown steps.
- Expected behavior and observed behavior.
- Scope boundaries and explicit non-goals.
- What evidence would distinguish the leading hypotheses.
- A placeholder is acceptable when a reported issue has been forgotten. Do not invent its symptom to fill the gap.

Search existing issues, branches, investigation logs, and dormant probes before opening a new issue. An adjacent historical issue is context, not a duplicate, when it covers a different ownership boundary.

## 2. Map Static Touchpoints

Use AXF/Lex first, after inspecting each capability. A useful order is:

1. `research-search` or `dump-find` for the symptom vocabulary.
2. `dump-pack` for the strongest owner class.
3. `dump-method`, `dump-enum`, and `dump-script` for exact signatures and native ABI evidence.
4. Repo source and documentation search for wrappers, current hooks, config ownership, tests, and abandoned experiments.

Record each finding as one of:

- Verified source relationship.
- Static dump symbol or signature.
- Hypothesized relationship.
- Existing runtime evidence.

The dump proves that a symbol exists. It does not prove a caller, callee, execution order, safety, ownership, or product semantics. A class neighborhood is not a call graph.

## 3. Establish A Passive Baseline

Use existing read-only runtime surfaces before adding a hook:

- Capture the relevant live model before, during, and after one human action.
- Read existing recent events and bounded logs.
- Preserve numeric state alongside friendly labels so enum drift cannot hide evidence.
- Identify whether the UI, client model, incoming server model, and request result agree.

Runtime observation commands are serialized. The current file-backed live-debug request channel is single-flight; concurrent `live-state` requests can collide and time out. Parallelize static corpus work, not live commands or human repro windows.

## 4. Write The Probe Contract Before Code

For a new native seam:

1. Add one file under `docs/probes/` from `TEMPLATE.md`.
2. Add one seam entry to `docs/NATIVE_SEAM_LEDGER.md`.
3. State one question and one human action.
4. Record the exact assembly, namespace, class, method, overload, parameters, and return type.
5. Choose the lowest risk rung that can answer the question.
6. Define the disable path and deletion path before implementation.

One probe normally owns one hook target. If a second seam is needed, finish, remove, or explicitly retire the first probe before moving downstream. Correlation across existing passive events is preferred to installing a hook family.

## 5. Build Airlocks Into The Probe

Probe code lives in an independent module and is removable by deleting that module and one install entry. It must use `HookDescriptor`, `HookModuleHealth`, and `HOOK_REGISTRY_SPUD_STATIC_DETOUR`.

The investigation advances through explicit airlocks:

1. **Reachability:** one seam, bounded entry/exit evidence, no payload interpretation beyond already proven scalar ABI.
2. **Parameters:** add one field or argument only after its signature and lifetime are recorded.
3. **Call traffic:** separately enable one-shot, module-relative return addresses; symbolize callers offline. Resolve direct callees from offline disassembly of the exact game build and map their RVAs back to current script/dump metadata. A dump neighborhood is not a callee list.
4. **State correlation:** join request, incoming-model, client-model, and UI events by sequence, fleet ID, and monotonic time.
5. **Behavior:** implement or suppress behavior only in a separate promotion change after the failure mechanism is demonstrated.

Required controls:

- Science-tier, default-off config; never advertise it in the release example.
- At most one experimental seam enabled at a time.
- No action injection, callback replacement, pointer retention, or server request from a discovery probe.
- Bounded and deduplicated logging with sequence, monotonic timestamp, thread ID, seam, phase, relevant stable IDs, numeric states, and result.
- Stack capture is off independently of event capture and limited to the first matching event or an explicit small budget.
- Fail closed on missing class, overload ambiguity, signature drift, duplicate ownership, or unsupported platform.
- A documented TOML kill switch that can be changed without rebuilding after a crash.

Calling the original/trampoline is itself a confidence claim. Record ABI evidence and classify the risk accordingly before loading the probe.

## 6. Use A Hypothesis-Driven Knob Loop

"Knob turning" means changing one experimental variable to answer one stated question. For every iteration, record:

| Field | Required entry |
| --- | --- |
| Hypothesis | The one explanation being tested. |
| Knob | One seam, argument, filter, cadence, stack budget, or behavior flag. |
| Expected distinction | What result supports or rejects the hypothesis. |
| Repro action | One human action in one bounded observation window. |
| Evidence | Marker/sequence range, log/event artifact, build commit, and config snapshot. |
| Outcome | Supported, rejected, inconclusive, crashed, or tooling failure. |
| Next move | Keep, revert, move the probe, narrow the question, or stop. |

Do not change the hook target, payload reads, logging cadence, and behavior in the same iteration. A tooling failure is not a negative gameplay result.

## 7. Separate Discovery From Resolution

The discovery branch should answer where state diverges and why. The resolution branch should contain the smallest behavior change that repairs that demonstrated divergence.

Before coding the fix, write:

- Root cause and evidence strength.
- Authoritative state owner.
- Required behavior and invariants.
- Failure and rollback behavior.
- Pure policy that can be tested outside IL2CPP.
- Runtime seams that remain and probes that will be deleted.

The default outcome for a probe is deletion. Promotion requires an explicit hook-support tier decision.

## 8. QA Ladder

Run the narrowest applicable checks at each level:

1. Pure tests for parsing, state projection, dedupe, rate limits, and policy.
2. `git diff --check` and gameplay seam scanner.
3. Narrow XMake target, then the repository validation contract.
4. Releasedbg deploy and boot/log-health check.
5. One positive human reproduction.
6. One negative/control action that must remain unchanged.
7. Repeat across reconnect, scene change, or reload when state lifetime matters.
8. Windows and macOS CI when shared native code changes.
9. Production-build smoke before promoting a user-facing hook.

An AXF cycle proves build, deployment, boot, and bounded log health. It does not prove the human gameplay gesture unless that gesture was actually performed and recorded.

## 9. Review And Release

A ready-for-review change includes:

- Linked issue and investigation record.
- Root cause, not only symptom mitigation.
- Final seam ownership and support tier.
- Tests and exact runtime evidence.
- Config defaults and example-file placement.
- Probe removal or explicit science retention.
- Platform guards and unsupported-platform behavior.
- Rollback instructions.
- Honest statement of untested gestures or environments.

After release, preserve concise durable lessons in the SOP or seam ledger. Keep raw timelines and high-volume probe output out of product documentation.

## Current Tooling Notes

- `dump-pack`, `dump-find`, and short-class-name `dump-method` queries work for current static navigation.
- Scoped `dump-method --fts` with a fully qualified class currently fails in the private query adapter; use the short class name or `dump-pack` and record this as a tooling limitation.
- `live-state` and other live-debug requests must run serially because their file transport is single-flight.
- Existing live snapshots are useful for client-model state but may not expose job state, deployed state, enabled-action masks, request callbacks, or real callers.
- Obtain real callers from a separately gated runtime stack sample. Identify direct callees from build-matched offline disassembly, then validate the next seam by moving the probe. Do not infer either relationship from adjacent dump symbols.
