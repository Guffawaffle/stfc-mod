#!/usr/bin/env python3
"""Stream a CI build, retrying only transient Zig dependency HTTP failures."""

import os
import re
import signal
import subprocess
import sys
import time


TRANSIENT_FETCH = re.compile(
    rb"(?:[^\r\n]*[/\\])?build\.zig\.zon:\d+:\d+: error: (?:bad HTTP response code: "
    rb"'(?:408|429|500|502|503|504)(?: [^'\r\n]*)?'|invalid HTTP response: HttpConnectionClosing)"
)
OTHER_FAILURE = re.compile(rb"\berror:|\bfail(?:ed|ure)?\b", re.IGNORECASE)
RETRY_DELAYS = (10, 20)


class Diagnostics:
    def __init__(self):
        self.pending = b""
        self.transient = False
        self.other_failure = False

    def line(self, line):
        if TRANSIENT_FETCH.fullmatch(line.rstrip(b"\r")):
            self.transient = True
        elif OTHER_FAILURE.search(line):
            self.other_failure = True

    def feed(self, chunk):
        lines = (self.pending + chunk).split(b"\n")
        self.pending = lines.pop()
        for line in lines:
            self.line(line)
        # Do not retain unbounded output or guess at an incomplete diagnostic.
        if len(self.pending) > 65536:
            self.other_failure = True
            self.pending = b""

    def retryable(self):
        self.line(self.pending)
        return self.transient and not self.other_failure


def run(command, *, popen=subprocess.Popen, sleep=time.sleep, output=None, notes=None):
    output = sys.stdout.buffer if output is None else output
    notes = sys.stderr if notes is None else notes
    for attempt in range(len(RETRY_DELAYS) + 1):
        diagnostics = Diagnostics()
        with popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0) as child:
            try:
                while chunk := child.stdout.read(65536):
                    output.write(chunk)
                    output.flush()
                    diagnostics.feed(chunk)
                status = child.wait()
            except BaseException:
                if child.poll() is None:
                    child.terminate()
                    try:
                        child.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        child.kill()
                        child.wait()
                raise
        # Negative statuses are POSIX signals; 128+ includes shell cancellation
        # statuses and Windows exception statuses such as CTRL_C_EXIT.
        if not 0 < status < 128 or not diagnostics.retryable() or attempt == len(RETRY_DELAYS):
            return status
        delay = RETRY_DELAYS[attempt]
        print(
            f"ci-build-retry: transient Zig dependency HTTP failure; "
            f"retrying attempt {attempt + 2}/{len(RETRY_DELAYS) + 1} in {delay}s.",
            file=notes, flush=True,
        )
        sleep(delay)


def main():
    command = sys.argv[1:]
    if command[:1] == ["--"]:
        command = command[1:]
    if not command:
        print("usage: ci-build-retry.py [--] command [args ...]", file=sys.stderr)
        return 2

    def terminate(signum, frame):
        raise SystemExit(128 + signum)

    signal.signal(signal.SIGTERM, terminate)
    try:
        return run(command)
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    result = main()
    if result < 0 and os.name != "nt":
        # Preserve child signal termination for the caller, not a wrapped code.
        if -result not in (signal.SIGKILL, signal.SIGSTOP):
            signal.signal(-result, signal.SIG_DFL)
        os.kill(os.getpid(), -result)
    if os.name == "nt" and result > 0x7FFFFFFF:
        result -= 0x100000000
    sys.exit(result)
