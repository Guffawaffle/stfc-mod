#!/usr/bin/env python3
"""Run only the synthetic sync fixture against an owned ephemeral loopback listener."""
import hashlib
import http.server
import json
import os
import pathlib
import secrets
import subprocess
import sys
import threading


def main():
    if len(sys.argv) != 2:
        raise SystemExit("expected the built sync-upload-tests executable")
    executable = pathlib.Path(sys.argv[1]).resolve(strict=True)
    if executable.name not in ("sync-upload-tests", "sync-upload-tests.exe"):
        raise SystemExit("only the named headless fixture is accepted")
    key = secrets.token_hex(16)
    lock = threading.Lock()
    release = threading.Event()
    records = []
    counters = {"fixture": key, "retry": {"ordinary": 0, "diagnostic": 0},
                "blocked": {"ordinary": 0, "diagnostic": 0, "entered": False}}

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def setup(self):
            super().setup()
            self.connection.settimeout(5)

        def log_message(self, *_):
            pass

        def answer(self, status, value):
            body = json.dumps(value).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            self.close_connection = True

        def allowed(self):
            return self.client_address[0] == "127.0.0.1" and self.headers.get("stfc-sync-token") == key

        def do_GET(self):
            if not self.allowed() or self.path != "/control/state":
                self.answer(403, {})
                return
            with lock:
                snapshot = json.loads(json.dumps(counters))
            self.answer(200, snapshot)

        def do_POST(self):
            if not self.allowed():
                self.answer(403, {})
                return
            if self.path == "/control/release":
                release.set()
                self.answer(200, {})
                return
            if self.path not in ("/retry", "/blocked"):
                self.answer(404, {})
                return
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 1024 * 1024:
                self.answer(413, {})
                return
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, list) or len(body) != 1:
                self.answer(400, {})
                return
            diagnostic = body[0].get("type") == "diagnostic"
            lane = "diagnostic" if diagnostic else "ordinary"
            path = self.path[1:]
            with lock:
                if len(records) >= 128:
                    self.answer(429, {})
                    return
                counters[path][lane] += 1
                ordinal = counters[path][lane]
                records.append({"path": path, "lane": lane, "body": body,
                                "first_sync": self.headers.get("X-PRIME-SYNC")})
                if path == "blocked" and not diagnostic:
                    counters[path]["entered"] = True
            if path == "blocked" and not diagnostic and not release.wait(10):
                self.answer(504, {})
                return
            status = 500 if path == "retry" and (diagnostic or ordinal == 1) else 200
            self.answer(status, {})

    class Server(http.server.ThreadingHTTPServer):
        daemon_threads = False
        block_on_close = True

    server = Server(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.05})
    thread.start()
    child = None
    try:
        url = f"http://127.0.0.1:{server.server_port}"
        environment = os.environ.copy()
        for name in list(environment):
            if name.lower() in ("http_proxy", "https_proxy", "all_proxy", "no_proxy"):
                del environment[name]
        environment["NO_PROXY"] = "*"
        environment["no_proxy"] = "*"
        child = subprocess.Popen([str(executable), url, key], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True, env=environment)
        stdout, stderr = child.communicate(timeout=45)
        sys.stdout.write(stdout)
        sys.stderr.write(stderr)
        if child.returncode:
            raise RuntimeError(f"headless sync fixture exited {child.returncode}")
        with lock:
            captured = list(records)
        ordinary = [r for r in captured if r["path"] == "retry" and r["lane"] == "ordinary"]
        diagnostic = [r for r in captured if r["path"] == "retry" and r["lane"] == "diagnostic"]
        if len(ordinary) != 3 or len(diagnostic) != 1:
            raise RuntimeError("ordinary retry or diagnostic best-effort count differs")
        if ordinary[0]["first_sync"] != "2" or ordinary[1]["first_sync"] != "2":
            raise RuntimeError("retry lost initial-sync header")
        if ordinary[2]["first_sync"] is not None or diagnostic[0]["first_sync"] is not None:
            raise RuntimeError("initial-sync header leaked into delta/diagnostic")
        if ordinary[0]["body"][0]["amount"] != 10 or ordinary[2]["body"][0]["amount"] != 11:
            raise RuntimeError("ordinary wire observations changed")
        tail = [r for r in captured if r["path"] == "blocked" and r["lane"] == "diagnostic"]
        if len(tail) != 1 or tail[0]["body"][0]["sequence"] != "2":
            raise RuntimeError("diagnostic tail did not survive ordinary seal")
        print("Owned127.0.0.1 listener verified retry, headers, best-effort drop and sealed-response fence; "
              f"fixture_sha256={hashlib.sha256(executable.read_bytes()).hexdigest()}")
        print(f"fixture_path={executable}")
    finally:
        release.set()
        if child is not None and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
        if thread.is_alive():
            raise RuntimeError("fixture listener did not stop")


if __name__ == "__main__":
    main()
