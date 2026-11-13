import copy
import hashlib
import importlib.util
import pathlib
import stat
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("signing", pathlib.Path(__file__).parents[1] / "scripts/ci-signing-input.py")
signing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(signing)


class SigningInput(unittest.TestCase):
    def setUp(self):
        self.run = {"id": 123, "run_attempt": 2, "repository": {"id": 9, "full_name": "owner/mod"},
                    "head_repository": {"full_name": "owner/mod"}, "head_sha": "a" * 40,
                    "head_branch": "feature/presets", "event": "push", "path": ".github/workflows/ci.yaml",
                    "status": "completed", "conclusion": "success", "run_started_at": "2026-10-31T00:00:00Z"}
        self.artifact = {"id": 456, "name": "stfc-community-mod-installer", "expired": False,
                         "digest": "sha256:" + "b" * 64, "created_at": "2026-10-31T00:01:00Z",
                         "workflow_run": {"id": 123, "head_sha": "a" * 40, "repository_id": 9,
                                          "head_repository_id": 9}}

    def pick(self, **kwargs):
        return signing.select(self.run, {"artifacts": [self.artifact]}, "owner/mod", "123", "macos", **kwargs)

    def test_branch_independent_and_exact_selection(self):
        r = self.pick(expected_sha="a" * 40, expected_attempt="2", expected_artifact="456")
        self.assertEqual(r["artifact_id"], "456")
        for event in ["workflow_dispatch", "pull_request"]:
            self.run["event"] = event
            self.assertEqual(self.pick()["event"], event)

    def test_producing_checkout_is_not_replaced_by_pr_run_head(self):
        head, merge = "a" * 40, "c" * 40
        self.assertEqual(signing.verify_source(self.pick(), head)["source_relationship"], "run-head")
        self.run["event"] = "pull_request"
        commit = {"sha": merge, "parents": [{"sha": "d" * 40}, {"sha": head}]}
        result = signing.verify_source(self.pick(), merge, commit)
        self.assertEqual(result, {"source_commit": merge, "build_run_head_sha": head,
                                  "source_relationship": "pull-request-merge-parent"})
        for event in ("push", "workflow_dispatch"):
            self.run["event"] = event
            with self.assertRaisesRegex(ValueError, "differs"):
                signing.verify_source(self.pick(), merge, commit)

    def test_pr_source_requires_exact_verified_commit_and_direct_parent(self):
        self.run["event"] = "pull_request"
        for commit in (None, {}, {"sha": "c" * 40}, {"sha": "c" * 40, "parents": []},
                       {"sha": "d" * 40, "parents": [{"sha": "a" * 40}]},
                       {"sha": "c" * 40, "parents": [{"sha": "b" * 40}]},
                       {"sha": "c" * 40, "parents": [None, "a" * 40]}):
            with self.assertRaisesRegex(ValueError, "direct parent"):
                signing.verify_source(self.pick(), "c" * 40, commit)
        for source in ("", "c" * 8, "C" * 40, "c" * 40 + "\n", None):
            with self.assertRaises(ValueError):
                signing.verify_source(self.pick(), source)

    def test_test_signing_does_not_claim_failed_ci_passed(self):
        self.run["conclusion"] = "failure"
        self.assertEqual(self.pick()["build_conclusion"], "failure")
        with self.assertRaises(ValueError): self.pick(mode="release")

    def test_existing_artifact_does_not_wait_for_unrelated_jobs(self):
        self.run.update(status="in_progress", conclusion=None, head_branch="play")
        result = self.pick()
        self.assertEqual(result["build_status"], "in_progress")
        self.assertIsNone(result["build_conclusion"])
        with self.assertRaises(ValueError): self.pick(mode="release")

    def test_release_stays_play_success_push_only(self):
        with self.assertRaises(ValueError): self.pick(mode="release")
        self.run["head_branch"] = "play"
        self.assertEqual(self.pick(mode="release")["mode"], "release")
        self.run["event"] = "pull_request"
        with self.assertRaises(ValueError): self.pick(mode="release")

    def test_changed_attempt_sha_artifact_refused(self):
        for args in [{"expected_attempt": "1"}, {"expected_sha": "c" * 40}, {"expected_artifact": "987"}]:
            with self.assertRaises(ValueError): self.pick(**args)

    def test_run_repository_workflow_and_status_refused(self):
        for key, value in [("id", 999), ("head_sha", "bad"), ("path", "other.yaml"),
                           ("event", "pull_request_target"), ("status", "queued"),
                           ("head_repository", {"full_name": "fork/mod"})]:
            original = copy.deepcopy(self.run)
            self.run[key] = value
            with self.assertRaises(ValueError): self.pick()
            self.run = original

    def test_artifact_identity_expiry_and_ambiguity(self):
        for key, value in [("expired", True), ("digest", None), ("workflow_run", {}),
                           ("created_at", "2026-10-30T00:00:00Z")]:
            original = copy.deepcopy(self.artifact)
            self.artifact[key] = value
            with self.assertRaises((ValueError, TypeError)): self.pick()
            self.artifact = original
        with self.assertRaises(ValueError):
            signing.select(self.run, {"artifacts": [self.artifact, self.artifact]}, "owner/mod", 123, "macos")

    def test_extract_only_exact_payload_with_verified_archive_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            for name in ["stfc-community-mod-installer.dmg", "../escape", "nested/stfc-community-mod-installer.dmg"]:
                archive = root / "input.zip"
                with zipfile.ZipFile(archive, "w") as z: z.writestr(name, b"payload")
                self.artifact["digest"] = "sha256:" + hashlib.sha256(archive.read_bytes()).hexdigest()
                selection = self.pick()
                if name == selection["filename"]:
                    signing.extract(archive, selection, root / "out")
                    self.assertEqual((root / "out" / name).read_bytes(), b"payload")
                else:
                    with self.assertRaises(ValueError): signing.extract(archive, selection, root / "bad")
                selection["artifact_digest"] = "sha256:" + "0" * 64
                with self.assertRaises(ValueError): signing.extract(archive, selection, root / "bad")
            self.assertFalse((root.parent / "escape").exists())

    def test_symlink_payload_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            info = zipfile.ZipInfo("stfc-community-mod-installer.dmg")
            info.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive = root / "input.zip"
            with zipfile.ZipFile(archive, "w") as z: z.writestr(info, "outside")
            self.artifact["digest"] = "sha256:" + hashlib.sha256(archive.read_bytes()).hexdigest()
            with self.assertRaises(ValueError): signing.extract(archive, self.pick(), root / "out")


if __name__ == "__main__": unittest.main()
