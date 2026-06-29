# Probe: <short name>

- Status: proposed | approved | deployed | observed | removed | promoted
- Owner:
- Date:
- Related patch label:
- Related timeline refresh ID:
- Related diff report:
- Native seam ledger entry:

## Question

One sentence. Example: "Can `Class.Method` be reached when the human performs one specific UI action?"

## Static Evidence

- Symbol:
- Method signature:
- String/script/config evidence:
- Old/new diff context:
- Why this target is the narrowest candidate:

## Risk

- Risk class: R0 | R1 | R2 | R3 | R4 | R5
- Confidence rung: symbol exists | static relationship | runtime observed | state-correlated | payload understood | product-safe
- Payload confidence:
- Original/trampoline confidence:
- Behavior change expected: yes | no

## Implementation Plan

- Module/file:
- Config or compile guard:
- Hook descriptor name:
- Target assembly:
- Target namespace:
- Target class:
- Target method:
- Install path:
- Log tag or event kind:

Registry requirements:

- Use `HookDescriptor`.
- Use `HookModuleHealth`.
- Use `HOOK_REGISTRY_SPUD_STATIC_DETOUR`.
- Do not use raw `SPUD_STATIC_DETOUR`.

## Disable Path

- Flag or code path to disable:
- File/entry to delete if it crashes:
- Expected boot log when disabled:

## Human Smoke Test

Goal:

Steps:

Expected log marker/event:

Stop immediately if:

Report back:

## Result

- Build/deploy command:
- Runtime command:
- Human action performed:
- Observed log/event evidence:
- Crash/hang/recovery notes:
- Answer to the question:

## Exit Decision

Delete | revise | repeat same seam | promote separately

Next action:
