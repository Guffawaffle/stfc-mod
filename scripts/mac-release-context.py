#!/usr/bin/env python3
"""Resolve signed Mac Pre/Release identity without weakening CI artifact eligibility."""

import argparse
import json
from pathlib import Path
import re


TAG = re.compile(r"v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+-guffa\.[1-9][0-9]*\Z")


def resolve(operation: str, release_tag: str, prerelease: bool, run: dict) -> dict:
    if operation not in ("sign", "sign-run") or type(prerelease) is not bool:
        raise ValueError("Unsupported signing operation or prerelease value")
    if not isinstance(run, dict):
        raise ValueError("Producing CI run metadata must be an object")
    if not isinstance(release_tag, str) or (release_tag and not TAG.fullmatch(release_tag)):
        raise ValueError("release_tag must be an exact v1.1.9.5-guffa.N style tag")
    if operation == "sign" and not release_tag:
        raise ValueError("Named signing requires the planned release_tag")
    if not release_tag and not prerelease:
        raise ValueError("A full Release requires the planned release_tag")

    # DEV belongs to ordinary unsigned builds. Every optional signing output is
    # Pre, including an untagged CI test whose other jobs have not passed yet.
    channel = "pre" if prerelease else "release"
    release_selection = operation == "sign" or channel == "release"
    if release_tag and (run.get("status") != "completed" or run.get("conclusion") != "success"):
        raise ValueError("A named Pre or Release build requires completed successful CI")
    if release_selection and (run.get("head_branch") != "play" or run.get("event") != "push"):
        raise ValueError("Release signing requires a successful PLAY push")
    run_number = str(run.get("run_number", ""))
    if not re.fullmatch(r"[1-9][0-9]*", run_number):
        raise ValueError("Producing CI run_number must be a positive integer")
    run_attempt = str(run.get("run_attempt", ""))
    if not re.fullmatch(r"[1-9][0-9]*", run_attempt):
        raise ValueError("Producing CI run_attempt must be a positive integer")
    return {
        "schema_version": "stfc-mac-release-context/v1",
        "channel": channel,
        "release_tag": release_tag,
        "prerelease": prerelease,
        "selection_mode": "release" if release_selection else "test",
        "run_number": run_number,
        "run_attempt": run_attempt,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--operation", choices=("sign", "sign-run"), required=True)
    parser.add_argument("--release-tag", default="")
    parser.add_argument("--prerelease", choices=("true", "false"), required=True)
    parser.add_argument("--run-json", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()
    try:
        context = resolve(args.operation, args.release_tag, args.prerelease == "true",
                          json.loads(args.run_json.read_text(encoding="utf-8")))
    except (OSError, ValueError, TypeError) as error:
        raise SystemExit(str(error)) from error
    args.output.write_text(json.dumps(context, indent=2) + "\n", encoding="utf-8")
    if args.github_output:
        with args.github_output.open("a", encoding="utf-8") as output:
            for key, value in (("build_channel", context["channel"]),
                               ("release_tag", context["release_tag"]),
                               ("prerelease", str(context["prerelease"]).lower()),
                               ("selection_mode", context["selection_mode"]),
                               ("run_number", context["run_number"]),
                               ("run_attempt", context["run_attempt"])):
                output.write(f"{key}={value}\n")


if __name__ == "__main__":
    main()
