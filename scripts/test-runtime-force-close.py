#!/usr/bin/env python3
"""Exercise only the compiled CONFIG_RUNTIME_TEST child, never a game process."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time


def main():
    executable = Path(sys.argv[1]).resolve(strict=True)
    if sys.platform != "win32" or executable.name != "runtime-config-tests.exe":
        raise SystemExit("Expected the Windows runtime-config-lifecycle fixture")
    digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    results = []
    for mode in ("idle", "finished", "deadline", "missing-handle"):
        started = time.monotonic()
        # Timeout termination, if needed, is scoped to this exact disposable child.
        child = subprocess.run([str(executable), mode], capture_output=True, timeout=5, check=False)
        elapsed = time.monotonic() - started
        if child.returncode != 1:
            raise SystemExit(f"{mode}: expected fixture self-termination 1, observed {child.returncode}")
        if mode == "deadline" and b"pending cancellation requested" not in child.stdout:
            raise SystemExit("The blocked-caller case never reached its test boundary")
        results.append({"mode": mode, "exit": child.returncode, "wall_seconds": elapsed})
    if hashlib.sha256(executable.read_bytes()).hexdigest() != digest:
        raise SystemExit("Fixture executable changed during qualification")
    print(json.dumps({"fixture": str(executable), "sha256": digest, "cases": results}))


if __name__ == "__main__":
    main()
