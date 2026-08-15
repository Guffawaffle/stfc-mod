# Runtime Impact Targeted Diagnostic

Date: 2026-08-15
Tracking issue: [#257](https://github.com/Guffawaffle/stfc-mod/issues/257)
Concern ID: `runtime-impact`

## Purpose

Identify mod-owned work correlated with aged-session frame latency, especially
hostile selection and attack actions, without adding high-volume records to
`community_patch.log`.

## Activation

```toml
[advanced.diagnostics.concerns]
enabled = ["runtime-impact"]
```

Restart the game after changing the allowlist. Records are written only to:

```text
community_patch_target_runtime-impact.jsonl
```

under `[advanced.diagnostics.files].root`, or the standard diagnostics location
when no root override is configured. The shared policy retains at most two
1 MiB files for this concern.

## Records

`probe-window` aggregates mod-owned timer samples over a fixed 5-second window.
It includes a stable probe name, sample count, average and maximum microseconds,
and counts above 250 and 1,000 microseconds. Instrumentation overhead is itself
reported as a probe.

`space-action-timing` records an attempted space action or a detour lasting at
least 1,000 microseconds. It includes the outcome, total and phase timings,
fleet state integers, pre-scan counts, input/context flags, and handled/slow
booleans.

Input flag bits are: physical primary (0), deferred primary for fleet (1),
deferred pending (2), secondary (3), queue (4), queue clear (5), recall (6),
repair (7), and recall cancel (8).

Context flag bits are: mining visible (0), star node visible (1), navigation
visible (2), and pre-scan fallback used (3).

## Data Boundary

The schema intentionally excludes fleet IDs, target IDs, object pointers,
player-visible names, key values, and native payload dumps. The producer uses
fixed-size event storage and the shared bounded targeted-diagnostics queue.

## Lifecycle

This is a temporary concern introduced in 2.1.0 with a 2.2.0 sunset. Remove or
revise it when hostile-interaction and aged-idle captures identify whether a
mod-owned path causes frame stalls. Promotion requires a supported performance
workflow and measured low overhead.
