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
    modes = {"battle-request", "battle-prepost", "battle-retry", "battle-journal", "battle-profile", "battle-native"}
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] not in modes):
        raise SystemExit("expected the built sync-upload-tests executable and optional local battle case")
    mode = sys.argv[2] if len(sys.argv) == 3 else None
    executable = pathlib.Path(sys.argv[1]).resolve(strict=True)
    if executable.name not in ("sync-upload-tests", "sync-upload-tests.exe"):
        raise SystemExit("only the named headless fixture is accepted")
    key = secrets.token_hex(16)
    lock = threading.Lock()
    release = threading.Event()
    lifecycle_release = {name: threading.Event() for name in ("configure", "retire", "battle-journal", "battle-profile")}
    journal_attempts = {}
    records = []
    counters = {"fixture": key, "retry": {"ordinary": 0, "diagnostic": 0},
                "blocked": {"ordinary": 0, "diagnostic": 0, "entered": False},
                "configure": {"ordinary": 0, "diagnostic": 0, "entered": False},
                "configured": {"ordinary": 0, "diagnostic": 0},
                "retire": {"ordinary": 0, "diagnostic": 0, "entered": False},
                "battle": {"counts": {"journal": 0, "profile": 0, "alliance": 0},
                           "journal_entered": False, "profile_entered": False}}

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
            if self.path in ("/journals/get", "/user_profile/profiles", "/alliance/get_alliances_public_info"):
                self.battle_request()
                return
            if not self.allowed():
                self.answer(403, {})
                return
            if self.path == "/control/release":
                release.set()
                self.answer(200, {})
                return
            if self.path.startswith("/control/release/"):
                event = lifecycle_release.get(self.path.rsplit("/", 1)[-1])
                if event is None:
                    self.answer(404, {})
                else:
                    event.set()
                    self.answer(200, {})
                return
            if self.path not in ("/retry", "/blocked", "/configure", "/configured", "/retire"):
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
                if path in lifecycle_release and not diagnostic and ordinal == 1:
                    counters[path]["entered"] = True
            if path == "blocked" and not diagnostic and not release.wait(10):
                self.answer(504, {})
                return
            if path in lifecycle_release and not diagnostic and ordinal == 1:
                if not lifecycle_release[path].wait(10):
                    self.answer(504, {})
                    return
            status = 500 if path == "retry" and (diagnostic or ordinal == 1) else 200
            self.answer(status, {})

        def battle_request(self):
            if self.client_address[0] != "127.0.0.1" or self.headers.get("X-AUTH-SESSION-ID") != key:
                self.answer(403, {})
                return
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 1024 * 1024:
                self.answer(413, {})
                return
            body = json.loads(self.rfile.read(length))
            lane = {"/journals/get": "journal", "/user_profile/profiles": "profile",
                    "/alliance/get_alliances_public_info": "alliance"}[self.path]
            with lock:
                counters["battle"]["counts"][lane] += 1
                records.append({"path": "battle-" + lane, "lane": "game", "body": body})
            if lane == "journal":
                identity = body.get("journal_id")
                if identity not in (7001, 7101, 7201, 7301):
                    self.answer(400, {})
                    return
                with lock:
                    journal_attempts[identity] = journal_attempts.get(identity, 0) + 1
                    ordinal = journal_attempts[identity]
                    if identity == 7101:
                        counters["battle"]["journal_entered"] = True
                if identity == 7001 and ordinal == 1:
                    self.answer(503, {})
                    return
                if identity == 7101 and not lifecycle_release["battle-journal"].wait(10):
                    self.answer(504, {})
                    return
                self.answer(200, {"journal": {"initiator_id": f"player{identity}", "target_id": "npc_fixture"}})
            elif lane == "profile":
                identities = body.get("user_ids", [])
                if not identities or any(value not in ("player7001", "player7101", "player7201", "player7301")
                                         for value in identities):
                    self.answer(400, {})
                    return
                if "player7201" in identities:
                    with lock:
                        counters["battle"]["profile_entered"] = True
                    if not lifecycle_release["battle-profile"].wait(10):
                        self.answer(504, {})
                        return
                self.answer(200, {"user_profiles": {identity: {"name": "Synthetic Player", "alliance_id": 42}
                                                     for identity in identities}})
            else:
                if body.get("alliance_ids") != [42]:
                    self.answer(400, {})
                    return
                self.answer(200, {"alliances_info": {"42": {"id": 42, "name": "Synthetic Alliance", "tag": "FX"}}})

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
        child = subprocess.Popen([str(executable), url, key] + ([mode] if mode else []), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True, env=environment)
        stdout, stderr = child.communicate(timeout=45)
        sys.stdout.write(stdout)
        sys.stderr.write(stderr)
        if child.returncode:
            raise RuntimeError(f"headless sync fixture exited {child.returncode}")
        with lock:
            captured = list(records)
            battle_counts = dict(counters["battle"]["counts"])
        expected = {"battle-request": (0, 0, 0), "battle-prepost": (0, 0, 0), "battle-retry": (2, 1, 1),
                    "battle-journal": (1, 0, 0), "battle-profile": (1, 1, 0), "battle-native": (4, 2, 1)}
        if tuple(battle_counts[name] for name in ("journal", "profile", "alliance")) != expected[mode or "battle-native"]:
            raise RuntimeError("battle transport admission/removal/re-enable counts differ")
        if mode:
            print(f"Owned127.0.0.1 listener verified {mode}: {battle_counts}; "
                  f"fixture_sha256={hashlib.sha256(executable.read_bytes()).hexdigest()}")
            print(f"fixture_path={executable}")
            return
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
        configured = [r for r in captured if r["path"] in ("configure", "configured")]
        if ([r["path"] for r in configured] != ["configure", "configured"]
                or any(r["first_sync"] != "2" or r["body"][0].get("amount") != 30 for r in configured)):
            raise RuntimeError("configuration change lost its independent unchanged-record baseline")
        retired = [r for r in captured if r["path"] == "retire"]
        if (len(retired) != 3 or [r["first_sync"] for r in retired] != ["2", "2", None]
                or [r["body"][0].get("amount") for r in retired] != [40, 40, 41]):
            raise RuntimeError("retirement/re-add lost baseline isolation or replacement progress")
        print("Owned127.0.0.1 listener verified retry, headers, best-effort drop, sealed-response fence, "
              "reconfiguration and retirement/re-add; "
              f"fixture_sha256={hashlib.sha256(executable.read_bytes()).hexdigest()}")
        print(f"fixture_path={executable}")
    finally:
        release.set()
        for event in lifecycle_release.values():
            event.set()
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
