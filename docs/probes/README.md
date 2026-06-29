# Probe Directory

This directory is the first-stop record for new exploratory probes and hooks.

Add one file per proposed probe before runtime implementation. Use `TEMPLATE.md`, keep the scope to one question, and link the matching `docs/NATIVE_SEAM_LEDGER.md` entry for native seams.

Naming convention:

```text
YYYYMMDD-short-slug.md
```

Examples:

```text
20260625-arcfall-action-queue-watchdog.md
20260625-territory-engage-blocker-canary.md
```

Rules:

- No new native hook without a probe entry.
- No new native hook without `HookDescriptor`, `HookModuleHealth`, and `HOOK_REGISTRY_SPUD_STATIC_DETOUR`.
- No generated hook family for discovery.
- No payload interpretation until the entry records payload confidence.
- Update the entry after the smoke test, even when the result is "crashed" or "delete."
