#!/usr/bin/env python3
"""Archive the passive selection journal outside the game, without deleting evidence."""

import argparse
import contextlib
import gzip
import json
import os
import re
import shutil
import time
from pathlib import Path


KIND = "stfc-bulk-selection-archive"
MAX_LINE = 8 * 1024 * 1024


def atomic_json(path, value):
    temporary = path.with_suffix(".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


@contextlib.contextmanager
def exclusive(root):
    """One collector per destination; the OS releases the lock after a crash."""
    root.mkdir(parents=True, exist_ok=True)
    with (root / "collector.lock").open("a+b") as stream:
        stream.seek(0)
        if not stream.read(1):
            stream.write(b"0")
            stream.flush()
        stream.seek(0)
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        try:
            yield
        finally:
            stream.seek(0)
            if os.name == "nt":
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream, fcntl.LOCK_UN)


class CapacityError(RuntimeError):
    pass


class Archive:
    def __init__(self, source, root, client, max_bytes, reserve_bytes):
        self.source, self.root = source.resolve(), root.resolve()
        self.client, self.max_bytes, self.reserve_bytes = client, max_bytes, reserve_bytes
        self.offsets = {}
        self.state_path = self.root / "archive_state.json"
        if self.state_path.exists():
            self.state = json.loads(self.state_path.read_text(encoding="utf-8"))
            if (self.state.get("schema") != 1 or self.state.get("kind") != KIND
                    or self.state.get("source") != str(self.source) or self.state.get("client") != client):
                raise ValueError("Destination belongs to another source/client or archive format")
        else:
            if list(self.root.glob("batch-*.jsonl.gz")):
                raise ValueError("Archive batches exist without their state; preserve and investigate them")
            self.state = dict(schema=1, kind=KIND, source=str(self.source), client=client,
                              last_batch=0, archived_bytes=0, records=0, sessions={}, gaps=[])
            atomic_json(self.state_path, self.state)
        # A crash after publishing a batch but before its checkpoint must neither
        # duplicate that batch nor discard its records when the collector restarts.
        pending = sorted(self.root.glob("batch-*.jsonl.gz"))
        for path in pending:
            number = int(path.name.split("-")[1].split(".")[0])
            if number <= self.state["last_batch"]:
                continue
            if number != self.state["last_batch"] + 1:
                raise ValueError("Archive batch sequence has a gap")
            with gzip.open(path, "rt", encoding="utf-8") as stream:
                for line in stream:
                    self.advance(json.loads(line))
            self.state["last_batch"] = number
            self.state["archived_bytes"] += path.stat().st_size
            atomic_json(self.state_path, self.state)

    def advance(self, event):
        session, seq = event["session"], event["seq"]
        previous = self.state["sessions"].get(session, 0)
        if seq <= previous:
            raise ValueError("Archive batch contains repeated or out-of-order records")
        if seq > previous + 1:
            self.state["gaps"].append(dict(session=session, first_missing=previous + 1,
                                           last_missing=seq - 1))
        self.state["sessions"][session] = seq
        self.state["records"] += 1

    def publish(self, events):
        raw = b"".join((json.dumps(dict(event, capture_client=self.client), separators=(",", ":")) + "\n")
                       .encode("utf-8") for event in events)
        compressed = gzip.compress(raw, mtime=0)
        if self.state["archived_bytes"] + len(compressed) > self.max_bytes:
            raise CapacityError("Archive budget reached; evidence retained, collection paused")
        if shutil.disk_usage(self.root).free - len(compressed) < self.reserve_bytes:
            raise CapacityError("Free-space reserve reached; evidence retained, collection paused")
        number = self.state["last_batch"] + 1
        path = self.root / f"batch-{number:012d}.jsonl.gz"
        if path.exists():
            raise ValueError("Uncheckpointed batch requires collector restart/recovery")
        temporary = path.with_suffix(".tmp")
        with temporary.open("wb") as stream:
            stream.write(compressed)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        for event in events:
            self.advance(event)
        self.state["last_batch"] = number
        self.state["archived_bytes"] += len(compressed)
        self.state["last_event_utc_ms"] = events[-1].get("utc_ms")
        atomic_json(self.state_path, self.state)

    def poll(self):
        """Read complete records by file identity, tolerating native log rotation."""
        offsets = {}
        watermarks = dict(self.state["sessions"])
        batch, size, added = [], 0, 0
        for index in (4, 3, 2, 1, 0):
            path = self.source / (f"bulk_selection.{index}.jsonl" if index else "bulk_selection.jsonl")
            try:
                stream = path.open("rb")
            except FileNotFoundError:
                continue
            with stream:
                stat = os.fstat(stream.fileno())
                identity = (stat.st_dev, stat.st_ino)
                position = self.offsets.get(identity, 0)
                stream.seek(position if position <= stat.st_size else 0)
                while True:
                    start = stream.tell()
                    line = stream.readline(MAX_LINE + 1)
                    if len(line) > MAX_LINE:
                        raise ValueError("Source record exceeds the 8 MiB limit")
                    if not line.endswith(b"\n"):
                        stream.seek(start)
                        break
                    if not line.strip():
                        continue
                    event = json.loads(line)
                    session, seq = event.get("session"), event.get("seq")
                    if (event.get("schema") != 1 or not isinstance(session, str)
                            or not re.fullmatch(r"[0-9_]{1,64}", session)
                            or type(seq) is not int or seq < 1):
                        raise ValueError("Unsupported source record identity")
                    if seq <= watermarks.get(session, 0):
                        continue
                    watermarks[session] = seq
                    batch.append(event)
                    size += len(line)
                    if size >= 4 * 1024 * 1024:
                        self.publish(batch)
                        added += len(batch)
                        batch, size = [], 0
                offsets[identity] = stream.tell()
        if batch:
            self.publish(batch)
            added += len(batch)
        self.offsets = offsets
        return added

    def status(self, state="running", message=None):
        atomic_json(self.root / "collector_status.json", dict(
            schema=1, state=state, message=message, pid=os.getpid(), utc_ms=int(time.time() * 1000),
            source=str(self.source), client=self.client, records=self.state["records"],
            batches=self.state["last_batch"], archived_bytes=self.state["archived_bytes"],
            max_bytes=self.max_bytes, free_space_reserve_bytes=self.reserve_bytes,
            known_gaps=len(self.state["gaps"]), account_scope="unverified; label identifies client only"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, required=True)
    parser.add_argument("--archive-dir", type=Path, required=True)
    parser.add_argument("--client", required=True, help="Stable client label, not an account identifier")
    parser.add_argument("--max-gib", type=float, default=50)
    parser.add_argument("--reserve-gib", type=float, default=10)
    parser.add_argument("--interval", type=float, default=30)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    if not (0 < args.max_gib <= 100000 and 0 <= args.reserve_gib <= 100000 and 1 <= args.interval <= 300):
        parser.error("Invalid budget, reserve, or polling interval")
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,64}", args.client):
        parser.error("Client label must contain only letters, digits, dots, underscores or hyphens")
    source = args.game_dir.resolve(strict=True)
    if (source / "science/claims").is_dir():
        source = source / "science/claims"
    with exclusive(args.archive_dir):
        archive = Archive(source, args.archive_dir, args.client,
                          int(args.max_gib * 1024**3), int(args.reserve_gib * 1024**3))
        while True:
            try:
                archive.poll()
                archive.status("idle" if args.once else "running")
            except CapacityError as error:
                archive.status("paused", str(error))
                if args.once:
                    return 2
            except Exception as error:
                archive.status("error", str(error))
                raise
            if args.once:
                return 0
            time.sleep(args.interval)


if __name__ == "__main__":
    raise SystemExit(main())
