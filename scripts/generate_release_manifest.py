#!/usr/bin/env python3
"""Generate and validate the canonical STFC Community Mod release manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path, PurePosixPath
from typing import Any


MANIFEST_SCHEMA_VERSION = 1
SPEC_VERSION = 1
TAG_PATTERN = re.compile(
    r"^v(?P<version>\d+\.\d+\.\d+"
    r"(?:(?:-guffa\.(?:\d+|rc\d+))|(?:\.(?:alpha|beta)\.\d+))?)$"
)
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
REPOSITORY_PATTERN = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
IDENTIFIER_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")
VERSION_PATTERN = re.compile(r"^\d+\.\d+\.\d+(?:[-.][0-9A-Za-z.-]+)?$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class ManifestError(ValueError):
    """Raised when release manifest input or output violates the v1 contract."""


def _load_object(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exception:
        raise ManifestError(f"{label} does not exist: {path}") from exception
    except json.JSONDecodeError as exception:
        raise ManifestError(
            f"{label} is not valid JSON at line {exception.lineno}, "
            f"column {exception.colno}: {exception.msg}"
        ) from exception

    if not isinstance(value, dict):
        raise ManifestError(f"{label} root must be a JSON object")
    return value


def _require_keys(
    value: dict[str, Any],
    *,
    required: set[str],
    optional: set[str] = frozenset(),
    context: str,
) -> None:
    missing = sorted(required - value.keys())
    unknown = sorted(value.keys() - required - optional)
    if missing:
        raise ManifestError(f"{context} is missing required field(s): {', '.join(missing)}")
    if unknown:
        raise ManifestError(f"{context} contains unknown field(s): {', '.join(unknown)}")


def _require_string(value: Any, context: str) -> str:
    if not isinstance(value, str) or not value:
        raise ManifestError(f"{context} must be a non-empty string")
    return value


def _release_identity(tag: str) -> tuple[str, str]:
    match = TAG_PATTERN.fullmatch(tag)
    if match is None:
        raise ManifestError(
            "tag must match a supported release form: "
            "vX.Y.Z, vX.Y.Z-guffa.N, vX.Y.Z-guffa.rcN, "
            "vX.Y.Z.alpha.N, or vX.Y.Z.beta.N"
        )

    version = match.group("version")
    channel = (
        "preview"
        if ".alpha." in version or ".beta." in version or "-guffa.rc" in version
        else "stable"
    )
    return version, channel


def _validate_authenticity(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ManifestError(f"{context} must be an object")
    _require_keys(
        value,
        required={"scheme", "scope"},
        optional={"signedFiles"},
        context=context,
    )

    scheme = _require_string(value["scheme"], f"{context}.scheme")
    scope = _require_string(value["scope"], f"{context}.scope")
    if scheme not in {"authenticode", "none"}:
        raise ManifestError(f"{context}.scheme must be 'authenticode' or 'none'")
    if scope not in {"artifact", "contents", "none"}:
        raise ManifestError(f"{context}.scope must be 'artifact', 'contents', or 'none'")

    signed_files = value.get("signedFiles")
    if scheme == "none":
        if scope != "none" or signed_files is not None:
            raise ManifestError(
                f"{context} with scheme 'none' must use scope 'none' and omit signedFiles"
            )
    elif scope == "none":
        raise ManifestError(f"{context} with Authenticode must not use scope 'none'")

    if scope == "contents":
        if (
            not isinstance(signed_files, list)
            or not signed_files
            or any(not isinstance(item, str) or not item for item in signed_files)
        ):
            raise ManifestError(
                f"{context}.signedFiles must be a non-empty string array for contents scope"
            )
        if len(set(signed_files)) != len(signed_files):
            raise ManifestError(f"{context}.signedFiles must not contain duplicates")
        for signed_file in signed_files:
            portable_path = PurePosixPath(signed_file)
            if (
                portable_path.is_absolute()
                or ".." in portable_path.parts
                or "\\" in signed_file
            ):
                raise ManifestError(
                    f"{context}.signedFiles entries must be safe relative paths "
                    "using '/' separators"
                )
    elif signed_files is not None:
        raise ManifestError(f"{context}.signedFiles is valid only for contents scope")

    normalized: dict[str, Any] = {"scheme": scheme, "scope": scope}
    if signed_files is not None:
        normalized["signedFiles"] = signed_files
    return normalized


def _load_spec(spec_path: Path) -> dict[str, Any]:
    spec = _load_object(spec_path, "manifest spec")
    _require_keys(
        spec,
        required={"specVersion", "minimumLauncherVersion", "artifacts"},
        context="manifest spec",
    )
    if type(spec["specVersion"]) is not int or spec["specVersion"] != SPEC_VERSION:
        raise ManifestError(
            f"unsupported manifest spec version {spec['specVersion']!r}; "
            f"expected {SPEC_VERSION}"
        )

    minimum_launcher_version = _require_string(
        spec["minimumLauncherVersion"],
        "manifest spec.minimumLauncherVersion",
    )
    if VERSION_PATTERN.fullmatch(minimum_launcher_version) is None:
        raise ManifestError(
            "manifest spec.minimumLauncherVersion must be a version such as 0.1.0"
        )

    artifacts = spec["artifacts"]
    if not isinstance(artifacts, list) or not artifacts:
        raise ManifestError("manifest spec.artifacts must be a non-empty array")

    normalized_artifacts: list[dict[str, Any]] = []
    artifact_ids: set[str] = set()
    for index, artifact in enumerate(artifacts):
        context = f"manifest spec.artifacts[{index}]"
        if not isinstance(artifact, dict):
            raise ManifestError(f"{context} must be an object")
        _require_keys(
            artifact,
            required={
                "id",
                "kind",
                "platform",
                "architecture",
                "sourcePath",
                "fileName",
                "mediaType",
                "authenticity",
            },
            context=context,
        )

        artifact_id = _require_string(artifact["id"], f"{context}.id")
        kind = _require_string(artifact["kind"], f"{context}.kind")
        platform = _require_string(artifact["platform"], f"{context}.platform")
        architecture = _require_string(
            artifact["architecture"],
            f"{context}.architecture",
        )
        source_path = _require_string(artifact["sourcePath"], f"{context}.sourcePath")
        file_name = _require_string(artifact["fileName"], f"{context}.fileName")
        media_type = _require_string(artifact["mediaType"], f"{context}.mediaType")

        if IDENTIFIER_PATTERN.fullmatch(artifact_id) is None:
            raise ManifestError(f"{context}.id must be a lowercase kebab-case identifier")
        if artifact_id in artifact_ids:
            raise ManifestError(f"duplicate artifact id: {artifact_id}")
        artifact_ids.add(artifact_id)
        if IDENTIFIER_PATTERN.fullmatch(kind) is None:
            raise ManifestError(f"{context}.kind must be a lowercase kebab-case identifier")
        if platform != "windows":
            raise ManifestError(f"{context}.platform must be 'windows' in schema v1")
        if architecture not in {"x64", "arm64", "neutral"}:
            raise ManifestError(
                f"{context}.architecture must be 'x64', 'arm64', or 'neutral'"
            )

        portable_path = PurePosixPath(source_path)
        if (
            portable_path.is_absolute()
            or ".." in portable_path.parts
            or "\\" in source_path
        ):
            raise ManifestError(
                f"{context}.sourcePath must be a safe relative path using '/' separators"
            )
        if "/" in file_name or "\\" in file_name or file_name in {".", ".."}:
            raise ManifestError(f"{context}.fileName must be a base file name")
        if portable_path.name != file_name:
            raise ManifestError(
                f"{context}.fileName must match the sourcePath base name"
            )

        normalized_artifacts.append(
            {
                "id": artifact_id,
                "kind": kind,
                "platform": platform,
                "architecture": architecture,
                "sourcePath": source_path,
                "fileName": file_name,
                "mediaType": media_type,
                "authenticity": _validate_authenticity(
                    artifact["authenticity"],
                    f"{context}.authenticity",
                ),
            }
        )

    return {
        "minimumLauncherVersion": minimum_launcher_version,
        "artifacts": normalized_artifacts,
    }


def _hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_manifest(
    *,
    tag: str,
    target_commit: str,
    repository: str,
    artifact_root: Path,
    spec_path: Path,
) -> dict[str, Any]:
    release_version, channel = _release_identity(tag)
    if COMMIT_PATTERN.fullmatch(target_commit) is None:
        raise ManifestError("target commit must be exactly 40 lowercase hexadecimal characters")
    if REPOSITORY_PATTERN.fullmatch(repository) is None:
        raise ManifestError("repository must use the owner/name form")

    root = artifact_root.resolve(strict=True)
    if not root.is_dir():
        raise ManifestError(f"artifact root is not a directory: {root}")
    spec = _load_spec(spec_path)

    artifacts: list[dict[str, Any]] = []
    for artifact in spec["artifacts"]:
        source = (root / PurePosixPath(artifact["sourcePath"])).resolve(strict=True)
        try:
            source.relative_to(root)
        except ValueError as exception:
            raise ManifestError(
                f"artifact source escapes the artifact root: {artifact['sourcePath']}"
            ) from exception
        if not source.is_file():
            raise ManifestError(f"artifact source is not a file: {artifact['sourcePath']}")
        size = source.stat().st_size
        if size <= 0:
            raise ManifestError(f"artifact source is empty: {artifact['sourcePath']}")

        artifacts.append(
            {
                "id": artifact["id"],
                "kind": artifact["kind"],
                "platform": artifact["platform"],
                "architecture": artifact["architecture"],
                "fileName": artifact["fileName"],
                "mediaType": artifact["mediaType"],
                "size": size,
                "sha256": _hash_file(source),
                "authenticity": artifact["authenticity"],
            }
        )

    return {
        "schemaVersion": MANIFEST_SCHEMA_VERSION,
        "releaseVersion": release_version,
        "tag": tag,
        "channel": channel,
        "releaseState": "active",
        "minimumLauncherVersion": spec["minimumLauncherVersion"],
        "source": {
            "repository": repository,
            "targetCommit": target_commit,
        },
        "manifestAuthenticity": {
            "scheme": "none",
        },
        "artifacts": artifacts,
    }


def write_manifest(manifest: dict[str, Any], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = output_path.with_name(f"{output_path.name}.tmp")
    temporary_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    temporary_path.replace(output_path)


def validate_manifest(
    *,
    manifest_path: Path,
    tag: str,
    target_commit: str,
    repository: str,
    artifact_root: Path,
    spec_path: Path,
) -> None:
    actual = _load_object(manifest_path, "release manifest")
    schema_version = actual.get("schemaVersion")
    if type(schema_version) is not int or schema_version != MANIFEST_SCHEMA_VERSION:
        raise ManifestError(
            f"unsupported release manifest schema version {schema_version!r}; "
            f"expected {MANIFEST_SCHEMA_VERSION}"
        )

    expected = build_manifest(
        tag=tag,
        target_commit=target_commit,
        repository=repository,
        artifact_root=artifact_root,
        spec_path=spec_path,
    )
    if actual != expected:
        raise ManifestError(
            "release manifest does not match the declared release metadata and artifact files"
        )

    for artifact in actual["artifacts"]:
        if (
            type(artifact["size"]) is not int
            or artifact["size"] <= 0
            or SHA256_PATTERN.fullmatch(artifact["sha256"]) is None
        ):
            raise ManifestError(
                f"release manifest artifact {artifact['id']!r} has invalid size or SHA-256"
            )


def _add_shared_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--tag", required=True)
    parser.add_argument("--target", required=True, dest="target_commit")
    parser.add_argument("--repository", required=True)
    parser.add_argument("--artifact-root", required=True, type=Path)
    parser.add_argument("--spec", required=True, type=Path)


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate_parser = subparsers.add_parser("generate")
    _add_shared_arguments(generate_parser)
    generate_parser.add_argument("--output", required=True, type=Path)

    validate_parser = subparsers.add_parser("validate")
    _add_shared_arguments(validate_parser)
    validate_parser.add_argument("--manifest", required=True, type=Path)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    try:
        if args.command == "generate":
            manifest = build_manifest(
                tag=args.tag,
                target_commit=args.target_commit,
                repository=args.repository,
                artifact_root=args.artifact_root,
                spec_path=args.spec,
            )
            write_manifest(manifest, args.output)
            print(f"Generated release manifest: {args.output}")
        else:
            validate_manifest(
                manifest_path=args.manifest,
                tag=args.tag,
                target_commit=args.target_commit,
                repository=args.repository,
                artifact_root=args.artifact_root,
                spec_path=args.spec,
            )
            print(f"Validated release manifest: {args.manifest}")
    except (ManifestError, OSError) as exception:
        print(f"error: {exception}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
