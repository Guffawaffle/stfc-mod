# Windows Launcher Parity+ Intent

## Outcome

Deliver a Windows 10/11 x64 launcher with parity-plus behavior relative to the
current macOS launcher. The Windows launcher must make the supported community
mod path discoverable, installable, updateable, configurable, launchable,
diagnosable, repairable, and safely reversible.

## Planning authority

- GitHub epic: `Guffawaffle/stfc-mod#182`
- Work items: `docs/windows-launcher/WORK_ITEMS.json`
- Product contract: `docs/windows-launcher/CONTRACT.md`
- Dependency pyramid: `docs/windows-launcher/LEXRUNNER_PYRAMID.json`
- Executable gate plan: `docs/windows-launcher/LEXRUNNER_EXECUTION_PLAN.json`

## Operating constraints

- This foreground chat remains the run controller and GitHub-write authority.
- Background workers receive one ready work item, bounded file scope, and
  verification commands.
- Workers do not receive release, signing, secrets, or GitHub-write authority.
- Existing C++ Windows proxy-DLL behavior and macOS launcher behavior must not
  regress.
- Filesystem mutation, update, repair, rollback, and diagnostic collection are
  treated as security- and data-loss-sensitive operations.

## Delivery target and pause state

The fork's `main` branch is the implementation base and fork PR target. This
decision was anchored on 2026-07-25 after confirming that the prior planning
baseline and `origin/main` had identical trees, while `upstream/dev` omitted
335 fork commits.

Launcher implementation is paused after planning. No work item is ready or
dispatched, and isolated worker worktrees require explicit authorization when
the sprint resumes.

## Known runner enforcement limit

Until `Guffawaffle/lexrunner#857` is resolved, LexRunner status is a scheduling
and command-gate signal, not final acceptance authority. The foreground
orchestrator must verify every required fixture, review, artifact, decision,
manual, accessibility, rollback, release, and security item in
`WORK_ITEMS.json` and the linked GitHub issue before accepting or merging work.

## Success criteria

- Every work item in `Guffawaffle/stfc-mod#182` closes with linked implementation
  and validation evidence.
- The declared dependency DAG is preserved through dispatch and acceptance.
- Windows CI builds and tests the launcher and existing proxy DLL.
- Manual smoke, accessibility, rollback, release-withdrawal, and offline-use
  evidence is recorded.
- Dogfood friction is logged and deduplicated against the owning repository.
