from __future__ import annotations

import json
import re
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
IDENTITY_PATH = Path("manifests/runtime_identity.v1.json")
CAPABILITIES_PATH = Path("docs/battle-bridge-producer-capabilities.v1.json")

EXPECTED_IDENTITY = {
    "distributionId": "guffawaffle.stfc-community-mod",
    "displayName": "Guffawaffle STFC Mod",
    "unofficialLabel": "Unofficial downstream build",
}
EXPECTED_BUILD_CLASSES = {
    "release": "Maintained fork release",
    "test": "Unofficial test build",
    "development": "Unofficial development build",
    "local": "Local build",
}

FORBIDDEN_RUNTIME_TEXT = (
    "Official Community Mod",
    "Official CC",
    "Supported by the Community Mod",
    "official-cc-logo",
    "STFCCCLogo",
    "CreateCCLogoOverlay",
)
REMOVED_PATHS = (
    Path("assets/official-cc-logo.png"),
    Path("mods/src/patches/parts/embedded_cc_logo_image.h"),
    Path("mods/src/patches/parts/embedded_logo_image.h"),
)
RUNTIME_FILES = (
    Path("mods/src"),
    Path("mods/xmake.lua"),
    Path("win-proxy-dll/src/version.rc"),
    Path("win-proxy-dll/xmake.lua"),
    Path("macos-dylib/xmake.lua"),
    Path("xmake.lua"),
)


def _load_json(path: Path, errors: list[str]) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"{path}: cannot load JSON: {exc}")
        return {}
    if not isinstance(value, dict):
        errors.append(f"{path}: root must be an object")
        return {}
    return value


def _iter_runtime_sources(root: Path):
    for relative in RUNTIME_FILES:
        candidate = root / relative
        if candidate.is_dir():
            yield from (
                path
                for path in candidate.rglob("*")
                if path.is_file() and path.suffix.lower() in {".cc", ".h", ".lua"}
            )
        elif candidate.is_file():
            yield candidate


def validate_identity(identity: dict[str, object]) -> list[str]:
    errors: list[str] = []

    if identity.get("schemaVersion") != 1:
        errors.append(f"{IDENTITY_PATH}: schemaVersion must be 1")
    for key, expected in EXPECTED_IDENTITY.items():
        if identity.get(key) != expected:
            errors.append(f"{IDENTITY_PATH}: {key} must be {expected!r}")

    upstream = identity.get("upstream")
    if not isinstance(upstream, dict):
        errors.append(f"{IDENTITY_PATH}: upstream must be an object")
    else:
        commit = upstream.get("commit")
        if upstream.get("repository") != "netniV/stfc-mod":
            errors.append(f"{IDENTITY_PATH}: upstream.repository must be 'netniV/stfc-mod'")
        if not isinstance(commit, str) or re.fullmatch(r"[0-9a-f]{40}", commit) is None:
            errors.append(f"{IDENTITY_PATH}: upstream.commit must be a lowercase 40-character SHA")

    build_classes = identity.get("buildClasses")
    if not isinstance(build_classes, dict) or set(build_classes) != set(EXPECTED_BUILD_CLASSES):
        errors.append(f"{IDENTITY_PATH}: buildClasses must be exactly {sorted(EXPECTED_BUILD_CLASSES)}")
    else:
        for build_class, expected_label in EXPECTED_BUILD_CLASSES.items():
            entry = build_classes.get(build_class)
            if not isinstance(entry, dict) or entry.get("label") != expected_label:
                errors.append(
                    f"{IDENTITY_PATH}: buildClasses.{build_class}.label must be {expected_label!r}"
                )

    manifest_text = json.dumps(identity, ensure_ascii=True)
    for forbidden in FORBIDDEN_RUNTIME_TEXT:
        if forbidden.casefold() in manifest_text.casefold():
            errors.append(f"{IDENTITY_PATH}: contains forbidden runtime branding {forbidden!r}")

    return errors


def validate_repository(root: Path = REPOSITORY_ROOT) -> list[str]:
    errors: list[str] = []
    identity = _load_json(root / IDENTITY_PATH, errors)
    errors.extend(validate_identity(identity))

    capabilities = _load_json(root / CAPABILITIES_PATH, errors)
    if capabilities.get("distributionId") != identity.get("distributionId"):
        errors.append(f"{CAPABILITIES_PATH}: distributionId must match the canonical runtime identity")

    for relative in REMOVED_PATHS:
        if (root / relative).exists():
            errors.append(f"{relative}: removed official-looking runtime asset must stay absent")

    for source in _iter_runtime_sources(root):
        text = source.read_text(encoding="utf-8", errors="replace")
        for forbidden in FORBIDDEN_RUNTIME_TEXT:
            if forbidden.casefold() in text.casefold():
                errors.append(f"{source.relative_to(root)}: contains forbidden runtime branding {forbidden!r}")

    mods_build = (root / "mods/xmake.lua").read_text(encoding="utf-8")
    for implicit_asset in ("assets/loadingscreen.png", "assets/stfc-mod-logo.png"):
        if implicit_asset in mods_build.replace("\\", "/"):
            errors.append(f"mods/xmake.lua: must not implicitly embed {implicit_asset}")

    return errors


def main() -> int:
    errors = validate_repository()
    if errors:
        for error in errors:
            print(f"runtime identity policy: {error}", file=sys.stderr)
        return 1
    print("runtime identity policy: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
