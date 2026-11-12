#!/usr/bin/env python3
"""Portable metadata/promotion checks; these do not establish Finder or signing behavior."""

import hashlib
import importlib.util
import json
import plistlib
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "scripts/mac-build-identity.py"
SPEC = importlib.util.spec_from_file_location("mac_build_identity", SCRIPT)
IDENTITY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(IDENTITY)
SOURCE_SPEC = importlib.util.spec_from_file_location("ci_signing_input", ROOT / "scripts/ci-signing-input.py")
SIGNING = importlib.util.module_from_spec(SOURCE_SPEC)
SOURCE_SPEC.loader.exec_module(SIGNING)
BASE = "1.1.9.5"
TAG = "v1.1.9.5-guffa.6"
SOURCE = "0123456789abcdef0123456789abcdef01234567"


def icon(red):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 16, 16, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress((b"\0" + bytes([red, 90, 20, 255]) * 16) * 16)) + chunk(b"IEND", b""))
    element = b"icp4" + struct.pack(">I", len(png) + 8) + png
    return b"icns" + struct.pack(">I", len(element) + 8) + element


def template():
    return {
        "CFBundleIdentifier": "com.stfcmod.startrekpatch", "CFBundleExecutable": "macOSLauncher",
        "CFBundleName": 'Research & Command "Test"', "CFBundleDisplayName": "Waffle <Mod>",
        "CFBundleIconFile": "launcher", "CFBundleShortVersionString": "${VERSION}",
        "CFBundleVersion": "${VERSION}", "LSMinimumSystemVersion": "${MACOS_MIN}",
        "Unrelated": {"Preserve": ["a", 42, True]},
    }


class IdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="stfc-mac-identity-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.app = self.root / "Test.app"
        self.contents = self.app / "Contents"
        (self.contents / "MacOS").mkdir(parents=True)
        (self.contents / "Resources").mkdir()
        self.plist = self.contents / "Info.plist"
        self.icons = self.root / "trusted-icons"
        self.icons.mkdir()
        self.receipt = self.root / "stamp.json"
        for number, channel in enumerate(("ci", "dev", "pre", "release")):
            (self.icons / f"launcher-{channel}.icns").write_bytes(icon(30 + 50 * number))
        self.output_icon = self.contents / "Resources/launcher.icns"
        self.output_icon.write_bytes(icon(4))
        self.binaries = {
            "MacOS/macOSLauncher": b"launcher Mach-O sentinel\0\x80",
            "stfc-community-mod-loader": b"loader Mach-O sentinel\0\x81",
            "libstfc-community-mod.dylib": b"mod Mach-O sentinel\0\x82",
        }
        for name, payload in self.binaries.items():
            (self.contents / name).write_bytes(payload)
        initial = template()
        initial.update(CFBundleShortVersionString=BASE, CFBundleVersion=BASE, LSMinimumSystemVersion="14.6")
        self.plist.write_bytes(plistlib.dumps(initial))
        self.input_template = self.root / "template.plist"
        self.input_template.write_bytes(plistlib.dumps(template()))

    def snapshot(self):
        return {str(path.relative_to(self.root)): path.read_bytes()
                for path in self.root.rglob("*") if path.is_file() and not path.is_symlink()}

    def stamp(self, channel="release", **arguments):
        return IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt, channel=channel,
                                  release_tag=None if channel in ("ci", "dev") else TAG, **arguments)

    def generated(self, **identity):
        output = self.root / "generated.plist"
        IDENTITY.generate(self.input_template, output, base_version=BASE, macos_min="14.6", **identity)
        return plistlib.loads(output.read_bytes())

    def test_release_and_pre_are_explicit_despite_identical_tag(self):
        release = IDENTITY.build_metadata(BASE, channel="release", release_tag=TAG)
        pre = IDENTITY.build_metadata(BASE, channel="pre", release_tag=TAG)
        self.assertEqual(release["STFCDisplayVersion"], "1.1.9.5-guffa.6 · Release")
        self.assertEqual(pre["STFCDisplayVersion"], "1.1.9.5-guffa.6 · Pre")
        self.assertEqual(release["CFBundleVersion"], "5.6")
        self.assertEqual(pre["CFBundleVersion"], "5.6")
        self.assertEqual(release["CFBundleShortVersionString"], "1.1.9")

    def test_pr_pre_stamp_retains_the_merge_source_for_the_loaded_mod(self):
        head = "f" * 40
        initial = self.generated(source=SOURCE, run_id=123, run_number=137, run_attempt=2)
        self.plist.write_bytes(plistlib.dumps(initial))
        verified = SIGNING.verify_source({"head_sha": head, "event": "pull_request"},
                                         initial["STFCSourceCommit"],
                                         {"sha": SOURCE, "parents": [{"sha": head}]})
        IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt, channel="pre",
                           source=verified["source_commit"], run_id=123, run_number=137, run_attempt=2)
        result = plistlib.loads(self.plist.read_bytes())
        self.assertEqual(result["STFCDisplayVersion"], "1.1.9.5 · Pre b137.2")
        self.assertEqual(result["STFCSourceCommit"], SOURCE)
        self.assertNotEqual(result["STFCSourceCommit"], head)
        for name, payload in self.binaries.items():
            self.assertEqual((self.contents / name).read_bytes(), payload)

    def test_ci_and_local_keep_numeric_versions_separate_from_label(self):
        ci = IDENTITY.build_metadata(BASE, source=SOURCE.upper(), run_id=42576047391, run_number=137, run_attempt=2)
        local = IDENTITY.build_metadata(BASE)
        self.assertEqual(ci["STFCDisplayVersion"], "1.1.9.5 · CI b137.2")
        self.assertEqual(ci["CFBundleVersion"], "5.137.2")
        self.assertEqual(ci["STFCSourceCommit"], SOURCE)
        self.assertEqual(ci["STFCBuildRunID"], "42576047391")
        self.assertEqual(local["STFCDisplayVersion"], "1.1.9.5 · CI local")
        self.assertEqual(local["CFBundleVersion"], "5.0.1")

    def test_known_fork_lineage_survives_ci_dev_and_unnamed_pre_labels(self):
        local = IDENTITY.build_metadata(BASE, fork_revision=6)
        ci = IDENTITY.build_metadata(BASE, fork_revision="6", run_number=137, run_attempt=2)
        pre = IDENTITY.build_metadata(BASE, channel="pre", fork_revision=6, source=SOURCE,
                                      run_id=123, run_number=137, run_attempt=2)
        self.assertEqual(local["STFCDisplayVersion"], "1.1.9.5-guffa.6 · CI local")
        self.assertEqual(ci["STFCDisplayVersion"], "1.1.9.5-guffa.6 · CI b137.2")
        self.assertEqual(pre["STFCDisplayVersion"], "1.1.9.5-guffa.6 · Pre b137.2")
        self.assertEqual(local["STFCBaseVersion"], BASE)
        self.assertEqual(local["CFBundleShortVersionString"], "1.1.9")
        self.assertEqual(ci["CFBundleVersion"], "5.137.2")
        self.assertEqual(pre["CFBundleVersion"], "5.137.2")
        for fields in (local, ci, pre):
            self.assertEqual(fields["STFCForkRevision"], 6)
            self.assertEqual(fields["STFCReleaseTag"], "")

    def test_dev_requires_explicit_channel_and_keeps_its_distinct_identity(self):
        arguments = dict(fork_revision=6, run_number=137, run_attempt=2)
        ci = IDENTITY.build_metadata(BASE, **arguments)
        dev = IDENTITY.build_metadata(BASE, channel="dev", **arguments)
        local = IDENTITY.build_metadata(BASE, channel="dev", fork_revision=6)
        self.assertEqual(ci["STFCBuildChannel"], "ci")
        self.assertEqual(dev["STFCBuildChannel"], "dev")
        self.assertEqual(dev["STFCDisplayVersion"], "1.1.9.5-guffa.6 · DEV b137.2")
        self.assertEqual(local["STFCDisplayVersion"], "1.1.9.5-guffa.6 · DEV local")
        self.assertEqual(ci["CFBundleVersion"], dev["CFBundleVersion"])
        self.assertEqual(ci["STFCForkRevision"], dev["STFCForkRevision"])

    def test_named_dev_tag_is_exact_and_cannot_claim_another_channel(self):
        tag = "v1.1.9.5-guffa.6-dev.1"
        fields = IDENTITY.build_metadata(BASE, channel="dev", release_tag=tag, fork_revision=6)
        self.assertEqual(fields["STFCDisplayVersion"], "1.1.9.5-guffa.6-dev.1 · DEV")
        self.assertEqual(fields["CFBundleVersion"], "5.6.1")
        self.assertEqual(fields["STFCReleaseTag"], tag)
        for invalid in ("v1.1.9.5-guffa.6", "v1.1.9.5-guffa.7-dev.1", "v1.1.9.0-guffa.6-dev.1",
                        "v1.1.9.5-guffa.6-dev.0", "v1.1.9.5-guffa.6-dev.01", tag + "\n"):
            with self.subTest(tag=invalid), self.assertRaises(ValueError):
                IDENTITY.build_metadata(BASE, channel="dev", release_tag=invalid, fork_revision=6)
        for channel in ("ci", "pre", "release"):
            with self.subTest(channel=channel), self.assertRaises(ValueError):
                IDENTITY.build_metadata(BASE, channel=channel, release_tag=tag, fork_revision=6)

    def test_unsigned_ci_and_dev_select_their_own_icons_without_payload_changes(self):
        for channel in ("ci", "dev"):
            with self.subTest(channel=channel):
                initial = self.generated(channel=channel, fork_revision=6, source=SOURCE)
                self.plist.write_bytes(plistlib.dumps(initial))
                record = IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt,
                                            channel=channel, source=SOURCE)
                self.assertEqual(record["identity"]["STFCBuildChannel"], channel)
                self.assertEqual(record["identity"]["STFCForkRevision"], 6)
                self.assertEqual(record["selectedIcon"], f"launcher-{channel}.icns")
                self.assertEqual(self.output_icon.read_bytes(), (self.icons / f"launcher-{channel}.icns").read_bytes())
                IDENTITY.validate_plist(plistlib.loads(self.plist.read_bytes()))
                for name, payload in self.binaries.items():
                    self.assertEqual((self.contents / name).read_bytes(), payload)

    def test_unnamed_stamp_retains_producer_lineage_and_refuses_controller_override(self):
        initial = self.generated(fork_revision=6)
        self.assertEqual(initial["STFCBuildChannel"], "ci")
        self.plist.write_bytes(plistlib.dumps(initial))
        self.output_icon.write_bytes((self.icons / "launcher-ci.icns").read_bytes())
        before = self.snapshot()
        arguments = dict(channel="pre", source=SOURCE, run_id=123, run_number=137, run_attempt=2)
        with self.assertRaisesRegex(ValueError, "producing app's fork revision"):
            IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt,
                               fork_revision=7, **arguments)
        self.assertEqual(self.snapshot(), before)
        record = IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt, **arguments)
        self.assertEqual(record["identity"]["STFCForkRevision"], 6)
        self.assertEqual(record["identity"]["STFCDisplayVersion"], "1.1.9.5-guffa.6 · Pre b137.2")
        self.assertEqual(record["selectedIcon"], "launcher-pre.icns")
        self.assertEqual(self.output_icon.read_bytes(), (self.icons / "launcher-pre.icns").read_bytes())
        for name, payload in self.binaries.items():
            self.assertEqual((self.contents / name).read_bytes(), payload)

    def test_named_promotion_uses_the_planned_tag_revision_independently_of_baseline(self):
        self.plist.write_bytes(plistlib.dumps(self.generated(fork_revision=6)))
        record = IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt,
                                    channel="release", release_tag="v1.1.9.5-guffa.7", fork_revision=6)
        self.assertEqual(record["identity"]["STFCForkRevision"], 7)
        self.assertEqual(record["identity"]["STFCDisplayVersion"], "1.1.9.5-guffa.7 · Release")
        self.assertEqual(record["identity"]["CFBundleVersion"], "5.7")

    def test_legacy_unnamed_promotion_keeps_unknown_lineage_instead_of_inventing_one(self):
        record = IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt,
                                    channel="pre", source=SOURCE, run_id=123, run_number=137)
        self.assertEqual(record["identity"]["STFCForkRevision"], 0)
        self.assertEqual(record["identity"]["STFCDisplayVersion"], "1.1.9.5 · Pre b137.1")

    def test_unnamed_pre_requires_exact_ci_context_and_keeps_candidate_label(self):
        pre = IDENTITY.build_metadata(BASE, channel="pre", source=SOURCE,
                                      run_id=42576047391, run_number=137, run_attempt=2)
        self.assertEqual(pre["STFCDisplayVersion"], "1.1.9.5 · Pre b137.2")
        self.assertEqual(pre["CFBundleVersion"], "5.137.2")
        self.assertEqual(pre["STFCReleaseTag"], "")
        self.assertEqual(pre["STFCBuildRunID"], "42576047391")
        for incomplete in (
            {}, {"source": SOURCE}, {"source": SOURCE, "run_id": 10},
            {"run_id": 10, "run_number": 137}, {"source": SOURCE, "run_number": 137},
            {"source": SOURCE, "run_id": 10, "run_number": 0},
            {"source": SOURCE, "run_id": 10, "run_number": 137, "run_attempt": 0},
        ):
            with self.subTest(incomplete=incomplete), self.assertRaises(ValueError):
                IDENTITY.build_metadata(BASE, channel="pre", **incomplete)

    def test_unnamed_pre_generation_and_stamp_preserve_payload_and_refuse_partial_context(self):
        output = self.generated(channel="pre", source=SOURCE, fork_revision=6,
                                run_id=99, run_number=19, run_attempt=3)
        self.assertEqual(output["STFCDisplayVersion"], "1.1.9.5-guffa.6 · Pre b19.3")
        IDENTITY.validate_plist(output)
        verified = subprocess.run(
            [sys.executable, str(ROOT / "scripts/verify-version-substitution.py"),
             str(self.root / "generated.plist"), "--base-version", BASE, "--channel", "pre", "--source", SOURCE],
            capture_output=True, text=True,
        )
        self.assertEqual(verified.returncode, 0, verified.stderr)
        self.plist.write_bytes(plistlib.dumps(output))
        before = self.snapshot()
        with self.assertRaises(ValueError):
            IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt,
                               channel="pre", source=SOURCE, run_id=99)
        self.assertEqual(self.snapshot(), before)
        record = IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt,
                                    channel="pre", source=SOURCE, run_id=99, run_number=19, run_attempt=3)
        self.assertEqual(record["identity"]["STFCDisplayVersion"], "1.1.9.5-guffa.6 · Pre b19.3")
        self.assertEqual(self.output_icon.read_bytes(), (self.icons / "launcher-pre.icns").read_bytes())
        for name, payload in self.binaries.items():
            self.assertEqual((self.contents / name).read_bytes(), payload)

    def test_invalid_identity_inputs_are_rejected(self):
        invalid = [
            {"channel": "release"}, {"channel": "pre", "release_tag": "v1.1.9.5-guffa.0"},
            {"channel": "release", "release_tag": "v1.1.9.0-guffa.6"},
            {"channel": "release", "release_tag": "v1.1.9.5-guffa.6;echo nope"},
            {"channel": "release", "release_tag": "v1.1.9.5-guffa.6\n"},
            {"channel": "dev", "release_tag": TAG}, {"channel": "ci", "release_tag": TAG},
            {"channel": "PR"}, {"source": "abc123"},
            {"run_id": 0}, {"run_number": -1}, {"run_attempt": 0}, {"run_number": True},
            {"run_number": "01"}, {"run_number": 2**63},
            {"fork_revision": -1}, {"fork_revision": True}, {"fork_revision": "06"},
            {"fork_revision": ""}, {"fork_revision": 2**63},
            {"channel": "release", "release_tag": f"v{BASE}-guffa.{2**63}"},
        ]
        for arguments in invalid:
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                IDENTITY.build_metadata(BASE, **arguments)
        for base in ("1.1.9", "1.1.9.05", "1.1.9.5<xml>", "1.1.9.5\n"):
            with self.subTest(base=base), self.assertRaises(ValueError):
                IDENTITY.build_metadata(base)

    def test_generate_escapes_xml_and_preserves_unrelated_values(self):
        output = self.generated(channel="release", release_tag=TAG, source=SOURCE)
        self.assertEqual(output["CFBundleName"], 'Research & Command "Test"')
        self.assertEqual(output["CFBundleDisplayName"], "Waffle <Mod>")
        self.assertEqual(output["Unrelated"], {"Preserve": ["a", 42, True]})
        self.assertEqual(output["CFBundleGetInfoString"], "1.1.9.5-guffa.6 · Release")
        self.assertEqual(output["LSMinimumSystemVersion"], "14.6")
        IDENTITY.validate_plist(output)

    def test_unknown_template_placeholder_and_template_overwrite_are_rejected(self):
        bad = template()
        bad["Unknown"] = ["${NOT_A_SUPPORTED_FIELD}"]
        self.input_template.write_bytes(plistlib.dumps(bad))
        before = self.snapshot()
        with self.assertRaises(ValueError):
            self.generated()
        self.assertEqual(self.snapshot(), before)
        self.input_template.write_bytes(plistlib.dumps(template()))
        with self.assertRaises(ValueError):
            IDENTITY.generate(self.input_template, self.input_template, base_version=BASE, macos_min="14.6")

    def test_legacy_promotion_changes_only_identity_and_selected_icon(self):
        before = plistlib.loads(self.plist.read_bytes())
        old_plist = self.plist.read_bytes()
        old_icon = self.output_icon.read_bytes()
        record = self.stamp("pre", source=SOURCE, run_id=99, run_number=10)
        after = plistlib.loads(self.plist.read_bytes())
        self.assertEqual(after["STFCDisplayVersion"], "1.1.9.5-guffa.6 · Pre")
        for key, value in before.items():
            if key not in IDENTITY.IDENTITY_KEYS:
                self.assertEqual(after[key], value)
        self.assertEqual(self.output_icon.read_bytes(), (self.icons / "launcher-pre.icns").read_bytes())
        self.assertEqual(record["files"]["Info.plist"]["beforeSha256"], hashlib.sha256(old_plist).hexdigest())
        self.assertEqual(record["files"]["Resources/launcher.icns"]["beforeSha256"], hashlib.sha256(old_icon).hexdigest())
        for name, payload in self.binaries.items():
            self.assertEqual((self.contents / name).read_bytes(), payload)
            self.assertEqual(record["binarySha256"][name], hashlib.sha256(payload).hexdigest())

    def test_new_dev_bundle_can_be_promoted_and_release_is_not_pre(self):
        self.plist.write_bytes(plistlib.dumps(self.generated(channel="dev", run_number=17)))
        self.stamp("release")
        current = plistlib.loads(self.plist.read_bytes())
        self.assertEqual(current["STFCBuildChannel"], "release")
        self.assertTrue(current["STFCDisplayVersion"].endswith(" · Release"))

    def test_stamp_is_idempotent_including_original_receipt(self):
        first = self.stamp(source=SOURCE)
        before = self.snapshot()
        second = self.stamp(source=SOURCE)
        self.assertEqual(second, first)
        self.assertEqual(self.snapshot(), before)

    def test_missing_or_malformed_selected_icon_cannot_partially_stamp(self):
        selected = self.icons / "launcher-release.icns"
        selected.rename(self.icons / "parked.icns")
        before = self.snapshot()
        with self.assertRaises(ValueError):
            self.stamp()
        self.assertEqual(self.snapshot(), before)
        selected.write_bytes(b"not an icon")
        before = self.snapshot()
        with self.assertRaises(ValueError):
            self.stamp()
        self.assertEqual(self.snapshot(), before)

    def test_bad_existing_metadata_and_invalid_requested_tag_do_not_mutate(self):
        bad = self.generated()
        bad["STFCBuildRunNumber"] = False
        self.plist.write_bytes(plistlib.dumps(bad))
        before = self.snapshot()
        with self.assertRaises(ValueError):
            self.stamp()
        self.assertEqual(self.snapshot(), before)
        self.plist.write_bytes(plistlib.dumps(self.generated()))
        before = self.snapshot()
        with self.assertRaises(ValueError):
            IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.receipt,
                               channel="pre", release_tag="v1.1.8.0-guffa.9")
        self.assertEqual(self.snapshot(), before)

    def test_missing_binary_or_conflicting_receipt_path_does_not_mutate(self):
        binary = self.contents / "stfc-community-mod-loader"
        binary.rename(self.contents / "parked-loader")
        before = self.snapshot()
        with self.assertRaises(ValueError):
            self.stamp()
        self.assertEqual(self.snapshot(), before)
        (self.contents / "parked-loader").rename(binary)
        before = self.snapshot()
        with self.assertRaises(ValueError):
            IDENTITY.stamp_app(self.app, icons_dir=self.icons, receipt=self.plist,
                               channel="release", release_tag=TAG)
        self.assertEqual(self.snapshot(), before)

    def test_unrelated_or_malformed_receipt_is_not_overwritten(self):
        for payload in (b"private unrelated text", b'{"schema":"other"}'):
            with self.subTest(payload=payload):
                self.receipt.write_bytes(payload)
                before = self.snapshot()
                with self.assertRaises(ValueError):
                    self.stamp()
                self.assertEqual(self.snapshot(), before)

    def test_symlink_inputs_are_rejected_without_following_targets(self):
        link = self.root / "link.app"
        try:
            link.symlink_to(self.app, target_is_directory=True)
        except OSError as error:
            self.skipTest(f"symlink creation unavailable: {error}")
        before = self.snapshot()
        with self.assertRaises(ValueError):
            IDENTITY.stamp_app(link, icons_dir=self.icons, receipt=self.receipt, channel="release", release_tag=TAG)
        self.assertEqual(self.snapshot(), before)
        selected = self.icons / "launcher-release.icns"
        selected.rename(self.icons / "real-release.icns")
        selected.symlink_to(self.icons / "real-release.icns")
        before = self.snapshot()
        with self.assertRaises(ValueError):
            self.stamp()
        self.assertEqual(self.snapshot(), before)

    def test_cli_generate_and_verifier_accept_default_ci_and_reject_mismatch(self):
        output = self.root / "cli.plist"
        command = [sys.executable, str(SCRIPT), "generate", "--template", str(self.input_template),
                   "--base-version", BASE, "--macos-min", "14.6", "--run-id", "55", "--run-number", "19",
                   "--source", SOURCE, "--fork-revision", "6", "--output", str(output)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CI b19.1", result.stdout)
        self.assertIn("1.1.9.5-guffa.6", result.stdout)
        verify = [sys.executable, str(ROOT / "scripts/verify-version-substitution.py"), str(output),
                  "--base-version", BASE, "--channel", "ci", "--source", SOURCE]
        result = subprocess.run(verify, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run(verify + ["--release-tag", TAG], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("STFCReleaseTag", result.stderr)
        plist = plistlib.loads(output.read_bytes())
        plist["CFBundleVersion"] = "1.1.9.5"
        output.write_bytes(plistlib.dumps(plist))
        result = subprocess.run(verify, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("CFBundleVersion", result.stderr)

    def test_describe_cli_produces_same_pre_identity_without_app_or_icon_inputs(self):
        output = self.root / "identity.json"
        result = subprocess.run(
            [sys.executable, str(SCRIPT), "describe", "--base-version", BASE, "--channel", "pre",
             "--source", SOURCE, "--run-id", "55", "--run-number", "19", "--fork-revision", "6",
             "--output", str(output)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        fields = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(fields, IDENTITY.build_metadata(BASE, channel="pre", source=SOURCE, run_id=55,
                                                         run_number=19, fork_revision=6))
        self.assertEqual(fields["STFCDisplayVersion"], "1.1.9.5-guffa.6 · Pre b19.1")
        self.assertIn("Pre b19.1", result.stdout)


if __name__ == "__main__":
    unittest.main()
