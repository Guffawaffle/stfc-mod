#!/usr/bin/env python3
"""Keep one compiler-cache snapshot per family on a single fork branch.

Dependency, tool and Swift module caches are deliberately outside this policy.
Without --apply, print the plan without deleting anything.
"""

import argparse
import json
import re
import subprocess
from collections import defaultdict
from urllib.parse import urlencode


REPOSITORY = "Guffawaffle/stfc-mod"
BRANCH_REFS = {f"refs/heads/{branch}" for branch in ("main", "dev", "play", "play-dev")}
FAMILIES = (
    ("windows-sccache", re.compile(r"windows-x64-release-sccache-.+-[0-9a-f]{40}")),
    ("arm64-protobuf", re.compile(r"macos-arm64-release-protobuf-sccache-.+-[0-9a-f]{40}")),
    ("x86_64-protobuf", re.compile(r"macos-x86_64-release-protobuf-sccache-.+-[0-9a-f]{40}")),
    ("arm64-xmake", re.compile(r"macos-arm64-release-v[0-9]+-[0-9a-f]{40}")),
    ("x86_64-xmake", re.compile(r"macos-x86_64-release-v[0-9]+-[0-9a-f]{40}")),
)


def select_obsolete(caches, ref):
    if ref not in BRANCH_REFS:
        raise ValueError("Expected a supported fork branch ref, never a PR or tag ref")
    families = defaultdict(list)
    for cache in caches:
        if cache["ref"] != ref:
            continue
        for family, pattern in FAMILIES:
            if pattern.fullmatch(cache["key"]):
                families[family].append(cache)
                break
    obsolete = []
    for snapshots in families.values():
        # Cache contents are immutable; keep the most recently created snapshot.
        snapshots.sort(key=lambda cache: (cache["created_at"], cache["id"]), reverse=True)
        obsolete.extend(snapshots[1:])
    return obsolete


def list_caches(repo, ref):
    caches = []
    page = 1
    while True:
        query = urlencode({"ref": ref, "per_page": 100, "page": page})
        result = subprocess.run(
            ["gh", "api", f"repos/{repo}/actions/caches?{query}"],
            check=True, capture_output=True, text=True,
        )
        entries = json.loads(result.stdout)["actions_caches"]
        caches.extend(entries)
        if len(entries) < 100:
            return caches
        page += 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--ref", required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    if args.repo != REPOSITORY or args.ref not in BRANCH_REFS:
        parser.error("Cache maintenance is restricted to supported Guffawaffle/stfc-mod branches")

    obsolete = select_obsolete(list_caches(args.repo, args.ref), args.ref)
    reclaimed = sum(cache["size_in_bytes"] for cache in obsolete)
    print(f"{'Deleting' if args.apply else 'Would delete'} {len(obsolete)} compiler snapshots "
          f"on {args.ref} ({reclaimed / 1024**2:.1f} MiB)")
    for cache in obsolete:
        print(f"  {cache['id']}: {cache['key']}")
        if args.apply:
            subprocess.run(
                ["gh", "api", "--method", "DELETE", f"repos/{args.repo}/actions/caches/{cache['id']}"],
                check=True,
            )


if __name__ == "__main__":
    main()
