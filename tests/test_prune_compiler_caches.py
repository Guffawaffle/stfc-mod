#!/usr/bin/env python3
"""Local safety checks for compiler-cache retention; not registered in CI."""

import importlib.util
import json
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location(
    "cache_policy", Path(__file__).resolve().parents[1] / "scripts/prune-compiler-caches.py"
)
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


def snapshot(cache_id, key, ref="refs/heads/play", created="2026-10-06T01:00:00Z"):
    return {"id": cache_id, "key": key, "ref": ref, "created_at": created, "size_in_bytes": 100}


class CachePolicyTests(unittest.TestCase):
    def test_preserves_foundations_other_scopes_and_latest_each_family(self):
        prefixes = [
            "windows-x64-release-sccache-v0.18.0-v1-image-",
            "macos-arm64-release-protobuf-sccache-v0.18.0-v1-toolchain-",
            "macos-x86_64-release-protobuf-sccache-v0.18.0-v1-toolchain-",
            "macos-arm64-release-v4-",
            "macos-x86_64-release-v4-",
        ]
        caches = []
        obsolete_ids = set()
        for index, prefix in enumerate(prefixes):
            old_id = index * 10 + 1
            obsolete_ids.add(old_id)
            caches.extend([
                snapshot(old_id, prefix + "a" * 40),
                snapshot(old_id + 1, prefix + "b" * 40, created="2026-10-06T02:00:00Z"),
                snapshot(old_id + 2, prefix + "a" * 40, ref="refs/heads/play-dev"),
                snapshot(old_id + 3, prefix + "a" * 40, ref="refs/pull/345/merge"),
            ])
        for index, key in enumerate([
            "xmake-tool-Windows-X64-3.1.1-v4",
            "windows-x64-release-xmake-3.1.1-v4-" + "a" * 64,
            "macos-arm64-release-xmake-local-3.1.1-v4-" + "a" * 64,
            "macos-arm64-release-swift-modules-v2-" + "a" * 40,
            "macos-arm64-release-v4-not-a-commit",
        ]):
            caches.append(snapshot(100 + index, key))
        self.assertEqual({cache["id"] for cache in policy.select_obsolete(caches, "refs/heads/play")},
                         obsolete_ids)

    def test_new_windows_schema_replaces_old_snapshot(self):
        old = snapshot(1, "windows-x64-release-sccache-v0.18.0-v1-image-" + "a" * 40)
        new = snapshot(2, "windows-x64-release-sccache-v0.18.0-v2-image-" + "b" * 40,
                       created="2026-10-06T02:00:00Z")
        self.assertEqual(policy.select_obsolete([old, new], "refs/heads/play"), [old])

    def test_unsupported_ref_rejected(self):
        for ref in ("refs/pull/345/merge", "refs/tags/v1", "play", "refs/heads/feature"):
            with self.subTest(ref=ref), self.assertRaises(ValueError):
                policy.select_obsolete([], ref)

    def test_paginates_listing_and_keeps_ref_query_encoded(self):
        first = [snapshot(index, "unrelated") for index in range(100)]
        last = [snapshot(101, "unrelated")]
        responses = [subprocess.CompletedProcess([], 0, json.dumps({"actions_caches": page}))
                     for page in (first, last)]
        with patch.object(policy.subprocess, "run", side_effect=responses) as run:
            self.assertEqual(len(policy.list_caches(policy.REPOSITORY, "refs/heads/play")), 101)
            self.assertIn("ref=refs%2Fheads%2Fplay", run.call_args_list[0].args[0][-1])
            self.assertIn("page=2", run.call_args_list[1].args[0][-1])

    def test_overlapping_pages_never_select_duplicate_keeper(self):
        old = snapshot(499, "macos-arm64-release-v4-" + "a" * 40)
        keeper = snapshot(500, "macos-arm64-release-v4-" + "b" * 40,
                          created="2026-10-06T02:00:00Z")
        first = [snapshot(index, "unrelated") for index in range(99)] + [keeper]
        responses = [subprocess.CompletedProcess([], 0, json.dumps({"actions_caches": page}))
                     for page in (first, [keeper, old])]
        with patch.object(policy.subprocess, "run", side_effect=responses) as run:
            caches = policy.list_caches(policy.REPOSITORY, "refs/heads/play")
            self.assertIn("sort=created_at&direction=desc", run.call_args_list[0].args[0][-1])
        self.assertEqual(sum(cache["id"] == keeper["id"] for cache in caches), 1)
        self.assertEqual(policy.select_obsolete(caches, "refs/heads/play"), [old])
        self.assertEqual(policy.select_obsolete([keeper, keeper, old], "refs/heads/play"), [old])

    def test_default_is_dry_run_and_api_failure_does_not_delete(self):
        old = snapshot(1, "macos-arm64-release-v4-" + "a" * 40)
        new = snapshot(2, "macos-arm64-release-v4-" + "b" * 40, created="2026-10-06T02:00:00Z")
        argv = ["prune", "--repo", policy.REPOSITORY, "--ref", "refs/heads/play"]
        with patch("sys.argv", argv), patch.object(policy, "list_caches", return_value=[old, new]), \
                patch.object(policy.subprocess, "run") as run:
            policy.main()
            run.assert_not_called()
        with patch("sys.argv", argv + ["--apply"]), \
                patch.object(policy, "list_caches", side_effect=subprocess.CalledProcessError(1, "gh")), \
                patch.object(policy.subprocess, "run") as run:
            with self.assertRaises(subprocess.CalledProcessError):
                policy.main()
            run.assert_not_called()

    def test_apply_deletes_only_selected_id_and_rejects_other_repository(self):
        old = snapshot(1, "macos-arm64-release-v4-" + "a" * 40)
        new = snapshot(2, "macos-arm64-release-v4-" + "b" * 40, created="2026-10-06T02:00:00Z")
        other = snapshot(3, old["key"], ref="refs/heads/play-dev")
        argv = ["prune", "--repo", policy.REPOSITORY, "--ref", "refs/heads/play", "--apply"]
        with patch("sys.argv", argv), patch.object(policy, "list_caches", return_value=[old, new, other]), \
                patch.object(policy.subprocess, "run") as run:
            policy.main()
            run.assert_called_once_with(
                ["gh", "api", "--method", "DELETE", f"repos/{policy.REPOSITORY}/actions/caches/1"],
                check=True,
            )
        argv[2] = "STFC-Mod/mod"
        with patch("sys.argv", argv), patch.object(policy, "list_caches") as listing:
            with self.assertRaises(SystemExit):
                policy.main()
            listing.assert_not_called()


if __name__ == "__main__":
    unittest.main()
