#!/usr/bin/env python3
"""Read the private bulk-selection journal without changing game state."""

import argparse
import json
import sys
from collections import Counter
from pathlib import Path


def quantities(value, key="key"):
    if not isinstance(value, dict) or value.get("supported") is not True:
        return None
    rows = value.get("entries")
    if not isinstance(rows, list) or len(rows) != value.get("count", len(rows)):
        return None
    result = {}
    for row in rows:
        if not isinstance(row, dict) or key not in row or type(row.get("quantity")) is not int:
            return None
        if row["quantity"] < 0 or row[key] in result:
            return None
        result[row[key]] = row["quantity"]
    # Native serialization may omit zero quantities. Omitted and zero both mean
    # deselected, while the edit event still preserves an explicit zero action.
    return {key: quantity for key, quantity in result.items() if quantity}


def state_maps(state):
    if not isinstance(state, dict):
        return {}
    container = state.get("selection", state)
    if not isinstance(container, dict):
        return {}
    result = {}
    if "pending" in container:
        result["pending"] = quantities(container["pending"])
    for name in ("categories", "groups"):
        groups = container.get(name)
        if not isinstance(groups, dict) or groups.get("supported") is not True:
            if name in container:
                result[f"{name}/$coverage"] = None
            continue
        result[f"{name}/$coverage"] = {}
        for group in groups.get("entries", []):
            for phase in ("selected", "editing"):
                result[f"{name}/{group['key']}/{phase}"] = quantities(group.get(phase))
    return result


def differences(before, after):
    return [dict(bundle=key, before=before.get(key, 0), after=after.get(key, 0))
            for key in sorted(before.keys() | after.keys(), key=str)
            if before.get(key, 0) != after.get(key, 0)]


def analyze(events):
    counts = Counter()
    sessions = {}
    last_write = {}
    changes, readbacks, edits, coverage, checkpoint_changes = [], [], [], [], []
    transport = Counter()
    for event in events:
        method = event.get("event", "unknown")
        counts[method] += 1
        reference = {key: event.get(key) for key in ("utc_ms", "session", "seq", "span")}
        if method == "session":
            sessions[event.get("session")] = event
        if method == "transport":
            transport[event.get("phase", "unknown")] += 1
        if method == "begin":
            continue
        detail = event.get("details") or {}
        if method == "edit":
            edits.append(dict(reference, **detail, unwind=event.get("unwind", False)))
        for delta in event.get("selection_changes", []):
            changes.append(dict(reference, method=method, scope=delta["scope"], changes=delta["changes"],
                                unwind=event.get("unwind", False)))
        for scope in event.get("selection_unknown", []):
            coverage.append(dict(reference, method=method, scope=scope, reason="unsupported snapshot"))
        before, after = state_maps(event.get("before")), state_maps(event.get("after"))
        for scope in before.keys() | after.keys():
            first, second = before.get(scope), after.get(scope)
            if scope.count("/") == 2:
                coverage_key = scope.split("/", 1)[0] + "/$coverage"
                if before.get(coverage_key) == {} and after.get(coverage_key) == {}:
                    first, second = before.get(scope, {}), after.get(scope, {})
            if first is None or second is None:
                coverage.append(dict(reference, method=method, scope=scope, reason="unsupported snapshot"))
                continue
            changed = differences(first, second)
            if changed:
                changes.append(dict(reference, method=method, scope=scope, changes=changed,
                                    unwind=event.get("unwind", False)))
        key = detail.get("key")
        if method == "preferences_set":
            written = quantities(detail.get("written"), "bundle")
            if written is not None and not event.get("unwind"):
                last_write[key] = (written, reference)
            else:
                # A later unreadable write supersedes any earlier baseline.
                last_write.pop(key, None)
                coverage.append(dict(reference, method=method, reason="write result unknown"))
        if method == "preferences_get":
            read = quantities(detail.get("read"), "bundle")
            checkpoint = event.get("checkpoint") or {}
            baseline = quantities(checkpoint.get("values"), "bundle")
            if read is not None and baseline is not None and not event.get("unwind"):
                changed = differences(baseline, read)
                if changed:
                    checkpoint_changes.append(dict(reference, key=key, basis=checkpoint.get("basis"),
                                                   baseline_session=checkpoint.get("session"), changes=changed))
            if read is None or event.get("unwind"):
                coverage.append(dict(reference, method=method, reason="read result unknown"))
            elif key in last_write:
                written, origin = last_write[key]
                changed = differences(written, read)
                if changed:
                    readbacks.append(dict(reference, key=key, changes=changed, written_at=origin,
                                          cloud_loaded=(event.get("after") or {}).get("cloud_loaded"),
                                          same_session=origin["session"] == event.get("session")))
    return dict(event_counts=dict(counts), sessions=list(sessions.values()), edits=edits,
                state_changes=changes, reads_differing_from_last_observed_write=readbacks,
                reads_differing_from_checkpoint=checkpoint_changes,
                unknown_observations=coverage, transport_counts=dict(transport),
                limits="Changes describe observed native calls. Nested calls can report the same transition. "
                       "Cloud attempts or successful transport do not prove durable persistence. "
                       "Across sessions, confirm the same account. Missing/rotated history is not reconstructed.")


def journal_events(root):
    nested = root / "science/claims"
    if (nested / "bulk_selection.jsonl").is_file():
        root = nested
    files = [root / (f"bulk_selection.{number}.jsonl" if number else "bulk_selection.jsonl")
             for number in (4, 3, 2, 1, 0)]
    found = False
    for path in files:
        if not path.is_file():
            continue
        found = True
        with path.open(encoding="utf-8") as stream:
            for number, line in enumerate(stream, 1):
                if not line.strip():
                    continue
                try:
                    event = json.loads(line)
                except json.JSONDecodeError as error:
                    raise ValueError(f"Incomplete/invalid journal record in {path.name}:{number}; retry after the writer flushes") from error
                if not isinstance(event, dict) or event.get("schema") != 1:
                    raise ValueError(f"Unsupported journal record in {path.name}:{number}")
                yield event
    if not found:
        raise FileNotFoundError("No bulk-selection journal in the supplied directory")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, required=True,
                        help="Game/mod data directory or explicit claims trace/snapshot directory")
    parser.add_argument("--limit", type=int, default=30, help="Maximum recent observations per section")
    args = parser.parse_args()
    try:
        if not 1 <= args.limit <= 1000:
            raise ValueError("--limit must be between 1 and 1000")
        report = analyze(journal_events(args.game_dir.resolve(strict=True)))
        for key in ("edits", "state_changes", "reads_differing_from_last_observed_write",
                    "reads_differing_from_checkpoint", "unknown_observations"):
            values = report[key]
            report[key + "_total"] = len(values)
            report[key] = values[-args.limit:]
        print(json.dumps(report, indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(json.dumps(dict(ok=False, error=str(error))))
        return 1


if __name__ == "__main__":
    sys.exit(main())
