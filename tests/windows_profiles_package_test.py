import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


profiles = module("profiles", "windows-profiles-package.py")
signing = module("signing", "ci-signing-input.py")


class WindowsProfilesPackage(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prefix, self.payload = self.root / "prefix", self.root / "payload"
        for name in profiles.PROFILE_FILES:
            path = self.prefix / "share/stfc-profiles" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture notice\n", encoding="utf-8")
        (self.prefix / "share/stfc-profiles/source.txt").write_text(
            "Guffawaffle/stfc-profiles\n" + profiles.pin() + "\n", encoding="utf-8")
        (self.prefix / "bin").mkdir()
        for name in ("version.dll", "stfc-profiles.exe"):
            (self.prefix / "bin" / name).write_bytes(b"unsigned " + name.encode())
        self.manifest = profiles.stage(self.prefix, self.payload, "a" * 40, "a" * 40, "123:2")

    def verify(self, **kwargs):
        options = dict(payload=self.payload, dll=self.prefix / "bin/version.dll", source="a" * 40, run="123:2")
        options.update(kwargs)
        return profiles.verify(**options)

    def test_stage_carries_complete_dependency_notices_and_matching_receipt(self):
        self.assertEqual(self.verify(), self.manifest)
        self.assertIn("notices/profiles/notices/windows/zlib/LICENSE", self.manifest["files"])
        self.assertEqual(set(profiles.file_map(self.payload)), profiles.ARCHIVE_FILES)

    def test_modified_helper_notice_or_missing_license_fails(self):
        for name in ("stfc-profiles.exe", "notices/profiles/notices/windows/xz/COPYING.0BSD"):
            path = self.payload / name
            original = path.read_bytes()
            path.write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError, "differs"):
                self.verify()
            path.write_bytes(original)
        (self.payload / "notices/profiles/LICENSE").unlink()
        with self.assertRaises(ValueError):
            self.verify()

    def test_source_run_pin_and_canonical_dll_must_all_match(self):
        for args in ({"source": "b" * 40}, {"run": "123:3"}, {"run": "0:2"}):
            with self.assertRaises(ValueError):
                self.verify(**args)
        (self.prefix / "bin/version.dll").write_bytes(b"other DLL")
        with self.assertRaisesRegex(ValueError, "canonical DLL"):
            self.verify()
        manifest = copy.deepcopy(self.manifest)
        manifest["profiles"]["revision"] = "b" * 40
        (self.payload / "manifest.json").write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "trusted pin"):
            self.verify()

    def test_extra_files_and_manifest_payload_changes_fail(self):
        (self.payload / "extra.exe").write_bytes(b"unexpected")
        with self.assertRaises(ValueError):
            self.verify()
        (self.payload / "extra.exe").unlink()
        manifest = copy.deepcopy(self.manifest)
        del manifest["files"]["notices/profiles/LICENSE"]
        (self.payload / "manifest.json").write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            self.verify()

    def test_signed_package_preserves_notices_and_retains_both_hash_sets(self):
        receipt = self.root / "verified.json"
        receipt.write_text(json.dumps(self.verify()))
        signed_dll = self.root / "signed.dll"
        signed_dll.write_bytes(b"signed DLL")
        (self.payload / "stfc-profiles.exe").write_bytes(b"signed helper")
        signing_receipt = self.root / "provenance.json"
        signing_receipt.write_text('{"signer":"fixture publisher"}')
        output = self.root / "profiles.zip"
        result = profiles.package(self.payload, signed_dll, receipt, output, signing_receipt)
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(set(archive.namelist()), profiles.ARCHIVE_FILES)
            self.assertEqual(archive.read("version.dll"), b"signed DLL")
            self.assertEqual(archive.read("stfc-profiles.exe"), b"signed helper")
        self.assertEqual(result["unsignedFiles"], self.manifest["files"])
        self.assertEqual(result["files"]["version.dll"]["sha256"], profiles.digest(signed_dll))
        self.assertNotEqual(result["files"]["stfc-profiles.exe"], result["unsignedFiles"]["stfc-profiles.exe"])
        (self.payload / "notices/mod-LICENSE").write_bytes(b"changed later")
        with self.assertRaisesRegex(ValueError, "notices changed"):
            profiles.package(self.payload, signed_dll, receipt, self.root / "bad.zip", signing_receipt)

    def selection(self, archive):
        return {"platform": "windows-profiles", "filename": "manifest.json",
                "artifact_digest": "sha256:" + profiles.digest(archive)}

    def archive(self, extra=None, symlink=False):
        archive = self.root / "input.zip"
        with zipfile.ZipFile(archive, "w") as package:
            for name in profiles.ARCHIVE_FILES:
                if symlink and name == "stfc-profiles.exe":
                    info = zipfile.ZipInfo(name)
                    info.external_attr = (stat.S_IFLNK | 0o777) << 16
                    package.writestr(info, b"outside")
                else:
                    package.write(self.payload / name, name)
            if extra:
                package.writestr(extra, b"extra")
        return archive

    def test_strict_extraction_uses_finite_allowlist_and_archive_digest(self):
        archive = self.archive()
        output = self.root / "extracted"
        signing.extract(archive, self.selection(archive), output)
        self.assertEqual(profiles.file_map(output), profiles.file_map(self.payload))
        for extra in ("../escape", "extra.exe", "version.dll"):
            archive = self.archive(extra)
            with self.assertRaises(ValueError):
                signing.extract(archive, self.selection(archive), self.root / "bad")
        archive = self.archive(symlink=True)
        with self.assertRaises(ValueError):
            signing.extract(archive, self.selection(archive), self.root / "bad")
        selection = self.selection(archive)
        selection["artifact_digest"] = "sha256:" + "0" * 64
        with self.assertRaisesRegex(ValueError, "SHA256"):
            signing.extract(archive, selection, self.root / "bad")


if __name__ == "__main__":
    unittest.main()
