import importlib.util
import json
import tempfile
import threading
import time
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("claim_capture", REPO / "scripts/claim-capture.py")
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


class CaptureTests(unittest.TestCase):
    def setUp(self):
        private = REPO / ".codex/claims"
        private.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="test-", dir=private)
        self.root = Path(self.temporary.name).resolve()
        assert self.root.is_relative_to(private.resolve())
        self.status = dict(session="current", utc_ms=int(time.time() * 1000), healthy=True,
                           summary_bound=True, summary_generation=7)
        self.write_status()

    def tearDown(self):
        # The generated absolute cleanup target was checked inside this workspace.
        self.temporary.cleanup()

    def write_status(self):
        (self.root / "community_claims_status.json").write_text(json.dumps(self.status))

    def test_old_session_is_not_capture_health(self):
        self.status["utc_ms"] -= 90000
        self.write_status()
        with self.assertRaisesRegex(RuntimeError, "stale"):
            capture.request(self.root, "status", "")
        self.assertFalse((self.root / "community_claims_control.json").exists())

    def test_new_layout_selects_newest_even_if_unhealthy(self):
        nested = self.root / "science/claims"
        nested.mkdir(parents=True)
        status = dict(self.status, utc_ms=self.status["utc_ms"] + 1, healthy=False)
        (nested / "community_claims_status.json").write_text(json.dumps(status))
        self.assertEqual(capture.capture_root(self.root), nested)
        self.assertEqual(capture.capture_root(nested), nested)

    def test_stale_new_layout_does_not_hide_live_legacy_build(self):
        nested = self.root / "science/claims"
        nested.mkdir(parents=True)
        status = dict(self.status, utc_ms=self.status["utc_ms"] - 90000)
        (nested / "community_claims_status.json").write_text(json.dumps(status))
        self.assertEqual(capture.capture_root(self.root), self.root)

    def test_snapshot_layout_without_status(self):
        (self.root / "community_claims_status.json").unlink()
        nested = self.root / "science/claims"
        nested.mkdir(parents=True)
        (nested / "community_claims.jsonl").write_text('{}\n')
        self.assertEqual(capture.capture_root(self.root), nested)

    def test_injection_requires_healthy_bound_summary(self):
        for field in ("healthy", "summary_bound"):
            self.status[field] = False
            self.write_status()
            with self.assertRaisesRegex(RuntimeError, "healthy capture"):
                capture.request(self.root, "reject-summary-transition-once", "")
            self.assertFalse((self.root / "community_claims_control.json").exists())
            self.status[field] = True

    def test_presentation_escape_requires_healthy_current_stall(self):
        self.status.update(navigation_recovery_enabled=True,
                           presentation_watch=dict(stalled=True, escape_attempted=False, local_claim_key=25))
        for field in ("healthy", "navigation_recovery_enabled"):
            self.status[field] = False
            self.write_status()
            with self.assertRaisesRegex(RuntimeError, "healthy stalled claim"):
                capture.request(self.root, "escape-presentation", "")
            self.status[field] = True
        for field, value in (("stalled", False), ("escape_attempted", True), ("local_claim_key", 0)):
            original = self.status["presentation_watch"][field]
            self.status["presentation_watch"][field] = value
            self.write_status()
            with self.assertRaisesRegex(RuntimeError, "healthy stalled claim"):
                capture.request(self.root, "escape-presentation", "")
            self.status["presentation_watch"][field] = original
        self.assertFalse((self.root / "community_claims_control.json").exists())

    def test_snapshot_preserves_item_receipts_and_unity_exception_log(self):
        (self.root / "community_claims.jsonl").write_text('{}\n')
        (self.root / "claim_receipts.jsonl").write_text('{"phase":"purchase_orders"}\n')
        unity = self.root / "Unity-original.log"
        unity.write_text("NullReferenceException: ShopCinematicDirector\n")
        receipt = capture.snapshot(self.root, self.root / "incident", player_log=unity)
        names = {item["name"] for item in receipt["files"]}
        self.assertIn("claim_receipts.jsonl", names)
        self.assertIn("Player.log", names)
        self.assertEqual((self.root / "incident/Player.log").read_text(), unity.read_text())

    def test_escape_command_binds_the_observed_claim_key(self):
        self.status.update(navigation_recovery_enabled=True,
                           presentation_watch=dict(stalled=True, escape_attempted=False, local_claim_key=25))
        self.write_status()
        observed = []

        def runtime():
            command = self.root / "community_claims_control.json"
            deadline = time.monotonic() + 2
            while not command.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            request = json.loads(command.read_text())
            observed.append(request)
            reply = dict(id=request["id"], session=request["session"], ok=True, status=self.status)
            (self.root / "community_claims_reply.json").write_text(json.dumps(reply))

        worker = threading.Thread(target=runtime)
        worker.start()
        capture.request(self.root, "escape-presentation", "")
        worker.join(timeout=3)
        self.assertEqual(observed[0]["claim_key"], 25)
        self.assertEqual(observed[0]["session"], "current")

    def test_pending_command_is_preserved(self):
        path = self.root / "community_claims_control.json"
        path.write_text('{"id":"another-controller"}')
        with self.assertRaisesRegex(RuntimeError, "pending"):
            capture.request(self.root, "status", "")
        self.assertEqual(json.loads(path.read_text())["id"], "another-controller")

    def test_acknowledgement_is_unique_even_when_marker_reused(self):
        path = self.root / "community_claims_reply.json"
        path.write_text(json.dumps(dict(id="repro", session="current", ok=True)))
        observed = []

        def runtime():
            deadline = time.monotonic() + 2
            command = self.root / "community_claims_control.json"
            while not command.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            request = json.loads(command.read_text())
            observed.append(request)
            reply = dict(id=request["id"], session=request["session"], ok=True, status=self.status)
            temporary = path.with_suffix(".tmp")
            temporary.write_text(json.dumps(reply))
            temporary.replace(path)

        worker = threading.Thread(target=runtime)
        worker.start()
        reply = capture.request(self.root, "mark", "repro")
        worker.join(timeout=3)
        self.assertNotEqual(reply["id"], "repro")
        self.assertEqual(observed[0]["label"], "repro")
        self.assertEqual(observed[0]["summary_generation"], 7)

    def test_preserve_rotations_and_explicit_live_acknowledgement(self):
        for name in ("community_claims.jsonl", "community_claims.1.jsonl", "community_claims.2.jsonl"):
            (self.root / name).write_text('{"session":"older"}\n')
        receipt = capture.snapshot(self.root, self.root / "incident")
        self.assertIsNone(receipt["live_acknowledgement"])
        traces = [item for item in receipt["files"] if item["name"].endswith("jsonl")]
        self.assertEqual(len(traces), 3)
        self.assertTrue(all(not item["source_changed_during_copy"] for item in traces))
        self.assertTrue(all(len(item["sha256"]) == 64 for item in traces))


if __name__ == "__main__":
    unittest.main()
