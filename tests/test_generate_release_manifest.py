from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from scripts.generate_release_manifest import (
    MANIFEST_SCHEMA_VERSION,
    ManifestError,
    build_manifest,
    validate_manifest,
    write_manifest,
)


TARGET_COMMIT = "0123456789abcdef0123456789abcdef01234567"
REPOSITORY = "Guffawaffle/stfc-mod"


class ReleaseManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.artifact_root = self.root / "artifacts"
        self.artifact_root.mkdir()
        self.mod = self.artifact_root / "version.dll"
        self.launcher = self.artifact_root / "launcher.zip"
        self.mod.write_bytes(b"signed-mod-fixture")
        self.launcher.write_bytes(b"signed-launcher-fixture")
        self.spec_path = self.root / "spec.json"
        self.spec_path.write_text(
            json.dumps(
                {
                    "specVersion": 1,
                    "minimumLauncherVersion": "0.1.0",
                    "artifacts": [
                        {
                            "id": "windows-mod-dll-x64",
                            "kind": "windows-mod",
                            "platform": "windows",
                            "architecture": "x64",
                            "sourcePath": "version.dll",
                            "fileName": "version.dll",
                            "mediaType": "application/vnd.microsoft.portable-executable",
                            "authenticity": {
                                "scheme": "authenticode",
                                "scope": "artifact",
                            },
                        },
                        {
                            "id": "windows-launcher-archive-x64",
                            "kind": "windows-launcher",
                            "platform": "windows",
                            "architecture": "x64",
                            "sourcePath": "launcher.zip",
                            "fileName": "launcher.zip",
                            "mediaType": "application/zip",
                            "authenticity": {
                                "scheme": "authenticode",
                                "scope": "contents",
                                "signedFiles": [
                                    "STFCCommunityMod.Launcher.exe",
                                    "STFCCommunityMod.Launcher.Updater.exe",
                                ],
                            },
                        },
                    ],
                }
            ),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _build(self, tag: str = "v2.1.0-guffa.8") -> dict[str, object]:
        return build_manifest(
            tag=tag,
            target_commit=TARGET_COMMIT,
            repository=REPOSITORY,
            artifact_root=self.artifact_root,
            spec_path=self.spec_path,
        )

    def test_fixture_generates_and_validates_artifact_facts(self) -> None:
        manifest = self._build()
        manifest_path = self.root / "release-manifest.json"
        write_manifest(manifest, manifest_path)

        validate_manifest(
            manifest_path=manifest_path,
            tag="v2.1.0-guffa.8",
            target_commit=TARGET_COMMIT,
            repository=REPOSITORY,
            artifact_root=self.artifact_root,
            spec_path=self.spec_path,
        )

        self.assertEqual(MANIFEST_SCHEMA_VERSION, manifest["schemaVersion"])
        self.assertEqual("2.1.0-guffa.8", manifest["releaseVersion"])
        self.assertEqual("stable", manifest["channel"])
        self.assertEqual("none", manifest["manifestAuthenticity"]["scheme"])
        self.assertEqual(self.mod.stat().st_size, manifest["artifacts"][0]["size"])
        self.assertRegex(manifest["artifacts"][0]["sha256"], r"^[0-9a-f]{64}$")

    def test_checked_in_release_spec_round_trips_fixture_layout(self) -> None:
        repository_root = Path(__file__).resolve().parents[1]
        checked_in_spec = repository_root / "scripts" / "windows_release_manifest_spec.json"
        spec = json.loads(checked_in_spec.read_text(encoding="utf-8"))
        fixture_root = self.root / "release-layout"
        fixture_root.mkdir()
        for index, artifact in enumerate(spec["artifacts"]):
            artifact_path = fixture_root / artifact["sourcePath"]
            artifact_path.parent.mkdir(parents=True, exist_ok=True)
            artifact_path.write_bytes(f"release-artifact-{index}".encode())

        manifest = build_manifest(
            tag="v2.1.0-guffa.8",
            target_commit=TARGET_COMMIT,
            repository=REPOSITORY,
            artifact_root=fixture_root,
            spec_path=checked_in_spec,
        )
        manifest_path = self.root / "checked-in-spec-manifest.json"
        write_manifest(manifest, manifest_path)
        validate_manifest(
            manifest_path=manifest_path,
            tag="v2.1.0-guffa.8",
            target_commit=TARGET_COMMIT,
            repository=REPOSITORY,
            artifact_root=fixture_root,
            spec_path=checked_in_spec,
        )

        self.assertEqual(
            [
                "windows-mod-dll-x64",
                "windows-mod-runtime-manifest-x64",
                "windows-mod-archive-x64",
                "windows-launcher-archive-x64",
                "windows-launcher-setup-x64",
            ],
            [artifact["id"] for artifact in manifest["artifacts"]],
        )
        runtime_manifest_artifact = next(
            artifact
            for artifact in manifest["artifacts"]
            if artifact["id"] == "windows-mod-runtime-manifest-x64"
        )
        self.assertEqual("windows-mod-runtime-manifest", runtime_manifest_artifact["kind"])
        self.assertEqual("stfc-runtime-manifest.json", runtime_manifest_artifact["fileName"])
        self.assertEqual("application/json", runtime_manifest_artifact["mediaType"])
        self.assertEqual(
            {"scheme": "none", "scope": "none"},
            runtime_manifest_artifact["authenticity"],
        )
        self.assertRegex(runtime_manifest_artifact["sha256"], r"^[0-9a-f]{64}$")

    def test_preview_channel_is_derived_from_supported_tag(self) -> None:
        manifest = self._build("v2.2.0-guffa.rc1")
        self.assertEqual("preview", manifest["channel"])

    def test_tampered_artifact_fails_validation(self) -> None:
        manifest_path = self.root / "release-manifest.json"
        write_manifest(self._build(), manifest_path)
        self.mod.write_bytes(b"tampered")

        with self.assertRaisesRegex(ManifestError, "does not match"):
            validate_manifest(
                manifest_path=manifest_path,
                tag="v2.1.0-guffa.8",
                target_commit=TARGET_COMMIT,
                repository=REPOSITORY,
                artifact_root=self.artifact_root,
                spec_path=self.spec_path,
            )

    def test_unknown_manifest_schema_fails_closed(self) -> None:
        manifest = self._build()
        manifest["schemaVersion"] = 2
        manifest_path = self.root / "release-manifest.json"
        write_manifest(manifest, manifest_path)

        with self.assertRaisesRegex(ManifestError, "unsupported release manifest schema"):
            validate_manifest(
                manifest_path=manifest_path,
                tag="v2.1.0-guffa.8",
                target_commit=TARGET_COMMIT,
                repository=REPOSITORY,
                artifact_root=self.artifact_root,
                spec_path=self.spec_path,
            )

    def test_boolean_schema_version_fails_closed(self) -> None:
        manifest = self._build()
        manifest["schemaVersion"] = True
        manifest_path = self.root / "release-manifest.json"
        write_manifest(manifest, manifest_path)

        with self.assertRaisesRegex(ManifestError, "unsupported release manifest schema"):
            validate_manifest(
                manifest_path=manifest_path,
                tag="v2.1.0-guffa.8",
                target_commit=TARGET_COMMIT,
                repository=REPOSITORY,
                artifact_root=self.artifact_root,
                spec_path=self.spec_path,
            )

    def test_artifact_source_cannot_escape_root(self) -> None:
        spec = json.loads(self.spec_path.read_text(encoding="utf-8"))
        spec["artifacts"][0]["sourcePath"] = "../version.dll"
        spec["artifacts"][0]["fileName"] = "version.dll"
        self.spec_path.write_text(json.dumps(spec), encoding="utf-8")

        with self.assertRaisesRegex(ManifestError, "safe relative path"):
            self._build()

    def test_empty_artifact_fails_closed(self) -> None:
        self.mod.write_bytes(b"")
        with self.assertRaisesRegex(ManifestError, "artifact source is empty"):
            self._build()

    def test_signed_archive_member_cannot_escape_extraction_root(self) -> None:
        spec = json.loads(self.spec_path.read_text(encoding="utf-8"))
        spec["artifacts"][1]["authenticity"]["signedFiles"] = ["../launcher.exe"]
        self.spec_path.write_text(json.dumps(spec), encoding="utf-8")

        with self.assertRaisesRegex(ManifestError, "safe relative paths"):
            self._build()

    def test_unknown_release_tag_fails_closed(self) -> None:
        with self.assertRaisesRegex(ManifestError, "supported release form"):
            self._build("vnext")


if __name__ == "__main__":
    unittest.main()
