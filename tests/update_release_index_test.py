"""Portable feed migration checks; publication is verified against isolated metadata."""

import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location("update_release_index", ROOT / "scripts/update-release-index.py")
INDEX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INDEX)
LEGACY = {"schema": 1, "channels": {"release": "v1.1.9.5-guffa.11", "pre": "v1.1.9.5-guffa.7",
                                     "dev": "v1.1.9.5-guffa.7-dev.5"}}


class ReleaseIndex(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="stfc-release-index-")
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / "update-index-v2.json"
        self.path.write_text(json.dumps(LEGACY), encoding="utf-8")

    def test_canonical_tags_match_exactly_their_channel(self):
        for tag, channel in (("v2.2.0", "release"), ("v2.2.0-rc.1", "pre"), ("v2.2.0-dev.1", "dev"),
                             ("v3.0.0", "release"), ("v65535.65535.65535", "release")):
            self.assertTrue(INDEX.version(tag, channel))
            for other in set(INDEX.CHANNELS) - {channel}:
                with self.subTest(tag=tag, channel=other), self.assertRaises(ValueError):
                    INDEX.version(tag, other)
        for tag in ("v1.2.0", "2.2.0", "v02.2.0", "v2.2.0.0", "v2.2.0-rc.0", "v2.2.0-rc.01",
                    "v2.2.0-dev.4294967296", "v4294967296.2.0", "v2.2.0+build.1", "v2.2.0\n"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                INDEX.version(tag, "release" if "-" not in tag else "pre")

    def test_order_is_numeric_and_final_follows_rc_follows_dev(self):
        version = INDEX.version
        self.assertLess(version("v2.2.0-rc.9", "pre"), version("v2.2.0-rc.10", "pre"))
        self.assertLess(version("v2.2.0-dev.4294967295", "dev"), version("v2.2.0-rc.1", "pre"))
        self.assertLess(version("v2.2.0-rc.4294967295", "pre"), version("v2.2.0", "release"))
        self.assertLess(version("v2.2.0", "release"), version("v2.3.0-dev.1", "dev"))
        self.assertLess(version("v1.1.9.5-guffa.4294967295", "release"), version("v2.0.0-dev.1", "dev"))
        self.assertLess(version("v1.1.9.5-guffa.7-dev.9", "dev"),
                        version("v1.1.9.5-guffa.7-dev.10", "dev"))

    def test_default_feed_is_selected_from_tag_family(self):
        for channel, tag in LEGACY["channels"].items():
            self.assertEqual(INDEX.default_index(tag, channel), ROOT / "releases/update-index.json")
        for channel, tag in (("release", "v2.2.0"), ("pre", "v2.2.0-rc.1"), ("dev", "v2.2.0-dev.1")):
            self.assertEqual(INDEX.default_index(tag, channel), ROOT / "releases/update-index-v2.json")

    def test_new_feed_can_advance_each_channel_from_a_legacy_pointer(self):
        expected = copy.deepcopy(LEGACY)
        with patch.object(INDEX, "verify_published") as published:
            for channel, tag in (("release", "v2.2.0"), ("pre", "v2.2.0-rc.2"), ("dev", "v2.2.0-dev.10")):
                INDEX.advance_index(self.path, channel, tag)
                expected["channels"][channel] = tag
                self.assertEqual(json.loads(self.path.read_text()), expected)
                published.assert_called_with(tag, channel)

    def test_new_feed_still_supports_legacy_channel_advancement(self):
        with patch.object(INDEX, "verify_published"):
            INDEX.advance_index(self.path, "pre", "v1.1.9.5-guffa.12")
        self.assertEqual(json.loads(self.path.read_text())["channels"]["pre"], "v1.1.9.5-guffa.12")

    def test_legacy_feed_never_accepts_independent_tags(self):
        legacy = self.path.with_name("update-index.json")
        legacy.write_bytes(self.path.read_bytes())
        before = legacy.read_bytes()
        with patch.object(INDEX, "verify_published") as published, self.assertRaises(ValueError):
            INDEX.advance_index(legacy, "release", "v2.2.0")
        self.assertEqual(legacy.read_bytes(), before)
        published.assert_not_called()
        mixed = copy.deepcopy(LEGACY)
        mixed["channels"]["pre"] = "v2.2.0-rc.1"
        legacy.write_text(json.dumps(mixed))
        with patch.object(INDEX, "verify_published") as published, self.assertRaises(ValueError):
            INDEX.advance_index(legacy, "release", "v1.1.9.5-guffa.12")
        published.assert_not_called()

    def test_regressions_and_unpublished_candidates_preserve_bytes(self):
        with patch.object(INDEX, "verify_published"):
            INDEX.advance_index(self.path, "pre", "v2.2.0-rc.10")
        before = self.path.read_bytes()
        for tag in ("v2.2.0-rc.9", "v2.1.0-rc.99", "v1.1.9.5-guffa.12"):
            with patch.object(INDEX, "verify_published") as published, self.assertRaises(ValueError):
                INDEX.advance_index(self.path, "pre", tag)
            published.assert_not_called()
            self.assertEqual(self.path.read_bytes(), before)
        with patch.object(INDEX, "verify_published", side_effect=ValueError("not published")):
            with self.assertRaises(ValueError):
                INDEX.advance_index(self.path, "pre", "v2.2.0-rc.11")
        self.assertEqual(self.path.read_bytes(), before)

    def test_publication_gate_keeps_tag_channel_assets_and_digest_provenance(self):
        digest = "a" * 64
        for tag, channel in (("v2.2.0", "release"), ("v2.2.0-rc.1", "pre"), ("v2.2.0-dev.1", "dev")):
            manifest_name = "dev-release.json" if channel == "dev" else "release-manifest.json"
            release = {"tag_name": tag, "draft": False, "prerelease": channel != "release", "published_at": "now",
                       "assets": [{"name": name, "state": "uploaded", "size": 1, "digest": f"sha256:{digest}"}
                                  for name in (*INDEX.PLATFORM_ASSETS, manifest_name)]}
            manifest = {"repository": INDEX.REPOSITORY, "tag": tag, "prerelease": channel != "release",
                        "payloadSha256": {name: digest for name in INDEX.PLATFORM_ASSETS}}
            if channel == "dev":
                manifest.update(schema="stfc-dev-release/v1", channel="dev",
                                assets={name: {"sha256": digest} for name in INDEX.PLATFORM_ASSETS})
            with patch.object(INDEX, "fetch_json", side_effect=[release, manifest]):
                INDEX.verify_published(tag, channel)
            manifest["tag"] = "v2.0.0"
            with patch.object(INDEX, "fetch_json", side_effect=[release, manifest]), self.assertRaises(ValueError):
                INDEX.verify_published(tag, channel)
            manifest["tag"] = tag
            release["assets"][0]["digest"] = f"sha256:{'b' * 64}"
            with patch.object(INDEX, "fetch_json", side_effect=[release, manifest]), self.assertRaises(ValueError):
                INDEX.verify_published(tag, channel)


if __name__ == "__main__":
    unittest.main()
