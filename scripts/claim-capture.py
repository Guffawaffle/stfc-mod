#!/usr/bin/env python3
"""Control play-dev claim diagnostics and preserve a private incident capture."""

import argparse
import hashlib
import json
import shutil
import sys
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def capture_root(root):
    """Accept the game directory or an explicit trace directory, including old builds."""
    nested = root / "science/claims"
    candidates = []
    for directory in (nested, root):
        try:
            stamp = read_json(directory / "community_claims_status.json")["utc_ms"]
            candidates.append((int(stamp), directory))
        except (OSError, ValueError, KeyError, TypeError):
            continue
    if candidates:
        # Select the newest heartbeat, including unhealthy captures. A stale new
        # directory must not shadow a live legacy build after a rollback.
        return max(candidates, key=lambda item: item[0])[1]
    return nested if (nested / "community_claims.jsonl").is_file() else root


def request(root, command, label):
    status = read_json(root / "community_claims_status.json")
    age = time.time() - status["utc_ms"] / 1000
    if not 0 <= age <= 65:
        raise RuntimeError("Capture heartbeat is stale; current-session recording is unverified")
    if command == "reject-summary-transition-once" and not (status.get("healthy") and status.get("summary_bound")):
        raise RuntimeError("UI test requires a healthy capture and an open chest summary")
    identifier = str(uuid.uuid4())
    if len(label) > 64:
        raise ValueError("Marker must contain at most 64 characters")
    path = root / "community_claims_control.json"
    if path.exists():
        raise RuntimeError("Another capture command is pending")
    temporary = path.with_suffix("." + identifier + ".tmp")
    try:
        temporary.write_text(json.dumps(dict(id=identifier, session=status["session"], command=command,
                                            label=label, summary_generation=status.get("summary_generation"))), encoding="utf-8")
        # Atomic create-without-replacement: a concurrent caller cannot replace
        # a command that the runtime has not yet acknowledged.
        path.hardlink_to(temporary)
    finally:
        temporary.unlink(missing_ok=True)
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        reply_path = root / "community_claims_reply.json"
        try:
            reply = read_json(reply_path)
        except (FileNotFoundError, json.JSONDecodeError):
            reply = {}
        if reply.get("session") == status["session"] and reply.get("id") == identifier:
            if not reply.get("ok"):
                raise RuntimeError("Capture command rejected: " + str(reply.get("error")))
            return reply
        time.sleep(0.1)
    # The runtime expires every UI test independently after 60 seconds. A timed
    # out acknowledgement is never interpreted as a successful arm or marker.
    raise RuntimeError("No current-session acknowledgement; command outcome is unknown")


def snapshot(root, destination, reply=None):
    destination.mkdir(parents=True, exist_ok=False)
    files = []
    for name in ("community_claims.2.jsonl", "community_claims.1.jsonl", "community_claims.jsonl",
                 "community_claims_status.json", "community_claims_reply.json"):
        source = root / name
        if not source.is_file():
            continue
        before = source.stat()
        target = destination / name
        shutil.copyfile(source, target)
        after = source.stat()
        files.append(dict(name=name, bytes=target.stat().st_size,
                          sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                          source_changed_during_copy=(before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns)))
    if not any(item["name"] == "community_claims.jsonl" for item in files):
        raise RuntimeError("No claim trace found; snapshot is not a current-session capture")
    receipt = dict(captured_at_utc=datetime.now(timezone.utc).isoformat(),
                   live_acknowledgement=reply, files=files,
                   limits="Live copies may include open spans or rotation races; preserve every listed rotation.")
    (destination / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("status", "mark", "snapshot", "arm-ui-failure", "cancel", "preview-notice", "clear-notice"))
    parser.add_argument("--game-dir", type=Path, required=True, help="Game/mod data directory or explicit claims trace directory")
    parser.add_argument("--label", default="")
    parser.add_argument("--output", type=Path, help="Private snapshot destination; default is the checkout's ignored .codex tree")
    args = parser.parse_args()
    try:
        root = capture_root(args.game_dir.resolve(strict=True))
        command = {"arm-ui-failure": "reject-summary-transition-once"}.get(args.action, args.action)
        reply = None if args.action == "snapshot" else request(root, command, args.label)
        result = dict(ok=True, action=args.action, reply=reply)
        if args.action in ("mark", "snapshot"):
            stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
            destination = args.output or Path(__file__).resolve().parents[1] / ".codex/claims/incidents" / stamp
            receipt = snapshot(root, destination, reply)
            result.update(snapshot=str(destination), files=len(receipt["files"]))
        if args.action == "status" and not reply["status"].get("healthy"):
            result["ok"] = False
        print(json.dumps(result, indent=2))
        return 0 if result["ok"] else 1
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(json.dumps(dict(ok=False, error=str(error))))
        return 1


if __name__ == "__main__":
    sys.exit(main())
