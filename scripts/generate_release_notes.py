#!/usr/bin/env python3
"""Generate fork release notes from merged PRs instead of raw commit titles."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


STABLE_FORK_TAG_PATTERN = re.compile(r"^v(?P<major>\d+)\.(?P<minor>\d+)\.(?P<patch>\d+)-guffa\.(?P<fork>\d+)$")


@dataclass(frozen=True)
class PullRequest:
    number: int
    title: str
    url: str
    merge_commit: str
    issues: tuple[int, ...]


@dataclass(frozen=True)
class Issue:
    number: int
    title: str
    url: str
    prs: tuple[int, ...]


def run(command: list[str]) -> str:
    result = subprocess.run(command, check=False, capture_output=True, text=True, encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result.stdout


def git(*args: str) -> str:
    return run(["git", *args]).strip()


def gh_json(*args: str) -> Any:
    output = run(["gh", *args])
    return json.loads(output)


def parse_stable_fork_tag(tag: str) -> tuple[int, int, int, int] | None:
    match = STABLE_FORK_TAG_PATTERN.match(tag)
    if not match:
        return None
    return tuple(int(match.group(name)) for name in ("major", "minor", "patch", "fork"))


def resolve_previous_tag(current_tag: str, override: str | None) -> str:
    if override:
        return override

    current_version = parse_stable_fork_tag(current_tag)
    tags = git("tag", "--list", "v*-guffa.*").splitlines()
    stable_tags = [(parsed, tag) for tag in tags if (parsed := parse_stable_fork_tag(tag)) is not None]
    if not stable_tags:
        raise RuntimeError("no stable vX.Y.Z-guffa.N tags found")

    if current_version is not None:
        candidates = [(version, tag) for version, tag in stable_tags if version < current_version]
    else:
        candidates = [(version, tag) for version, tag in stable_tags if tag != current_tag]

    if not candidates:
        raise RuntimeError(f"could not infer a previous stable fork tag for {current_tag}")

    return max(candidates, key=lambda item: item[0])[1]


def resolve_repository(explicit_repo: str | None) -> str:
    if explicit_repo:
        return explicit_repo
    if os.environ.get("GITHUB_REPOSITORY"):
        return os.environ["GITHUB_REPOSITORY"]
    data = gh_json("repo", "view", "--json", "nameWithOwner")
    return str(data["nameWithOwner"])


def commits_in_range(previous_tag: str, target: str) -> list[str]:
    return git("rev-list", "--reverse", f"{previous_tag}..{target}").splitlines()


def merged_prs_for_commits(repo: str, commit_order: dict[str, int], pr_limit: int) -> list[PullRequest]:
    data = gh_json(
        "pr",
        "list",
        "--repo",
        repo,
        "--state",
        "merged",
        "--base",
        "main",
        "--limit",
        str(pr_limit),
        "--json",
        "number,title,url,mergeCommit,closingIssuesReferences",
    )

    prs: list[PullRequest] = []
    for entry in data:
        merge_commit = ((entry.get("mergeCommit") or {}).get("oid") or "").lower()
        if merge_commit not in commit_order:
            continue
        issue_numbers = tuple(
            sorted(
                {
                    int(issue["number"])
                    for issue in entry.get("closingIssuesReferences") or []
                    if isinstance(issue.get("number"), int)
                }
            )
        )
        prs.append(
            PullRequest(
                number=int(entry["number"]),
                title=str(entry["title"]),
                url=str(entry["url"]),
                merge_commit=merge_commit,
                issues=issue_numbers,
            )
        )

    return sorted(prs, key=lambda pr: commit_order[pr.merge_commit])


def issues_for_prs(repo: str, prs: list[PullRequest]) -> list[Issue]:
    pr_by_issue: dict[int, list[int]] = {}
    for pr in prs:
        for issue_number in pr.issues:
            pr_by_issue.setdefault(issue_number, []).append(pr.number)

    issues: list[Issue] = []
    for issue_number in sorted(pr_by_issue):
        data = gh_json("issue", "view", str(issue_number), "--repo", repo, "--json", "number,title,url")
        issues.append(
            Issue(
                number=int(data["number"]),
                title=str(data["title"]),
                url=str(data["url"]),
                prs=tuple(pr_by_issue[issue_number]),
            )
        )
    return issues


def markdown_link(label: str, url: str) -> str:
    return f"[{label}]({url})"


def render_release_notes(
    tag: str,
    previous_tag: str,
    repo: str,
    prs: list[PullRequest],
    issues: list[Issue],
) -> str:
    readme_url = f"https://github.com/{repo}/blob/{tag}/README.md#whats-different-in-this-fork"
    compare_url = f"https://github.com/{repo}/compare/{previous_tag}...{tag}"
    previous_release_url = f"https://github.com/{repo}/releases/tag/{previous_tag}"

    lines = [
        f"# {tag}",
        "",
        "> **UNOFFICIAL FORK - This is NOT the official STFC Community Mod.**",
        ">",
        f"> This build comes from [Guffawaffle's personal fork](https://github.com/{repo}) and contains experimental features not yet accepted upstream.",
        "> **For the official mod, go to [netniV/stfc-mod](https://github.com/netniV/stfc-mod/releases/latest).**",
        "> **To support the project, [sponsor netniV](https://github.com/sponsors/netniV)** - he built this.",
        "",
        "---",
        "",
        "### Release Assets",
        "- `stfc-community-mod.zip` - recommended Windows download",
        f"- `stfc-community-mod-{tag}.zip` - versioned Windows archive",
        "- `stfc-community-mod-windows-x64.tar.zst` - Windows archive for launcher/update tooling",
        "- `stfc-community-mod-macos-universal.tar.zst` - macOS dylib archive for launcher/update tooling",
        "- `version.dll` - direct drop-in update for existing installs",
        "- `SHA256SUMS.txt` - optional hash verification for release assets",
        "",
        "### Included Changes",
        f"Compared with {markdown_link(previous_tag, previous_release_url)}.",
        "",
    ]

    if prs:
        lines.append("#### Merged PRs")
        for pr in prs:
            issue_suffix = ""
            if pr.issues:
                issue_suffix = " (fixes " + ", ".join(f"#{number}" for number in pr.issues) + ")"
            lines.append(f"- {markdown_link(f'#{pr.number}', pr.url)} {pr.title}{issue_suffix}")
        lines.append("")
    else:
        lines.extend(["#### Merged PRs", "- No merged PRs were detected in this release range.", ""])

    if issues:
        lines.append("#### Issues Fixed")
        for issue in issues:
            via = ", ".join(f"#{number}" for number in issue.prs)
            lines.append(f"- {markdown_link(f'#{issue.number}', issue.url)} {issue.title} (via {via})")
        lines.append("")

    lines.extend(
        [
            "### What's Different In This Fork?",
            f"See the {markdown_link('fork README', readme_url)} for a list of experimental features.",
            "",
            "### Compare",
            f"Full compare: {compare_url}",
            "",
        ]
    )

    return "\n".join(lines)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", default=os.environ.get("GITHUB_REF_NAME", ""), help="Release tag to describe.")
    parser.add_argument("--target", default=os.environ.get("GITHUB_SHA", "HEAD"), help="Target commit/ref for the release.")
    parser.add_argument("--previous-tag", default="", help="Previous stable fork tag. Inferred when omitted.")
    parser.add_argument("--repo", default="", help="GitHub repository owner/name. Defaults to current gh repo.")
    parser.add_argument("--pr-limit", type=int, default=200, help="Merged PRs to inspect when matching merge commits.")
    parser.add_argument("--output", default="", help="Write markdown to this path instead of stdout.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if not args.tag:
        raise RuntimeError("--tag is required when GITHUB_REF_NAME is not set")

    repo = resolve_repository(args.repo or None)
    previous_tag = resolve_previous_tag(args.tag, args.previous_tag or None)
    target_sha = git("rev-parse", f"{args.target}^{{commit}}")
    commit_order = {sha.lower(): index for index, sha in enumerate(commits_in_range(previous_tag, target_sha))}
    prs = merged_prs_for_commits(repo, commit_order, args.pr_limit)
    issues = issues_for_prs(repo, prs)
    notes = render_release_notes(args.tag, previous_tag, repo, prs, issues)

    if args.output:
        Path(args.output).write_text(notes, encoding="utf-8")
    else:
        print(notes)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
