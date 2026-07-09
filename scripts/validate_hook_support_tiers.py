#!/usr/bin/env python3
"""Validate hook support-tier manifest and release config exposure.

The manifest is the durable source for production/science/dormant/internal
support-tier decisions. This script keeps that inventory tied to the code and
to the release-facing example config.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import Any

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - Python < 3.11 fallback path.
    tomllib = None  # type: ignore[assignment]


DEFAULT_MANIFEST = "manifests/hook_support_tiers.json"
DEFAULT_RELEASE_CONFIG = "example_community_patch_settings.toml"
DEFAULT_SCIENCE_CONFIG = "example_science_patch_settings.toml"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".mm"}
HOOK_MODULE_PATTERN = re.compile(r'HookModuleHealth\s+hooks\s*\(\s*"([^"]+)"\s*\)')


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as input_file:
        data = json.load(input_file)
    if not isinstance(data, dict):
        raise ValueError(f"manifest must be a JSON object: {path}")
    return data


def load_toml(path: Path) -> dict[str, Any]:
    if tomllib is None:
        raise RuntimeError("Python 3.11+ tomllib is required to parse TOML")
    with path.open("rb") as input_file:
        data = tomllib.load(input_file)
    if not isinstance(data, dict):
        raise ValueError(f"TOML root must be a table: {path}")
    return data


def path_exists(config: dict[str, Any], dotted_path: str) -> bool:
    current: Any = config
    for segment in dotted_path.split("."):
        if not isinstance(current, dict) or segment not in current:
            return False
        current = current[segment]
    return True


def iter_source_files(repo_root: Path) -> list[Path]:
    source_root = repo_root / "mods" / "src"
    if not source_root.exists():
        return []
    return sorted(path for path in source_root.rglob("*") if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES)


def discover_hook_modules(repo_root: Path) -> list[dict[str, Any]]:
    modules: list[dict[str, Any]] = []
    for path in iter_source_files(repo_root):
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            text = path.read_text(encoding="utf-8", errors="replace")

        for line_number, line in enumerate(text.splitlines(), start=1):
            match = HOOK_MODULE_PATTERN.search(line)
            if not match:
                continue
            modules.append(
                {
                    "id": match.group(1),
                    "path": path.relative_to(repo_root).as_posix(),
                    "line": line_number,
                }
            )
    return modules


def add_issue(issues: list[dict[str, str]], code: str, path: str, message: str) -> None:
    issues.append({"code": code, "path": path, "message": message})


def validate_manifest(
    repo_root: Path,
    manifest: dict[str, Any],
    release_config: dict[str, Any],
    science_config: dict[str, Any],
) -> dict[str, Any]:
    errors: list[dict[str, str]] = []
    warnings: list[dict[str, str]] = []

    if manifest.get("schema_version") != 1:
        add_issue(errors, "schema-version", "schema_version", "expected schema_version 1")

    tiers_node = manifest.get("tiers")
    tiers = set(tiers_node.keys()) if isinstance(tiers_node, dict) else set()
    required_tiers = {"production", "science", "dormant", "internal"}
    if tiers != required_tiers:
        add_issue(errors, "tier-set", "tiers", f"expected tiers {sorted(required_tiers)}, found {sorted(tiers)}")

    hook_modules = manifest.get("hook_modules")
    if not isinstance(hook_modules, list):
        add_issue(errors, "hook-modules-type", "hook_modules", "expected list")
        hook_modules = []

    manifest_hook_ids: set[str] = set()
    manifest_hook_tiers: Counter[str] = Counter()
    for index, entry in enumerate(hook_modules):
        entry_path = f"hook_modules[{index}]"
        if not isinstance(entry, dict):
            add_issue(errors, "hook-module-entry-type", entry_path, "expected object")
            continue

        module_id = entry.get("id")
        tier = entry.get("tier")
        source = entry.get("source")
        if not isinstance(module_id, str) or not module_id:
            add_issue(errors, "hook-module-id", entry_path, "missing id")
        elif module_id in manifest_hook_ids:
            add_issue(errors, "hook-module-duplicate", entry_path, f"duplicate hook module id {module_id}")
        else:
            manifest_hook_ids.add(module_id)

        if tier not in required_tiers:
            add_issue(errors, "hook-module-tier", entry_path, f"invalid tier {tier!r}")
        else:
            manifest_hook_tiers[str(tier)] += 1

        if not isinstance(source, str) or not source:
            add_issue(errors, "hook-module-source", entry_path, "missing source")
        elif not (repo_root / source).exists():
            add_issue(errors, "hook-module-source-missing", entry_path, f"source does not exist: {source}")
        elif tier in {"science", "dormant", "internal"}:
            source_text = (repo_root / source).read_text(encoding="utf-8", errors="replace")
            expected_token = "HookSupportTier::" + str(tier).capitalize()
            if expected_token not in source_text:
                add_issue(
                    errors,
                    "hook-module-source-tier-missing",
                    entry_path,
                    f"non-production module source must contain {expected_token}",
                )

    discovered_modules = discover_hook_modules(repo_root)
    discovered_ids = {str(module["id"]) for module in discovered_modules}
    for module in discovered_modules:
        if module["id"] not in manifest_hook_ids:
            add_issue(
                errors,
                "hook-module-unmanifested",
                f"{module['path']}:{module['line']}",
                f"HookModuleHealth module is missing from manifest: {module['id']}",
            )
    for module_id in sorted(manifest_hook_ids - discovered_ids):
        add_issue(warnings, "hook-module-not-discovered", module_id, "manifest entry has no HookModuleHealth site")

    config_surfaces = manifest.get("config_surfaces")
    if not isinstance(config_surfaces, list):
        add_issue(errors, "config-surfaces-type", "config_surfaces", "expected list")
        config_surfaces = []

    surface_paths: set[str] = set()
    config_tiers: Counter[str] = Counter()
    blocked_release_paths: list[str] = []
    for index, entry in enumerate(config_surfaces):
        entry_path = f"config_surfaces[{index}]"
        if not isinstance(entry, dict):
            add_issue(errors, "config-surface-entry-type", entry_path, "expected object")
            continue

        path = entry.get("path")
        tier = entry.get("tier")
        release_allowed = entry.get("release_example_allowed")
        science_required = entry.get("science_example_required")

        if not isinstance(path, str) or not path:
            add_issue(errors, "config-surface-path", entry_path, "missing path")
            continue
        if path in surface_paths:
            add_issue(errors, "config-surface-duplicate", entry_path, f"duplicate config surface path {path}")
        surface_paths.add(path)

        if tier not in required_tiers:
            add_issue(errors, "config-surface-tier", path, f"invalid tier {tier!r}")
        else:
            config_tiers[str(tier)] += 1

        if not isinstance(release_allowed, bool):
            add_issue(errors, "config-surface-release-allowed", path, "release_example_allowed must be boolean")
            continue

        if not release_allowed:
            blocked_release_paths.append(path)
            if path_exists(release_config, path):
                add_issue(
                    errors,
                    "non-production-release-example",
                    path,
                    "non-production config surface appears in example_community_patch_settings.toml",
                )

        if science_required is True and not path_exists(science_config, path):
            add_issue(
                errors,
                "science-example-missing",
                path,
                "config surface is missing from example_science_patch_settings.toml",
            )

    kirshara_module = next((entry for entry in hook_modules if isinstance(entry, dict) and entry.get("id") == "KirsharaQueueRepairHooks"), None)
    if not kirshara_module:
        add_issue(errors, "kirshara-module-missing", "hook_modules", "KirsharaQueueRepairHooks must be manifest-tracked")
    elif kirshara_module.get("tier") != "dormant":
        add_issue(errors, "kirshara-module-tier", "KirsharaQueueRepairHooks", "Kirshara queue repair must remain dormant")

    for required_path in ("advanced.kirshara_queue", "advanced.diagnostics.kirshara_queue"):
        surface = next((entry for entry in config_surfaces if isinstance(entry, dict) and entry.get("path") == required_path), None)
        if not surface:
            add_issue(errors, "kirshara-config-surface-missing", required_path, "Kir'Shara dormant config path missing")
        elif surface.get("tier") != "dormant" or surface.get("release_example_allowed") is not False:
            add_issue(
                errors,
                "kirshara-config-surface-tier",
                required_path,
                "Kir'Shara config path must be dormant and blocked from release example config",
            )

    return {
        "ok": not errors,
        "summary": {
            "hook_modules": len(hook_modules),
            "discovered_hook_modules": len(discovered_modules),
            "config_surfaces": len(config_surfaces),
            "blocked_release_config_surfaces": len(blocked_release_paths),
            "hook_modules_by_tier": dict(sorted(manifest_hook_tiers.items())),
            "config_surfaces_by_tier": dict(sorted(config_tiers.items())),
        },
        "errors": errors,
        "warnings": warnings,
        "blocked_release_config_surfaces": blocked_release_paths,
        "discovered_hook_modules": discovered_modules,
    }


def print_text_report(result: dict[str, Any]) -> None:
    summary = result["summary"]
    print("Hook support-tier manifest")
    print(f"  hook modules: {summary['hook_modules']} manifest / {summary['discovered_hook_modules']} discovered")
    print(f"  config surfaces: {summary['config_surfaces']}")
    print(f"  blocked release config surfaces: {summary['blocked_release_config_surfaces']}")
    print(f"  hook modules by tier: {summary['hook_modules_by_tier']}")
    print(f"  config surfaces by tier: {summary['config_surfaces_by_tier']}")

    if result["errors"]:
        print("")
        print("Errors:")
        for issue in result["errors"]:
            print(f"  {issue['path']} [{issue['code']}] {issue['message']}")

    if result["warnings"]:
        print("")
        print("Warnings:")
        for issue in result["warnings"]:
            print(f"  {issue['path']} [{issue['code']}] {issue['message']}")


def run_self_test() -> int:
    manifest = {
        "schema_version": 1,
        "tiers": {
            "production": {},
            "science": {},
            "dormant": {},
            "internal": {},
        },
        "hook_modules": [
            {
                "id": "KirsharaQueueRepairHooks",
                "tier": "dormant",
                "source": "mods/src/patches/parts/action_queue_repair.cc",
            }
        ],
        "config_surfaces": [
            {
                "path": "advanced.kirshara_queue",
                "tier": "dormant",
                "release_example_allowed": False,
                "science_example_required": True,
            },
            {
                "path": "advanced.diagnostics.kirshara_queue",
                "tier": "dormant",
                "release_example_allowed": False,
                "science_example_required": True,
            },
        ],
    }

    with tempfile.TemporaryDirectory() as temp_dir:
        repo_root = Path(temp_dir)
        source = repo_root / "mods" / "src" / "patches" / "parts"
        source.mkdir(parents=True)
        (source / "action_queue_repair.cc").write_text(
            'constexpr auto tier = HookSupportTier::Dormant;\nvoid f(){ HookModuleHealth hooks("KirsharaQueueRepairHooks"); }\n',
            encoding="utf-8",
        )

        clean_release = {}
        science = {
            "advanced": {
                "kirshara_queue": {},
                "diagnostics": {"kirshara_queue": {}},
            }
        }
        clean = validate_manifest(repo_root, manifest, clean_release, science)
        if not clean["ok"]:
            print_text_report(clean)
            print("self-test failed: clean fixture should pass")
            return 1

        dirty_release = {"advanced": {"kirshara_queue": {}}}
        dirty = validate_manifest(repo_root, manifest, dirty_release, science)
        if dirty["ok"] or not any(issue["code"] == "non-production-release-example" for issue in dirty["errors"]):
            print_text_report(dirty)
            print("self-test failed: release fixture should reject dormant config surface")
            return 1

    print("self-test passed: hook support-tier manifest validation rejects dormant release config exposure")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root. Defaults to current directory.")
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST, help="Hook support-tier manifest path.")
    parser.add_argument("--release-config", default=DEFAULT_RELEASE_CONFIG, help="Release-facing example TOML path.")
    parser.add_argument("--science-config", default=DEFAULT_SCIENCE_CONFIG, help="Science/dormant example TOML path.")
    parser.add_argument("--format", choices=("text", "json"), default="text", help="Report format.")
    parser.add_argument("--self-test", action="store_true", help="Run fixture self-test.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.self_test:
        return run_self_test()

    repo_root = Path(args.root).resolve()
    manifest_path = (repo_root / args.manifest).resolve()
    release_config_path = (repo_root / args.release_config).resolve()
    science_config_path = (repo_root / args.science_config).resolve()

    manifest = load_json(manifest_path)
    release_config = load_toml(release_config_path)
    science_config = load_toml(science_config_path)
    result = validate_manifest(repo_root, manifest, release_config, science_config)

    if args.format == "json":
        print(json.dumps(result, indent=2))
    else:
        print_text_report(result)

    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
