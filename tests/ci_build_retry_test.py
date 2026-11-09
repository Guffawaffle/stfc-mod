"""Local CI retry fixtures; no network, compiler, game or external dependencies."""

import io
import os
from pathlib import Path
import runpy
import signal
import subprocess
import sys
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/ci-build-retry.py"
API = runpy.run_path(str(SCRIPT))
FETCH = b"D:\\a\\stfc-mod\\stfc-mod\\build.zig.zon:86:20: error: bad HTTP response code: '504 Gateway Timeout'\r\n"


class FixtureProcess:
    def __init__(self, output, status):
        class ShortReads(io.BytesIO):
            def read(self, size):
                return super().read(min(size, 13))

        self.stdout = ShortReads(output)
        self.status = status

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.stdout.close()

    def wait(self, timeout=None):
        return self.status


class BuildRetryTests(unittest.TestCase):
    def invoke(self, attempts, *, sleep=None):
        calls, delays = [], []
        output, notes = io.BytesIO(), io.StringIO()
        fixtures = iter(attempts)

        def popen(command, **kwargs):
            calls.append(list(command))
            return FixtureProcess(*next(fixtures))

        status = API["run"](
            ["zig", "build", "test-mod", "-Dtarget=x86_64-windows-msvc"],
            popen=popen, sleep=delays.append if sleep is None else sleep, output=output, notes=notes,
        )
        return status, calls, delays, output.getvalue(), notes.getvalue()

    def test_transient_fetch_then_success_keeps_command_and_output(self):
        result = self.invoke([(FETCH, 1), (b"tests passed\n", 0)])
        status, calls, delays, output, notes = result
        self.assertEqual(status, 0)
        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[0], calls[1])
        self.assertEqual(delays, [10])
        self.assertEqual(output, FETCH + b"tests passed\n")
        self.assertIn("attempt 2/3 in 10s", notes)

    def test_transient_fetch_exhaustion_preserves_last_failure(self):
        status, calls, delays, output, notes = self.invoke([(FETCH, 1), (FETCH, 2), (FETCH, 7)])
        self.assertEqual(status, 7)
        self.assertEqual(len(calls), 3)
        self.assertEqual(delays, [10, 20])
        self.assertEqual(output, FETCH * 3)
        self.assertEqual(notes.count("retrying"), 2)

    def test_supported_transient_statuses_and_manifest_paths(self):
        for code in (408, 429, 500, 502, 503, 504):
            for path in (b"build.zig.zon", b"/Users/runner/work/stfc-mod/stfc-mod/build.zig.zon"):
                with self.subTest(code=code, path=path):
                    failure = path + b":71:20: error: bad HTTP response code: '" + str(code).encode() + b" HTTP reason'"
                    status, calls, *_ = self.invoke([(failure, 1), (b"", 0)])
                    self.assertEqual((status, len(calls)), (0, 2))

    def test_real_failures_and_mixed_diagnostics_do_not_retry(self):
        failures = (
            FETCH.replace(b"504 Gateway Timeout", b"404 Not Found"),
            b"src/mod/config.cc:35:2: error: use of undeclared identifier 'foo'\n",
            b"error: 'sync.test upload' failed: TestUnexpectedResult\n",
            b"1/2 sync.test upload...FAIL (TestUnexpectedResult)\n",
            FETCH.replace(b"build.zig.zon", b"source.cc"),
        )
        for failure in failures:
            for output in (failure, FETCH + failure):
                with self.subTest(output=output):
                    status, calls, delays, actual, notes = self.invoke([(output, 1)])
                    self.assertEqual((status, len(calls), delays), (1, 1, []))
                    self.assertEqual(actual, output)
                    self.assertEqual(notes, "")

    def test_compile_failure_after_fetch_retry_stops_immediately(self):
        compiler = b"src/mod/config.cc:35:2: error: use of undeclared identifier 'foo'\n"
        status, calls, delays, output, _ = self.invoke([(FETCH, 1), (compiler, 5)])
        self.assertEqual((status, len(calls), delays), (5, 2, [10]))
        self.assertEqual(output, FETCH + compiler)

    def test_cancellation_statuses_do_not_retry(self):
        for code in (-signal.SIGINT, -signal.SIGTERM, 130, 143, -1073741510, 0xC000013A):
            with self.subTest(code=code):
                status, calls, delays, *_ = self.invoke([(FETCH, code)])
                self.assertEqual((status, len(calls), delays), (code, 1, []))

    def test_cancellation_during_delay_does_not_launch_another_attempt(self):
        def cancel(delay):
            raise KeyboardInterrupt

        with self.assertRaises(KeyboardInterrupt):
            self.invoke([(FETCH, 1)], sleep=cancel)

    def test_streams_partial_binary_stdout_and_stderr_before_child_exits(self):
        process = None

        def popen(command, **kwargs):
            nonlocal process
            process = subprocess.Popen(command, stdin=subprocess.PIPE, **kwargs)
            return process

        class StreamingOutput(io.BytesIO):
            sent = False

            def flush(self):
                if not self.sent:
                    process.stdin.write(b"received\n")
                    process.stdin.flush()
                    self.sent = True

        output = StreamingOutput()
        child = (
            "import os, sys, threading; "
            "timeout = threading.Timer(5, lambda: os._exit(99)); timeout.start(); "
            "os.write(1, b'partial stdout \\xff'); os.write(2, b' and stderr \\xfe'); "
            "sys.stdin.buffer.readline(); timeout.cancel(); sys.exit(17)"
        )
        status = API["run"]([sys.executable, "-c", child], popen=popen, output=output, notes=io.StringIO())
        self.assertEqual(status, 17)
        self.assertEqual(output.getvalue(), b"partial stdout \xff and stderr \xfe")

    def test_cli_preserves_failure_status_and_literal_arguments(self):
        argument = "literal spaces; $variable $(command)"
        result = subprocess.run(
            [sys.executable, str(SCRIPT), "--", sys.executable, "-c",
             "import sys; print(sys.argv[1]); sys.exit(23)", argument],
            capture_output=True,
        )
        self.assertEqual(result.returncode, 23)
        self.assertEqual(result.stdout.decode().strip(), argument)
        self.assertEqual(result.stderr, b"")

    @unittest.skipIf(os.name == "nt", "POSIX signal return codes")
    def test_cli_preserves_child_signal_termination(self):
        for signum in (signal.SIGTERM, signal.SIGKILL):
            with self.subTest(signum=signum):
                result = subprocess.run(
                    [sys.executable, str(SCRIPT), sys.executable, "-c", f"import os; os.kill(os.getpid(), {signum})"],
                    capture_output=True,
                )
                self.assertEqual(result.returncode, -signum)

    @unittest.skipUnless(os.name == "nt", "Windows exception return codes")
    def test_cli_preserves_windows_ctrl_c_status(self):
        result = subprocess.run(
            [sys.executable, str(SCRIPT), sys.executable, "-c", "import os; os._exit(-1073741510)"],
            capture_output=True,
        )
        self.assertEqual(result.returncode, 0xC000013A)


if __name__ == "__main__":
    unittest.main()
