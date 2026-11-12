import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/windows-build-identity.ps1"
FIXTURE = os.environ.get("STFC_WINDOWS_IDENTITY_FIXTURE")
BASE = os.environ.get("STFC_WINDOWS_IDENTITY_BASE", "1.1.9.5")
PWSH = os.environ.get("STFC_PWSH") or shutil.which("pwsh")


@unittest.skipUnless(os.name == "nt" and FIXTURE and PWSH, "Set a Windows PE version fixture and use PowerShell 7")
class WindowsBuildIdentity(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.dll = self.root / "version.dll"
        shutil.copyfile(FIXTURE, self.dll)
        self.original_hash = hashlib.sha256(self.dll.read_bytes()).hexdigest()
        self.context = {"schema_version": "stfc-mac-release-context/v1", "channel": "pre",
                        "release_tag": "", "prerelease": True, "selection_mode": "test",
                        "run_number": "987", "run_attempt": "2"}
        self.identity = self.root / "build-identity.json"
        self.outputs = self.root / "outputs"

    def run_identity(self, context=None, *, expected_hash=None, output=None):
        context_file = self.root / "context.json"
        context_file.write_text(json.dumps(context or self.context), encoding="utf-8")
        command = [PWSH, "-NoProfile", "-File", str(SCRIPT), "-DllPath", str(self.dll),
                   "-ReleaseContextPath", str(context_file), "-SourceSha", "a" * 40,
                   "-BuildRunId", "123", "-ExpectedUnsignedSha256", expected_hash or self.original_hash,
                   "-OutputPath", str(output or self.identity), "-GitHubOutput", str(self.outputs)]
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="strict")
        self.assertEqual(hashlib.sha256(self.dll.read_bytes()).hexdigest(), self.original_hash)
        return result

    def assert_rejected(self, result):
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.identity.exists())
        self.assertFalse(self.outputs.exists())

    def test_unnamed_pre_unicode_description_round_trips_without_pe_changes(self):
        result = self.run_identity()
        self.assertEqual(result.returncode, 0, result.stderr)
        identity = json.loads(self.identity.read_text(encoding="utf-8"))
        self.assertEqual(identity["STFCBaseVersion"], BASE)
        self.assertEqual(identity["STFCDisplayVersion"], f"{BASE} · Pre b987.2")
        raw = self.outputs.read_bytes()
        self.assertNotEqual(raw[:3], b"\xef\xbb\xbf")
        self.assertIn(b"\xc2\xb7", raw)
        self.assertEqual(raw.decode("utf-8"), f"signing_description=STFC Community Mod | {BASE} · Pre b987.2\n")
        self.assertIn(f"STFC Community Mod | {BASE} · Pre b987.2", result.stdout)

    def test_named_release_uses_native_quad_and_exact_canonical_tag(self):
        self.context.update(channel="release", release_tag=f"v{BASE}-guffa.6", prerelease=False,
                            selection_mode="release")
        result = self.run_identity()
        self.assertEqual(result.returncode, 0, result.stderr)
        identity = json.loads(self.identity.read_text(encoding="utf-8"))
        self.assertEqual(identity["STFCDisplayVersion"], f"{BASE}-guffa.6 · Release")
        self.assertEqual(identity["STFCSourceCommit"], "a" * 40)
        self.assertEqual(identity["STFCBuildRunID"], "123")

    def test_named_pre_keeps_its_distinct_label(self):
        self.context.update(release_tag=f"v{BASE}-guffa.6")
        result = self.run_identity()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.identity.read_text(encoding="utf-8"))["STFCDisplayVersion"],
                         f"{BASE}-guffa.6 · Pre")

    def test_changed_unsigned_payload_is_refused_before_any_output(self):
        self.assert_rejected(self.run_identity(expected_hash="0" * 64))

    def test_unsigned_dev_context_cannot_be_relabelled_by_signing_wrapper(self):
        self.context["channel"] = "dev"
        self.assert_rejected(self.run_identity())

    def test_bad_attempt_and_output_collision_are_refused(self):
        for value in ("0", "2\nextra=value"):
            context = copy.deepcopy(self.context)
            context["run_attempt"] = value
            self.assert_rejected(self.run_identity(context))
        self.assert_rejected(self.run_identity(output=self.dll))

    def test_other_core_version_or_malformed_tag_cannot_be_signed_as_this_build(self):
        for tag in ("v9.9.9.9-guffa.6", f"v{BASE}-guffa.6\nsigning_description=forged"):
            context = copy.deepcopy(self.context)
            context["release_tag"] = tag
            self.assert_rejected(self.run_identity(context))


class WindowsWorkflowWiring(unittest.TestCase):
    def test_trust_and_identity_checks_precede_signing_identity_access(self):
        workflow = (ROOT / ".github/workflows/sign-ci-windows.yaml").read_text()
        self.assertLess(workflow.index("python scripts/mac-release-context.py"), workflow.index("azure/login@"))
        self.assertLess(workflow.index("windows-build-identity.ps1"), workflow.index("azure/login@"))
        self.assertIn('--mode "$($context.selection_mode)"', workflow)
        self.assertIn('-SourceSha $env:EMBEDDED_SOURCE_SHA', workflow)
        self.assertIn('description: ${{ steps.build_identity.outputs.signing_description }}', workflow)
        self.assertIn('buildIdentity = $buildIdentity', workflow)
        self.assertIn('unsignedSha256 = $env:UNSIGNED_SHA256', workflow)
        self.assertIn('Signing changed the DLL source provenance resource', workflow)
        self.assertIn('Get-AuthenticodeSignature', workflow)
        self.assertIn('Expected Azure Public Trust code-signing EKUs', workflow)
        dispatch = (ROOT / ".github/workflows/validate-macos-signing.yaml").read_text()
        windows = dispatch.split("  sign-windows:", 1)[1]
        self.assertIn('release_tag: ${{ needs.resolve.outputs.release_tag }}', windows)
        self.assertIn('prerelease: ${{ fromJSON(needs.resolve.outputs.prerelease) }}', windows)


if __name__ == "__main__":
    unittest.main()
