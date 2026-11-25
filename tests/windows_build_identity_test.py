import copy
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/windows-build-identity.ps1"
FIXTURE = os.environ.get("STFC_WINDOWS_IDENTITY_FIXTURE")
BASE = os.environ.get("STFC_WINDOWS_IDENTITY_BASE", "1.1.9.5")
FORK_REVISION = int(os.environ.get("STFC_WINDOWS_IDENTITY_FORK_REVISION", "0"))
DISPLAY_BASE = f"{BASE}-guffa.{FORK_REVISION}" if FORK_REVISION else BASE
INVALID_FIXTURE = os.environ.get("STFC_WINDOWS_IDENTITY_INVALID_FIXTURE")
PWSH = os.environ.get("STFC_PWSH") or shutil.which("pwsh")


def pe_fixture(path, fixed=(1, 1, 9, 5), product="1.1.9.5"):
    """Give a temporary copy of Python an isolated, controlled version resource."""
    def block(key, value=b"", children=(), *, text=False):
        data = bytearray(struct.pack("<HHH", 0, len(value) // 2 if text else len(value), int(text)))
        data.extend((key + "\0").encode("utf-16le"))
        data.extend(b"\0" * (-len(data) % 4))
        data.extend(value)
        for child in children:
            data.extend(b"\0" * (-len(data) % 4))
            data.extend(child)
        struct.pack_into("<H", data, 0, len(data))
        return bytes(data)

    ms, ls = (fixed[0] << 16) | fixed[1], (fixed[2] << 16) | fixed[3]
    fixed_info = struct.pack("<13I", 0xFEEF04BD, 0x10000, ms, ls, ms, ls, 0x3f, 0, 0x40004, 2, 0, 0, 0)
    strings = block("StringFileInfo", children=[block("040904b0", children=[
        block("ProductVersion", (product + "\0").encode("utf-16le"), text=True),
        block("FileVersion", (".".join(map(str, fixed)) + "\0").encode("utf-16le"), text=True),
    ], text=True)], text=True)
    translations = block("VarFileInfo", children=[block("Translation", struct.pack("<HH", 0x409, 1200))], text=True)
    resource = block("VS_VERSION_INFO", fixed_info, [strings, translations])
    shutil.copyfile(sys.executable, path)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.BeginUpdateResourceW.argtypes = [ctypes.c_wchar_p, ctypes.c_bool]
    kernel.BeginUpdateResourceW.restype = ctypes.c_void_p
    kernel.UpdateResourceW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                      ctypes.c_ushort, ctypes.c_void_p, ctypes.c_ulong]
    kernel.UpdateResourceW.restype = ctypes.c_bool
    kernel.EndUpdateResourceW.argtypes = [ctypes.c_void_p, ctypes.c_bool]
    kernel.EndUpdateResourceW.restype = ctypes.c_bool
    handle = kernel.BeginUpdateResourceW(str(path), True)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    payload = ctypes.create_string_buffer(resource)
    if not kernel.UpdateResourceW(handle, 16, 1, 0x409, payload, len(resource)):
        error = ctypes.WinError(ctypes.get_last_error())
        kernel.EndUpdateResourceW(handle, True)
        raise error
    if not kernel.EndUpdateResourceW(handle, False):
        raise ctypes.WinError(ctypes.get_last_error())


@unittest.skipUnless(os.name == "nt" and PWSH, "Use Windows and PowerShell 7")
class WindowsBuildIdentity(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.dll = self.root / "version.dll"
        if FIXTURE:
            shutil.copyfile(FIXTURE, self.dll)
        else:
            pe_fixture(self.dll)
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
        self.assertEqual(identity["STFCForkRevision"], FORK_REVISION)
        self.assertEqual(identity["STFCDisplayVersion"], f"{DISPLAY_BASE} · Pre b987.2")
        raw = self.outputs.read_bytes()
        self.assertNotEqual(raw[:3], b"\xef\xbb\xbf")
        self.assertIn(b"\xc2\xb7", raw)
        self.assertEqual(raw.decode("utf-8"), f"signing_description=STFC Community Mod | {DISPLAY_BASE} · Pre b987.2\n")
        self.assertIn(f"STFC Community Mod | {DISPLAY_BASE} · Pre b987.2", result.stdout)

    def test_named_release_uses_native_quad_and_exact_canonical_tag(self):
        tag = f"v{BASE}" if len(BASE.split(".")) == 3 else f"v{BASE}-guffa.7"
        self.context.update(channel="release", release_tag=tag, prerelease=False,
                            selection_mode="release")
        result = self.run_identity()
        self.assertEqual(result.returncode, 0, result.stderr)
        identity = json.loads(self.identity.read_text(encoding="utf-8"))
        self.assertEqual(identity["STFCDisplayVersion"], f"{tag[1:]} · Release")

    def test_independent_native_quad_produces_three_part_release_and_rc(self):
        pe_fixture(self.dll, (2, 2, 0, 0), "2.2.0")
        self.original_hash = hashlib.sha256(self.dll.read_bytes()).hexdigest()
        for channel, tag, label, prerelease in (("release", "v2.2.0", "Release", False),
                                               ("pre", "v2.2.0-rc.12", "Pre", True),
                                               ("pre", "", "Pre b987.2", True)):
            self.context.update(channel=channel, release_tag=tag, prerelease=prerelease)
            result = self.run_identity()
            self.assertEqual(result.returncode, 0, result.stderr)
            identity = json.loads(self.identity.read_text(encoding="utf-8"))
            self.assertEqual(identity["STFCBaseVersion"], "2.2.0")
            self.assertEqual(identity["STFCForkRevision"], 0)
            self.assertEqual(identity["STFCDisplayVersion"], f"{tag[1:] if tag else '2.2.0'} · {label}")
            self.assertEqual(identity["STFCSourceCommit"], "a" * 40)
            self.assertEqual(identity["STFCBuildRunID"], "123")

    def test_independent_conflicting_resources_are_rejected_without_outputs(self):
        for fixed, product in (((2, 2, 0, 1), "2.2.0"), ((2, 2, 0, 0), "2.2.1"),
                               ((2, 2, 0, 0), "2.2.0.0"), ((2, 2, 0, 0), "2.2.0-guffa.7")):
            pe_fixture(self.dll, fixed, product)
            self.original_hash = hashlib.sha256(self.dll.read_bytes()).hexdigest()
            with self.subTest(fixed=fixed, product=product):
                self.assert_rejected(self.run_identity())

    def test_independent_wrong_base_and_wrong_channel_are_rejected_without_outputs(self):
        pe_fixture(self.dll, (2, 2, 0, 0), "2.2.0")
        self.original_hash = hashlib.sha256(self.dll.read_bytes()).hexdigest()
        for channel, tag in (("release", "v2.2.1"), ("release", "v2.2.0-rc.1"),
                             ("pre", "v2.2.0"), ("pre", "v2.2.0-dev.1"),
                             ("release", "v2.2.0.0-guffa.7")):
            self.context.update(channel=channel, release_tag=tag, prerelease=channel == "pre")
            with self.subTest(channel=channel, tag=tag):
                self.assert_rejected(self.run_identity())

    def test_named_pre_keeps_its_distinct_label(self):
        tag = f"v{BASE}-rc.6" if len(BASE.split(".")) == 3 else f"v{BASE}-guffa.6"
        self.context.update(release_tag=tag)
        result = self.run_identity()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.identity.read_text(encoding="utf-8"))["STFCDisplayVersion"],
                         f"{tag[1:]} · Pre")

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

    @unittest.skipUnless(INVALID_FIXTURE, "Set a PE fixture with a ProductVersion that conflicts with its fixed version")
    def test_conflicting_product_version_is_refused_without_output(self):
        shutil.copyfile(INVALID_FIXTURE, self.dll)
        self.original_hash = hashlib.sha256(self.dll.read_bytes()).hexdigest()
        self.assert_rejected(self.run_identity())


class WindowsWorkflowWiring(unittest.TestCase):
    def test_trust_and_identity_checks_precede_signing_identity_access(self):
        workflow = (ROOT / ".github/workflows/sign-ci-windows.yaml").read_text()
        self.assertLess(workflow.index("python scripts/mac-release-context.py"), workflow.index("azure/login@"))
        self.assertLess(workflow.index("windows-build-identity.ps1"), workflow.index("azure/login@"))
        self.assertIn('--mode "$($context.selection_mode)"', workflow)
        self.assertEqual(workflow.count('--expected-branch "$($context.source_branch)"'), 2)
        self.assertIn('compare/$env:SOURCE_SHA...$($context.source_branch)', workflow)
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
