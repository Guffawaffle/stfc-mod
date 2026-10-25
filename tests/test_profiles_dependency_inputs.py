"""Synthetic source/prefix verification fixtures; no game, network or account state."""
import contextlib
import io
import json
from pathlib import Path
import runpy
import tarfile
import tempfile
from types import SimpleNamespace
import unittest


API = runpy.run_path(str(Path(__file__).resolve().parents[1] / "scripts/prepare-profiles-deps.py"))


class ProfilesDependencyInputs(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="profiles-build-inputs-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def archive(self, name="package/src/file.cc", data=b"synthetic source"):
        path = self.root / "source.tar.gz"
        with tarfile.open(path, "w:gz") as package:
            info = tarfile.TarInfo(name)
            info.size = len(data)
            package.addfile(info, io.BytesIO(data))
        return path, API["digest"](path)

    def test_archive_hash_mismatch_is_rejected(self):
        archive, _ = self.archive()
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            API["archive_files"](archive, "0" * 64)

    def test_parent_escape_in_source_package_is_rejected(self):
        archive, sha = self.archive("package/../escape.cc")
        with self.assertRaisesRegex(ValueError, "unsafe package member"):
            API["archive_files"](archive, sha)

    def test_complete_source_tree_matches_archive(self):
        archive, sha = self.archive()
        source = self.root / "source"
        (source / "src").mkdir(parents=True)
        (source / "src/file.cc").write_bytes(b"synthetic source")
        self.assertEqual(len(API["verify_source"](source, archive, sha)["files"]), 1)
        (source / "src/file.cc").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "differs from its pinned archive"):
            API["verify_source"](source, archive, sha)

    def test_unlisted_source_file_is_rejected(self):
        archive, sha = self.archive()
        source = self.root / "source"
        (source / "src").mkdir(parents=True)
        (source / "src/file.cc").write_bytes(b"synthetic source")
        (source / "extra.cc").write_bytes(b"not in source archive")
        with self.assertRaisesRegex(ValueError, "differs from its pinned archive"):
            API["verify_source"](source, archive, sha)

    def prefix(self):
        prefix = self.root / "prefix"
        (prefix / "lib").mkdir(parents=True)
        libraries = ["lib/archive.lib", "lib/lzma.lib", "lib/zs.lib"]
        for name in libraries:
            (prefix / name).write_bytes(b"SYNTHETIC unit fixture; not a real library")
        options = {**API["ARCHIVE_OPTIONS"], "ENABLE_CNG": "ON"}
        receipt = {"schema": 1, "kind": "stfc-profiles-c-deps", "target": "x86_64-windows-msvc",
                   "configuration": "Release", "runtime": "MT", "static_libraries": libraries,
                   "sources": {name: {"archive_sha256": pin[2]} for name, pin in API["PINS"].items()},
                   "feature_options": options, "qualified_features": ["HAVE_LIBLZMA", "HAVE_LIBZ"],
                   "cache_values": options, "files": API["tree_files"](prefix)}
        file = self.root / "receipt.json"
        file.write_text(json.dumps(receipt), encoding="utf-8")
        return SimpleNamespace(profiles_source=None, profiles_archive=None, prefix=str(prefix), receipt=str(file), target="x86_64-windows-msvc"), receipt

    def test_prefix_integrity_and_mutation(self):
        args, _ = self.prefix()
        with contextlib.redirect_stdout(io.StringIO()):
            API["verify"](args)
        (Path(args.prefix) / "lib/archive.lib").write_bytes(b"changed archive")
        with self.assertRaisesRegex(ValueError, "differs from its qualified receipt"):
            API["verify"](args)

    def test_wrong_target_is_rejected(self):
        args, _ = self.prefix()
        args.target = "aarch64-macos.14.6"
        with self.assertRaisesRegex(ValueError, "schema/kind/target"):
            API["verify"](args)

    def test_missing_dependency_is_rejected(self):
        args, receipt = self.prefix()
        del receipt["sources"]["xz"]
        Path(args.receipt).write_text(json.dumps(receipt), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Incomplete dependency source closure"):
            API["verify"](args)

    def test_different_codec_configuration_is_rejected(self):
        args, receipt = self.prefix()
        receipt["feature_options"]["ENABLE_LZMA"] = "OFF"
        Path(args.receipt).write_text(json.dumps(receipt), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "feature qualification mismatch"):
            API["verify"](args)


if __name__ == "__main__":
    unittest.main()
