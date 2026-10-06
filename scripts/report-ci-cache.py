#!/usr/bin/env python3
"""Read-only per-job cache observations and phase timings for the Actions summary."""

import argparse
from collections import defaultdict
from datetime import datetime
import json
import os
from pathlib import Path
import re
import subprocess


def api_pages(path, collection):
    result = subprocess.run(["gh", "api", "--paginate", "--slurp", path],
                            check=True, capture_output=True, text=True)
    entries = {}
    for page in json.loads(result.stdout):
        entries.update((entry["id"], entry) for entry in page[collection])
    return list(entries.values())


def cell(value):
    return str(value).replace("|", "\\|").replace("\n", " ").replace("\r", " ")


def render_report(job, caches, restores, sizes):
    lines = [f"## Cache effectiveness: {cell(job['name'])}", "",
             "| Phase | Seconds | Result |", "| --- | ---: | --- |"]
    for step in job["steps"]:
        if step["name"] in ("Setup XMake", "Configure", "Build") or step["name"].startswith("Test "):
            seconds = "unavailable"
            if step["status"] == "completed" and step.get("started_at") and step.get("completed_at"):
                start = datetime.fromisoformat(step["started_at"].replace("Z", "+00:00"))
                end = datetime.fromisoformat(step["completed_at"].replace("Z", "+00:00"))
                seconds = f"{(end - start).total_seconds():.1f}"
            lines.append(f"| {cell(step['name'])} | {seconds} | {cell(step.get('conclusion') or step['status'])} |")
    lines += ["", "| Cache | Restore | Requested key | Restored key | Matching requested key currently stored under |",
              "| --- | --- | --- | --- | --- |"]
    for name, outputs in restores.items():
        requested = outputs.get("cache-primary-key", "")
        matched = outputs.get("cache-matched-key", "")
        result = ("exact hit" if outputs.get("cache-hit") == "true" else
                  "fallback hit" if matched else "miss" if requested else "unavailable / skipped")
        refs = sorted({cache["ref"] for cache in caches if cache["key"] == requested})
        lines.append(f"| {cell(name)} | {result} | {cell(requested or '—')} | {cell(matched or '—')} | {cell(', '.join(refs) or 'none observed')} |")
    totals = defaultdict(int)
    for cache in caches:
        totals[cache["ref"]] += cache["size_in_bytes"]
    lines += ["", "| Current repository cache scope | MiB |", "| --- | ---: |"]
    lines += [f"| {cell(ref)} | {size / 1024**2:.1f} |" for ref, size in sorted(totals.items())]
    lines += [f"| **Total** | **{sum(totals.values()) / 1024**2:.1f}** |", "",
              "| Local compiler cache after build | MiB |", "| --- | ---: |"]
    lines += [f"| {cell(name)} | {'unavailable' if size is None else f'{size / 1024**2:.1f}'} |" for name, size in sizes.items()]
    lines += ["", "Inventory is observed after the build; other jobs may be saving or evicting entries. "
              "A matching key in another scope is diagnostic evidence, not proof of restore access or matching cache version. "
              "A miss alone does not prove eviction.", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--run-id", required=True, type=int)
    parser.add_argument("--attempt", required=True, type=int)
    parser.add_argument("--job-name", required=True)
    parser.add_argument("--summary", default=os.environ.get("GITHUB_STEP_SUMMARY"))
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repo) or args.run_id < 1 or args.attempt < 1:
        parser.error("Expected owner/repository and positive run/attempt IDs")
    jobs = api_pages(f"repos/{args.repo}/actions/runs/{args.run_id}/attempts/{args.attempt}/jobs?per_page=100", "jobs")
    job = next(job for job in jobs if job["name"] == args.job_name)
    caches = api_pages(f"repos/{args.repo}/actions/caches?per_page=100&sort=created_at&direction=desc", "actions_caches")
    sizes = {}
    for name, value in json.loads(os.environ.get("CACHE_PATHS_JSON", "{}")).items():
        path = Path(value)
        sizes[name] = sum(file.stat().st_size for file in path.rglob("*") if file.is_file()) if path.is_dir() else None
    report = render_report(job, caches, json.loads(os.environ.get("CACHE_RESTORES_JSON", "{}")), sizes)
    print(report)
    if args.summary:
        with open(args.summary, "a", encoding="utf-8") as summary:
            summary.write(report)


if __name__ == "__main__":
    main()
