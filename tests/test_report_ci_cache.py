#!/usr/bin/env python3
"""Local diagnostic checks; not registered in CI."""

import importlib.util
from pathlib import Path
import unittest


spec = importlib.util.spec_from_file_location(
    "cache_report", Path(__file__).resolve().parents[1] / "scripts/report-ci-cache.py"
)
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


class CacheReportTests(unittest.TestCase):
    def test_restore_failure_with_published_primary_key_is_not_a_miss(self):
        states = {
            "failure": {"outcome": "failure", "outputs": {"cache-primary-key": "wanted"}},
            "cancelled": {"outcome": "cancelled", "outputs": {"cache-primary-key": "wanted"}},
            "skipped": {"outcome": "skipped", "outputs": {}},
            "unknown": None,
            "exact": {"outcome": "success", "outputs": {"cache-hit": "true", "cache-primary-key": "wanted", "cache-matched-key": "wanted"}},
            "fallback": {"outcome": "success", "outputs": {"cache-hit": "false", "cache-primary-key": "wanted", "cache-matched-key": "old"}},
            "empty": {"outcome": "success", "outputs": {"cache-primary-key": "wanted"}},
        }
        text = report.render_report({"name": "job", "steps": []}, [], states, {})
        for name, label in (("failure", "failure"), ("cancelled", "cancelled"), ("skipped", "skipped"),
                            ("unknown", "unavailable"), ("exact", "exact hit"), ("fallback", "fallback hit"),
                            ("empty", "no restore")):
            self.assertIn(f"| {name} | {label} |", text)
        self.assertNotIn("| miss |", text)

    def test_timing_and_inventory_preserve_failure_and_scope_evidence(self):
        job = {"name": "job|name", "steps": [
            {"name": "Configure", "status": "completed", "conclusion": "failure",
             "started_at": "2026-10-06T01:00:00Z", "completed_at": "2026-10-06T01:00:05Z"},
            {"name": "Build", "status": "skipped", "conclusion": "skipped", "started_at": None, "completed_at": None},
        ]}
        caches = [{"key": "wanted", "ref": "refs/pull/1/merge", "size_in_bytes": 1048576}]
        restores = {"failed": {"outcome": "failure", "outputs": {"cache-primary-key": "wanted"}}}
        text = report.render_report(job, caches, restores, {"missing": None})
        self.assertIn("| Configure | 5.0 | failure |", text)
        self.assertIn("| Build | unavailable | skipped |", text)
        self.assertIn("refs/pull/1/merge", text)
        self.assertIn("| **Total** | **1.0** |", text)
        self.assertIn("job\\|name", text)
        self.assertIn("| missing | unavailable |", text)


if __name__ == "__main__":
    unittest.main()
