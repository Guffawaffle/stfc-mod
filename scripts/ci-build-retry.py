#!/usr/bin/env python3
"""Retry transient Zig dependency fetches, with an optional bounded fetch before the build."""

import os
import re
import signal
import subprocess
import sys
import tempfile
import time


TRANSIENT_FETCH = re.compile(
    rb"(?:[^\r\n]*[/\\])?build\.zig\.zon:\d+:\d+: error: (?:bad HTTP response code: "
    rb"'(?:408|429|500|502|503|504)(?: [^'\r\n]*)?'|invalid HTTP response: HttpConnectionClosing)"
)
OTHER_FAILURE = re.compile(rb"\berror:|\bfail(?:ed|ure)?\b", re.IGNORECASE)
RETRY_DELAYS = (10, 20)
FETCH_TIMEOUT = 120
TIMEOUT_STATUS = 124


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

    def retryable(self, *, timed_out=False):
        self.line(self.pending)
        return (self.transient or timed_out) and not self.other_failure


def relay(stream, output, diagnostics=None):
    while chunk := stream.read(65536):
        output.write(chunk)
        output.flush()
        if diagnostics is not None:
            diagnostics.feed(chunk)


def stream_attempt(command, popen, output, diagnostics=None):
    with popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0) as child:
        try:
            relay(child.stdout, output, diagnostics)
            return child.wait()
        except BaseException:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
            raise


def stop_fetch(child):
    # Zig can spawn a build runner. Reap the attempt before another one touches the same cache.
    status = child.poll()
    if status is not None:
        return status
    if os.name == "nt":
        taskkill = os.path.join(os.environ["SystemRoot"], "System32", "taskkill.exe")
        try:
            subprocess.run([taskkill, "/PID", str(child.pid), "/T", "/F"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
        except (OSError, subprocess.SubprocessError):
            status = child.poll()
            if status is not None:
                return status
            child.kill()
            child.wait(timeout=5)
            raise
    else:
        try:
            os.killpg(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            return child.wait(timeout=10)
    child.wait(timeout=10)
    return None


def wait_for_fetch(child, timeout):
    deadline = time.monotonic() + timeout
    # Short waits let Python handle cancellation promptly on Windows.
    while (remaining := deadline - time.monotonic()) > 0:
        try:
            return child.wait(timeout=min(remaining, 0.25))
        except subprocess.TimeoutExpired:
            pass
    status = child.poll()
    if status is not None:
        return status
    raise subprocess.TimeoutExpired(child.args, timeout)


def fetch_attempt(command, popen, output, diagnostics, timeout):
    # A descendant holding an output pipe open cannot extend the attempt deadline.
    # Keep the fetch diagnostics on disk and replay them as soon as the attempt finishes.
    with tempfile.TemporaryFile() as capture:
        group = (
            {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}
            if os.name == "nt" else {"start_new_session": True}
        )
        child = popen(command, stdout=capture, stderr=subprocess.STDOUT, **group)
        timed_out = False
        try:
            status = wait_for_fetch(child, timeout)
        except subprocess.TimeoutExpired:
            status = stop_fetch(child)
            if status is None:
                status, timed_out = TIMEOUT_STATUS, True
        except BaseException:
            if child.poll() is None:
                stop_fetch(child)
            raise
        finally:
            capture.seek(0)
            relay(capture, output, diagnostics)
        return status, timed_out


def run(command, *, timeout=None, popen=subprocess.Popen, sleep=time.sleep, output=None, notes=None):
    output = sys.stdout.buffer if output is None else output
    notes = sys.stderr if notes is None else notes
    for attempt in range(len(RETRY_DELAYS) + 1):
        diagnostics = Diagnostics()
        if timeout is None:
            status, timed_out = stream_attempt(command, popen, output, diagnostics), False
        else:
            status, timed_out = fetch_attempt(command, popen, output, diagnostics, timeout)
        if timed_out:
            print(f"ci-build-retry: Zig dependency fetch timed out after {timeout:g}s.", file=notes, flush=True)
        # Negative statuses are POSIX signals; 128+ includes shell cancellation
        # statuses and Windows exception statuses such as CTRL_C_EXIT.
        if not 0 < status < 128 or not diagnostics.retryable(timed_out=timed_out) or attempt == len(RETRY_DELAYS):
            return status
        delay = RETRY_DELAYS[attempt]
        reason = "Zig dependency fetch timeout" if timed_out else "transient Zig dependency HTTP failure"
        print(
            f"ci-build-retry: {reason}; "
            f"retrying attempt {attempt + 2}/{len(RETRY_DELAYS) + 1} in {delay}s.",
            file=notes, flush=True,
        )
        sleep(delay)


def run_with_prefetch(command, *, timeout=FETCH_TIMEOUT, popen=subprocess.Popen, sleep=time.sleep, output=None,
                      notes=None):
    if len(command) < 2 or command[1] != "build":
        raise ValueError("--prefetch expects a zig build command")
    output = sys.stdout.buffer if output is None else output
    notes = sys.stderr if notes is None else notes
    print(f"ci-build-retry: fetching Zig dependencies ({timeout:g}s per attempt).", file=notes, flush=True)
    fetch = [*command[:2], "--fetch=needed", *command[2:]]
    status = run(fetch, timeout=timeout, popen=popen, sleep=sleep, output=output, notes=notes)
    return status or stream_attempt(command, popen, output)


def main():
    command = sys.argv[1:]
    prefetch = command[:1] == ["--prefetch"]
    if prefetch:
        command = command[1:]
    if command[:1] == ["--"]:
        command = command[1:]
    if not command:
        print("usage: ci-build-retry.py [--prefetch] [--] command [args ...]", file=sys.stderr)
        return 2

    def terminate(signum, frame):
        raise SystemExit(128 + signum)

    signal.signal(signal.SIGTERM, terminate)
    try:
        return run_with_prefetch(command) if prefetch else run(command)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f"ci-build-retry: {error}", file=sys.stderr)
        return 1
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
