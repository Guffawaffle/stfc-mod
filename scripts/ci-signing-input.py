#!/usr/bin/env python3
"""Select an immutable CI artifact; never execute or check out producer code."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import stat
import zipfile


def positive(value):
    text = str(value)
    if not re.fullmatch(r"[1-9][0-9]*", text):
        raise ValueError("Expected an exact positive run/attempt/artifact ID")
    return text


def select(run, artifacts, repository, run_id, platform, mode="test", expected_sha=None,
           expected_attempt=None, expected_artifact=None):
    if mode not in {"test", "release"}:
        raise ValueError("Unknown signing mode")
    run_id = positive(run_id)
    sha = run.get("head_sha", "")
    if (str(run.get("id")) != run_id or run.get("repository", {}).get("full_name") != repository
            or run.get("head_repository", {}).get("full_name") != repository
            or run.get("path") != ".github/workflows/ci.yaml"
            or run.get("event") not in {"push", "workflow_dispatch", "pull_request"}
            or run.get("status") not in {"completed", "in_progress"}
            or (run.get("status") == "completed" and not run.get("conclusion"))
            or not re.fullmatch(r"[0-9a-f]{40}", sha)):
        raise ValueError("Select a started same-repository Build run; fork and other workflow inputs are refused")
    attempt = positive(run.get("run_attempt"))
    if expected_sha and expected_sha != sha:
        raise ValueError("Selected run HEAD changed or differs from the requested SHA")
    if expected_attempt and positive(expected_attempt) != attempt:
        raise ValueError("Selected run was rerun after selection; select the new attempt explicitly")
    if mode == "release" and (run.get("status") != "completed" or run["event"] != "push" or run.get("head_branch") != "play"
                              or run["conclusion"] != "success"):
        raise ValueError("Release signing still requires a successful play push build")
    names = {"macos": ("stfc-community-mod-installer", "stfc-community-mod-installer.dmg"),
             "windows": ("stfc-community-mod", "version.dll")}
    name, filename = names[platform]
    candidates = [a for a in artifacts["artifacts"] if a.get("name") == name and a.get("expired") is False]
    if len(candidates) != 1:
        raise ValueError(f"Expected one retained {name} artifact; rebuild if absent or ambiguous")
    artifact = candidates[0]
    artifact_id = positive(artifact.get("id"))
    origin = artifact.get("workflow_run", {})
    repo_id = run["repository"].get("id")
    if (str(origin.get("id")) != run_id or origin.get("head_sha") != sha or repo_id is None
            or origin.get("repository_id") != repo_id or origin.get("head_repository_id") != repo_id
            or not isinstance(artifact.get("digest"), str)
            or not re.fullmatch(r"sha256:[0-9a-f]{64}", artifact["digest"])):
        raise ValueError("Artifact origin/digest does not establish the selected repository and run")
    try:
        started = datetime.fromisoformat(run["run_started_at"].replace("Z", "+00:00"))
        created = datetime.fromisoformat(artifact["created_at"].replace("Z", "+00:00"))
        if started.tzinfo is None or created.tzinfo is None or created < started:
            raise ValueError("Artifact predates the selected run attempt")
    except (KeyError, TypeError, AttributeError) as error:
        raise ValueError("Run-attempt and artifact creation timestamps are required") from error
    if expected_artifact and positive(expected_artifact) != artifact_id:
        raise ValueError("Artifact identity changed after selection")
    return {"repository": repository, "run_id": run_id, "run_attempt": attempt,
            "head_sha": sha, "branch": run.get("head_branch"), "event": run["event"],
            "build_status": run["status"], "build_conclusion": run.get("conclusion"), "mode": mode, "platform": platform,
            "artifact_id": artifact_id, "artifact_digest": artifact["digest"],
            "artifact_name": name, "filename": filename,
            "run_started_at": run["run_started_at"], "artifact_created_at": artifact["created_at"],
            "source_kind": "pull-request-run-head" if run["event"] == "pull_request" else "push-run-head"}


def extract(archive, selection, destination):
    archive, destination = Path(archive), Path(destination)
    with archive.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if "sha256:" + digest != selection["artifact_digest"]:
        raise ValueError("Downloaded artifact SHA256 differs from the selected GitHub artifact")
    with zipfile.ZipFile(archive) as package:
        entries = package.infolist()
        if len(entries) != 1 or entries[0].filename != selection["filename"]:
            raise ValueError("Expected only the exact installer/DLL; no paths or extra producer files")
        entry = entries[0]
        mode = entry.external_attr >> 16
        if (entry.is_dir() or stat.S_ISLNK(mode) or (stat.S_IFMT(mode) not in (0, stat.S_IFREG))
                or entry.flag_bits & 1 or not entry.file_size):
            raise ValueError("Expected an unencrypted, nonempty regular installer/DLL")
        destination.mkdir(parents=True, exist_ok=True)
        target = destination / selection["filename"]
        with package.open(entry) as source, target.open("xb") as output:
            shutil.copyfileobj(source, output, 1024 * 1024)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)
    pick = sub.add_parser("select")
    for field in ["run-json", "artifacts-json", "repository", "run-id", "platform", "output"]:
        pick.add_argument("--" + field, required=True)
    pick.add_argument("--mode", choices=["test", "release"], default="test")
    for field in ["expected-sha", "expected-attempt", "expected-artifact", "github-output"]:
        pick.add_argument("--" + field)
    unpack = sub.add_parser("extract")
    for field in ["archive", "selection", "destination"]:
        unpack.add_argument("--" + field, required=True)
    args = parser.parse_args()
    if args.operation == "extract":
        extract(args.archive, json.loads(Path(args.selection).read_text()), args.destination)
        return
    result = select(json.loads(Path(args.run_json).read_text()), json.loads(Path(args.artifacts_json).read_text()),
                    args.repository, args.run_id, args.platform, args.mode, args.expected_sha,
                    args.expected_attempt, args.expected_artifact)
    Path(args.output).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if args.github_output:
        with Path(args.github_output).open("a", encoding="utf-8") as output:
            for key in ["run_id", "run_attempt", "head_sha", "artifact_id", "artifact_digest", "source_kind"]:
                print(f"{args.platform}_{key}={result[key]}", file=output)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
