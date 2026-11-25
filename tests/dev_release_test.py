"""Publication boundaries: exact source, tag, payload set and immutable assets."""
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import tarfile
import unittest
import zipfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location("dev_release", ROOT / "scripts/dev-release.py")
DEV = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DEV)
SOURCE = "a" * 40
CONTEXT = dict(schema=DEV.SCHEMA, repository=DEV.REPOSITORY, source=SOURCE, aai_pin="b" * 40,
               profiles_pin="c" * 40, profiles_host=True,
               tag="v1.1.9.5-guffa.6-dev.1", base="1.1.9.5", fork_revision=6, channel="dev",
               run_id="123", run_number=17, run_attempt=1)
MODERN_CONTEXT = dict(CONTEXT, tag="v2.2.0-dev.10", base="2.2.0", fork_revision=0)


class DevelopmentReleaseTest(unittest.TestCase):
    def write_source(self, root, values):
        (root / "src/mod").mkdir(parents=True, exist_ok=True)
        (root / "zig").mkdir(exist_ok=True)
        names = ("VERSION_MAJOR", "VERSION_MINOR", "VERSION_REVISION", "VERSION_PATCH", "VERSION_FORK_REVISION")
        (root / "src/mod/version.h").write_text("".join(
            f"#define {name} {value}\n" for name, value in zip(names, values)), encoding="utf-8")
        (root / "zig/aai.zig").write_text(f'pub const pinned_revision = "{"b" * 40}";\n', encoding="utf-8")
        (root / "zig/profiles.zig").write_text(f'pub const revision = "{"c" * 40}";\n', encoding="utf-8")

    def test_tag_must_match_compiled_lineage_and_native_sequence_limit(self):
        self.assertEqual(DEV.validate_tag(CONTEXT["tag"], "1.1.9.5", 6)["STFCDisplayVersion"],
                         "1.1.9.5-guffa.6-dev.1 · DEV")
        for tag in ("v1.1.9.5-guffa.6", "v1.1.9.5-guffa.7-dev.1", "v1.1.9.0-guffa.6-dev.1",
                    "v1.1.9.5-guffa.6-dev.01", "v1.1.9.5-guffa.6-dev.0", "v1.1.9.5-guffa.6-dev.4294967296"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                DEV.validate_tag(tag, "1.1.9.5", 6)

    def test_independent_tag_and_untagged_identity_preserve_producer_metadata(self):
        for tag in ("v2.2.0-dev.10", ""):
            with self.subTest(tag=tag):
                context = dict(MODERN_CONTEXT, tag=tag)
                fields = DEV.metadata(context)
                self.assertEqual(fields["STFCBaseVersion"], "2.2.0")
                self.assertEqual(fields["STFCForkRevision"], 0)
                self.assertEqual(fields["CFBundleShortVersionString"], "2.2.0")
                self.assertEqual(fields["CFBundleVersion"], "17.1.0")
                self.assertEqual(fields["STFCSourceCommit"], SOURCE)
                self.assertEqual(fields["STFCDisplayVersion"],
                                 "2.2.0-dev.10 · DEV" if tag else "2.2.0 · DEV b17.1")
                self.assertEqual(DEV.product_version(context), "2.2.0-dev.10" if tag else "2.2.0")
        for tag in ("v2.2.0", "v2.2.0-rc.1", "v2.2.1-dev.1", "v2.2.0-dev.0", "v2.2.0-dev.01",
                    "v2.2.0-dev.4294967296", "v2.2.0-dev.1\n", "v1.1.9.5-guffa.6-dev.1"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                DEV.validate_tag(tag, "2.2.0", 0)
        self.assertEqual(DEV.product_version(CONTEXT), "1.1.9.5-guffa.6-dev.1")
        self.assertEqual(DEV.product_version(dict(CONTEXT, tag="")), "1.1.9.5-guffa.6")
        with self.assertRaises(ValueError):
            DEV.validate_tag(MODERN_CONTEXT["tag"], "1.1.9.5", 6)

    def test_independent_source_selection_canonicalizes_header_and_rejects_invalid_branch_versions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "source"
            output = Path(temporary) / "context.json"
            env = dict(GITHUB_REPOSITORY=DEV.REPOSITORY, GITHUB_RUN_ID="123", GITHUB_RUN_NUMBER="17",
                       GITHUB_RUN_ATTEMPT="1", GITHUB_OUTPUT=str(Path(temporary) / "outputs"))
            self.write_source(root, (2, 2, 0, 0, 0))
            for ref in ("refs/heads/play-dev", "refs/tags/v2.2.0-dev.10"):
                with self.subTest(ref=ref), patch.dict(os.environ, env), \
                        patch.object(DEV, "command", return_value=SOURCE), patch.object(DEV, "ROOT", root):
                    selected = DEV.resolve(ref, SOURCE, output)
                self.assertEqual(selected["base"], "2.2.0")
                self.assertEqual(selected["fork_revision"], 0)
                self.assertTrue(selected["aai_host"])
                self.assertTrue(selected["aai_ui_lab"])
                self.assertTrue(selected["profiles_host"])
                self.assertEqual(selected["aai_pin"], "b" * 40)
                self.assertEqual(selected["profiles_pin"], "c" * 40)
                output.unlink()
            for values in ((2, 2, 0, 1, 0), (2, 2, 0, 0, 12), (2, 65536, 0, 0, 0), (2, "02", 0, 0, 0)):
                self.write_source(root, values)
                with self.subTest(values=values), patch.dict(os.environ, env), \
                        patch.object(DEV, "command", return_value=SOURCE), patch.object(DEV, "ROOT", root), \
                        self.assertRaises(ValueError):
                    DEV.resolve("refs/heads/play-dev", SOURCE, output)
                self.assertFalse(output.exists())

    def test_product_version_cli_uses_the_same_validated_identity_as_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            context = Path(temporary) / "context.json"
            for value in (MODERN_CONTEXT, dict(MODERN_CONTEXT, tag=""), CONTEXT):
                context.write_text(json.dumps(value), encoding="utf-8")
                result = subprocess.run([sys.executable, str(ROOT / "scripts/dev-release.py"), "product-version",
                                         "--context", str(context)], check=True, capture_output=True, text=True)
                self.assertEqual(result.stdout.strip(), DEV.product_version(value))

    def test_selection_rejects_wrong_repo_stale_branch_and_moved_tag_before_credentials(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "context.json"
            env = dict(GITHUB_REPOSITORY=DEV.REPOSITORY, GITHUB_RUN_ID="123", GITHUB_RUN_NUMBER="17",
                       GITHUB_RUN_ATTEMPT="1", GITHUB_OUTPUT=str(Path(directory) / "outputs"))
            for repository, replies in (("other/stfc-mod", []), (DEV.REPOSITORY, ["c" * 40, SOURCE]),
                                        (DEV.REPOSITORY, [SOURCE, SOURCE, "c" * 40])):
                with patch.dict(os.environ, dict(env, GITHUB_REPOSITORY=repository)), \
                        patch.object(DEV, "command", side_effect=replies), self.assertRaises(ValueError):
                    DEV.resolve("refs/tags/" + CONTEXT["tag"], SOURCE, output)
                self.assertFalse(output.exists())
            # Use an independent source fixture so advancing the real AAI pin
            # cannot silently change this selector contract's expected value.
            root = Path(directory) / "source"
            self.write_source(root, (1, 1, 9, 5, 6))
            with patch.dict(os.environ, env), patch.object(DEV, "command", return_value=SOURCE), \
                    patch.object(DEV, "ROOT", root):
                selected = DEV.resolve("refs/tags/" + CONTEXT["tag"], SOURCE, output)
            self.assertEqual(selected["tag"], CONTEXT["tag"])
            self.assertEqual(selected["aai_pin"], "b" * 40)
            self.assertTrue(selected["aai_host"])
            self.assertEqual(selected["profiles_pin"], "c" * 40)
            self.assertTrue(selected["profiles_host"])

    def test_missing_helper_and_mismatched_profile_pin_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            notices = root / "notices"
            notices.mkdir()
            (notices / "source.txt").write_text("Guffawaffle/stfc-profiles\n" + CONTEXT["profiles_pin"] + "\n")
            (notices / "LICENSE").write_text("fixture license")
            helper = root / "stfc-profiles"
            with self.assertRaises((ValueError, OSError)):
                DEV.verify_profiles(CONTEXT, helper, notices)
            helper.write_bytes(b"fixture helper")
            DEV.verify_profiles(CONTEXT, helper, notices)
            (notices / "source.txt").write_text("Guffawaffle/stfc-profiles\n" + "d" * 40 + "\n")
            with self.assertRaises(ValueError):
                DEV.verify_profiles(CONTEXT, helper, notices)

    def test_normal_windows_package_stays_compatible_and_complete_bundles_keep_helpers(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            windows = root / "windows/bin/version.dll"
            windows.parent.mkdir(parents=True)
            windows.write_bytes(b"fixture DLL")
            (windows.parent / "stfc-profiles.exe").write_bytes(b"fixture CLI")
            notices = windows.parent.parent / "share/stfc-profiles"
            notices.mkdir(parents=True)
            (notices / "source.txt").write_text("Guffawaffle/stfc-profiles\n" + CONTEXT["profiles_pin"] + "\n")
            (notices / "LICENSE").write_text("fixture license")
            app = root / "STFC Community Mod.app"
            (app / "Contents/Resources/notices/profiles").mkdir(parents=True)
            (app / "Contents/libstfc-community-mod.dylib").write_bytes(b"fixture dylib")
            (app / "Contents/stfc-profiles").write_bytes(b"fixture Mac helper")
            (app / "Contents/Resources/notices/profiles/LICENSE").write_text("fixture license")

            def package_tool(args, **kwargs):
                if args[0] == "zstd":
                    # Keep the intermediate tar for direct inventory inspection; no compressor is needed here.
                    Path(args[-1]).write_bytes(Path(args[3]).read_bytes())
                elif args[0] == "bash":
                    Path(args[-1]).write_bytes(b"fixture DMG")
                else:
                    self.fail(f"unexpected packaging tool: {args[0]}")

            output = root / "dist"
            with patch.object(DEV, "verify_windows"), patch.object(DEV, "verify_app"), \
                    patch.object(DEV, "verify_dmg"), patch.object(DEV.subprocess, "run", side_effect=package_tool):
                DEV.package(CONTEXT, windows, app, output)
            with zipfile.ZipFile(output / "stfc-community-mod.zip") as archive:
                self.assertEqual(archive.namelist(), ["version.dll"])
            with zipfile.ZipFile(output / "stfc-community-mod-profiles-windows-x64.zip") as archive:
                self.assertTrue({"version.dll", "stfc-profiles.exe", "README.md", "notices/profiles/LICENSE",
                                 "notices/profiles/source.txt"}.issubset(archive.namelist()))
            with tarfile.open(output / "stfc-community-mod-windows-x64.tar.zst") as archive:
                self.assertEqual(archive.getnames(), ["version.dll"])
            with tarfile.open(output / "stfc-community-mod-macos-universal.tar.zst") as archive:
                self.assertTrue({"libstfc-community-mod.dylib", "stfc-profiles", "notices/profiles/LICENSE"}
                                .issubset(archive.getnames()))
            DEV.verify_assets(CONTEXT, output)

    def test_modified_dmg_app_is_rejected_and_volume_is_detached(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app = root / "STFC Community Mod.app"
            mount = root / "volume"
            mounted = mount / app.name
            app.mkdir()
            mounted.mkdir(parents=True)
            (app / "payload").write_bytes(b"verified")
            (mounted / "payload").write_bytes(b"changed")
            attached = plistlib.dumps({"system-entities": [{"mount-point": str(mount)}]})
            with patch.object(DEV, "verify_app"), patch.object(DEV.subprocess, "check_output", return_value=attached), \
                    patch.object(DEV.subprocess, "run") as commands, self.assertRaises(ValueError):
                DEV.verify_dmg(CONTEXT, app, root / "fixture.dmg")
            self.assertEqual(commands.call_args_list[-1].args[0], ["hdiutil", "detach", str(mount)])

    def make_assets(self, directory, context=CONTEXT):
        for name in DEV.payload_names(context["tag"]):
            (directory / name).write_bytes(name.encode())
        manifest = dict(context, display_version=DEV.metadata(context)["STFCDisplayVersion"],
                        assets={p.name: dict(sha256=DEV.sha256(p), size=p.stat().st_size)
                                for p in directory.iterdir()})
        (directory / "dev-release.json").write_text(json.dumps(manifest), encoding="utf-8")
        (directory / "SHA256SUMS").write_text("".join(f"{DEV.sha256(p)}  {p.name}\n"
                                                    for p in sorted(directory.iterdir())), encoding="utf-8")

    def test_publication_rejects_changed_payload_extra_asset_and_changed_provenance(self):
        for mutation in ("changed", "extra", "missing", "provenance", "checksums", "display"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                self.make_assets(directory)
                DEV.verify_assets(CONTEXT, directory)
                dll = directory / "stfc-community-mod.zip"
                if mutation == "changed":
                    dll.write_bytes(b"changed")
                elif mutation == "extra":
                    (directory / "private-source.cc").write_bytes(b"must never publish")
                elif mutation == "missing":
                    dll.unlink()
                elif mutation == "checksums":
                    (directory / "SHA256SUMS").write_bytes(b"changed")
                else:
                    path = directory / "dev-release.json"
                    manifest = json.loads(path.read_text(encoding="utf-8"))
                    manifest["aai_pin" if mutation == "provenance" else "display_version"] = "c" * 40
                    path.write_text(json.dumps(manifest), encoding="utf-8")
                with self.assertRaises((ValueError, OSError)):
                    DEV.verify_assets(CONTEXT, directory)

    def test_independent_manifest_retains_tag_source_and_canonical_display(self):
        for context in (MODERN_CONTEXT, dict(MODERN_CONTEXT, tag="")):
            with self.subTest(tag=context["tag"]), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                self.make_assets(directory, context)
                manifest = DEV.verify_assets(context, directory)
                self.assertEqual(manifest["base"], "2.2.0")
                self.assertEqual(manifest["fork_revision"], 0)
                self.assertEqual(manifest["source"], SOURCE)
                self.assertEqual(manifest["display_version"], DEV.metadata(context)["STFCDisplayVersion"])

    def test_existing_published_release_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.make_assets(directory)
            with patch.dict(os.environ, {"GITHUB_REPOSITORY": DEV.REPOSITORY}), \
                    patch.object(DEV, "api", return_value={"object": {"type": "commit", "sha": SOURCE}}), \
                    patch.object(DEV, "command", return_value=json.dumps([[dict(
                        tag_name=CONTEXT["tag"], draft=False, target_commitish=SOURCE)]])), \
                    patch.object(DEV.subprocess, "run") as upload, self.assertRaises(ValueError):
                DEV.publish(CONTEXT, directory)
            upload.assert_not_called()


if __name__ == "__main__":
    unittest.main()
