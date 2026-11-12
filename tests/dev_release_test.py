"""Publication boundaries: exact source, tag, payload set and immutable assets."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("dev_release", ROOT / "scripts/dev-release.py")
DEV = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DEV)
SOURCE = "a" * 40
CONTEXT = dict(schema=DEV.SCHEMA, repository=DEV.REPOSITORY, source=SOURCE, aai_pin="b" * 40,
               tag="v1.1.9.5-guffa.6-dev.1", base="1.1.9.5", fork_revision=6, channel="dev",
               run_id="123", run_number=17, run_attempt=1)


class DevelopmentReleaseTest(unittest.TestCase):
    def test_tag_must_match_compiled_lineage_and_native_sequence_limit(self):
        self.assertEqual(DEV.validate_tag(CONTEXT["tag"], "1.1.9.5", 6)["STFCDisplayVersion"],
                         "1.1.9.5-guffa.6-dev.1 · DEV")
        for tag in ("v1.1.9.5-guffa.6", "v1.1.9.5-guffa.7-dev.1", "v1.1.9.0-guffa.6-dev.1",
                    "v1.1.9.5-guffa.6-dev.01", "v1.1.9.5-guffa.6-dev.0", "v1.1.9.5-guffa.6-dev.4294967296"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                DEV.validate_tag(tag, "1.1.9.5", 6)

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
            (root / "src/mod").mkdir(parents=True)
            (root / "zig").mkdir()
            names = ("VERSION_MAJOR", "VERSION_MINOR", "VERSION_REVISION", "VERSION_PATCH", "VERSION_FORK_REVISION")
            (root / "src/mod/version.h").write_text("".join(
                f"#define {name} {value}\n" for name, value in zip(names, (1, 1, 9, 5, 6))), encoding="utf-8")
            (root / "zig/aai.zig").write_text(f'pub const pinned_revision = "{"b" * 40}";\n', encoding="utf-8")
            with patch.dict(os.environ, env), patch.object(DEV, "command", return_value=SOURCE), \
                    patch.object(DEV, "ROOT", root):
                selected = DEV.resolve("refs/tags/" + CONTEXT["tag"], SOURCE, output)
            self.assertEqual(selected["tag"], CONTEXT["tag"])
            self.assertEqual(selected["aai_pin"], "b" * 40)
            self.assertTrue(selected["aai_host"])

    def make_assets(self, directory):
        for name in DEV.payload_names(CONTEXT["tag"]):
            (directory / name).write_bytes(name.encode())
        manifest = dict(CONTEXT, assets={p.name: dict(sha256=DEV.sha256(p), size=p.stat().st_size)
                                         for p in directory.iterdir()})
        (directory / "dev-release.json").write_text(json.dumps(manifest), encoding="utf-8")
        (directory / "SHA256SUMS").write_text("".join(f"{DEV.sha256(p)}  {p.name}\n"
                                                    for p in sorted(directory.iterdir())), encoding="utf-8")

    def test_publication_rejects_changed_payload_extra_asset_and_changed_provenance(self):
        for mutation in ("changed", "extra", "missing", "provenance", "checksums"):
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
                    manifest["aai_pin"] = "c" * 40
                    path.write_text(json.dumps(manifest), encoding="utf-8")
                with self.assertRaises((ValueError, OSError)):
                    DEV.verify_assets(CONTEXT, directory)

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
