# Windows Launcher Dogfood Friction

This ledger records friction observed while using Codex, LexRunner, Lex, AXF,
and GitHub to plan and deliver `Guffawaffle/stfc-mod#182`.

Statuses:

- `setup-gap`: repository configuration we own locally
- `candidate`: confirmed behavior awaiting issue deduplication or filing
- `tracked`: linked to an existing or newly filed issue
- `resolved`: workaround or product fix verified

| ID | Owner | Severity | Status | Observation | Evidence / issue |
|---|---|---:|---|---|---|
| DF-001 | `openai/codex` | high | tracked | The official Windows standalone installer adds a visible `bin` junction to PATH, but Codex resolves sandbox helpers relative to that visible path and fails with `CreateProcessWithLogonW failed: 2`. Calling `.codex/packages/standalone/current/bin/codex.exe` succeeds. | Existing [openai/codex#32655](https://github.com/openai/codex/issues/32655); local PATH points at the canonical `current/bin` workaround. |
| DF-002 | `Guffawaffle/lexrunner` | medium | tracked | MCP `status` reduces a structured schema failure to `Plan validation failed with 2 error(s)` and omits paths, messages, and remediation. | `LEXRUNNER_PYRAMID.json` passed to `status` on 2026-07-25; tracked by [Guffawaffle/lexrunner#855](https://github.com/Guffawaffle/lexrunner/issues/855). |
| DF-003 | `Guffawaffle/lexrunner` | high | tracked | `orchestrate:plan-batch` emits a deterministic pyramid artifact that cannot be consumed by `status`, `merge_order`, or gate execution, and the surface provides no explicit conversion or artifact-kind discriminator. | Pyramid has `planVersion`/`batches`; runtime plan requires `schemaVersion`/`items`; tracked by [Guffawaffle/lexrunner#855](https://github.com/Guffawaffle/lexrunner/issues/855). |
| DF-004 | `Guffawaffle/lexrunner` | medium | tracked | MCP `local_init` reports success after creating only `profile.yml` and `runner/`; after tracked configs exist, `local_init(force)` reports copying all four into `runner/`, but `doctor` still reports them missing because it checks the profile root. `workflow_guide(initial)` nevertheless says `canProceed: true`. | Reproduced in `D:\dev\stfc-mod` on 2026-07-25; root-copy workaround required; tracked by [Guffawaffle/lexrunner#856](https://github.com/Guffawaffle/lexrunner/issues/856). |
| DF-005 | `Guffawaffle/lexrunner` | medium | tracked | `README.mcp.md` advertises `plan_validate` and `plan_analyze`, but neither tool is exposed by the current MCP server. | Current MCP tool inventory inspected on 2026-07-25; tracked by [Guffawaffle/lexrunner#856](https://github.com/Guffawaffle/lexrunner/issues/856). |
| DF-006 | `Guffawaffle/lexrunner` | medium | tracked | The quickstart manual `plan.json` example includes `id`, `branch`, and `strategy`, but the current strict `PlanItem` schema accepts only `name`, `deps`, `gates`, and optional `tier`. | `docs/quickstart.md` versus `src/schema.ts` at LexRunner `main`; tracked by [Guffawaffle/lexrunner#856](https://github.com/Guffawaffle/lexrunner/issues/856). |
| DF-007 | `Guffawaffle/lex` | high | tracked | A repository without Lex policy silently stores all handoff frames as `workspace/unscoped`; onboarding does not lead the operator to a canonical policy contract. | Lex introspection: policy not found, modules `0`, four branch frames unscoped; tracked by [Guffawaffle/lex#800](https://github.com/Guffawaffle/lex/issues/800). |
| DF-008 | `Guffawaffle/lex` | high | tracked | Windows Code Atlas reports success with zero files for this C++/C#/Windows repository instead of declaring unsupported coverage or failing with remediation. | Prior STFC dogfood audit; tracked by [Guffawaffle/lex#800](https://github.com/Guffawaffle/lex/issues/800). |
| DF-009 | `Guffawaffle/axf` | medium | tracked | AXF handoff/session context did not reliably propagate the selected workspace root or return useful handoff guidance. | Existing [Guffawaffle/axf#42](https://github.com/Guffawaffle/axf/issues/42). |
| DF-010 | `Guffawaffle/stfc-mod` | high | resolved | Repository guidance named conflicting fork PR targets (`dev`, `guffa-dev`, and `main`) even though `origin/dev` does not exist and `guffa-dev` is stale. | Base audit selected `origin/main`; guidance reconciled during the anchor pass and recorded in [Guffawaffle/stfc-mod#182](https://github.com/Guffawaffle/stfc-mod/issues/182). |
| DF-011 | `openai/codex` | low | candidate | `codex doctor` emits a generic MCP configuration warning for absent forwarded environment variables even when AXF, Lex, and LexRunner are live and healthy through alternate authentication/configuration paths. | Restart audit on 2026-07-25. |
| DF-012 | `Guffawaffle/lex` | medium | tracked | Without a loaded policy, frame validation rejects an empty `module_scope` even though the workspace's existing fallback is `workspace/unscoped`; callers must know and supply that undocumented sentinel to persist a handoff. | Reproduced while storing frame `frame-1785006342138-93252cdc-5507-4c7b-9440-a53c657bdbd8`; tracked by [Guffawaffle/lex#800](https://github.com/Guffawaffle/lex/issues/800). |
| DF-013 | `Guffawaffle/lexrunner` | high | tracked | Runtime policy has only global required/optional gates and command gates per item, so heterogeneous required review, artifact, decision, and manual evidence cannot be faithfully enforced. This plan's `strict-required` rule can pass after only `git diff --check`. | Reproduced by the read-only pyramid audit; tracked by [Guffawaffle/lexrunner#857](https://github.com/Guffawaffle/lexrunner/issues/857). |
| DF-014 | `openai/codex` | medium | tracked | The Windows exec environment preserves `SystemRoot` but drops the standard `WINDIR` variable. Self-contained WPF apps then crash in `MS.Internal.FontCache.Util` before creating a window; restoring `WINDIR` from `SystemRoot` makes the same artifact render correctly. | Reproduced with Codex CLI `0.145.0` during WL-001 packaging smoke; tracked by [openai/codex#35545](https://github.com/openai/codex/issues/35545). |

## Filing policy

Before creating an issue:

1. reproduce the behavior with the current installed version;
2. separate local setup gaps from product defects;
3. search open and closed issues in the owning repository;
4. link existing issues instead of duplicating them;
5. include exact inputs, observed output, expected contract, and a bounded
   acceptance test.
