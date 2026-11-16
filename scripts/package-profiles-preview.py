#!/usr/bin/env python3
"""Stage a matching mod, Profiles helper, notices and source receipt for preview testers."""

import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("windows", "macos"), required=True)
    parser.add_argument("--prefix", type=Path, required=True, help="Zig install prefix supplying notices")
    parser.add_argument("--app", type=Path, help="Assembled and signed universal Mac app")
    parser.add_argument("--output", type=Path, required=True, help="New staging directory")
    parser.add_argument("--source", required=True, help="Full mod source commit")
    parser.add_argument("--base", required=True, help="Full mod base commit")
    parser.add_argument("--run", required=True, help="GitHub Actions run ID and attempt")
    args = parser.parse_args()
    for value in (args.source, args.base):
        if not re.fullmatch(r"[0-9a-f]{40}", value):
            parser.error("source and base must be full lowercase commit IDs")
    if not re.fullmatch(r"[0-9]+:[0-9]+", args.run):
        parser.error("run must be RUN_ID:ATTEMPT")
    if args.output.exists():
        parser.error("output must be a new staging directory")
    if args.platform == "macos" and not args.app:
        parser.error("--app is required on macOS")

    zon = (ROOT / "build.zig.zon").read_text()
    dependency = re.search(r"\.stfc_profiles\s*=\s*\.\{(.*?)\n\s*\},", zon, re.S)
    if dependency is None:
        raise SystemExit("Missing Profiles dependency pin")
    revision = re.search(r"/archive/([0-9a-f]{40})\.tar\.gz", dependency[1])[1]
    archive_hash = re.search(r'\.hash\s*=\s*"([^"]+)"', dependency[1])[1]
    source_file = args.prefix / "share/stfc-profiles/source.txt"
    if source_file.read_text().splitlines() != ["Guffawaffle/stfc-profiles", revision]:
        raise SystemExit("Installed Profiles source receipt does not match the dependency pin")

    if args.platform == "windows":
        required = ("bin/version.dll", "bin/stfc-profiles.exe")
        for name in required:
            if not (args.prefix / name).is_file():
                raise SystemExit(f"Missing {name}")
    else:
        required = ("MacOS/macOSLauncher", "stfc-community-mod-loader", "libstfc-community-mod.dylib", "stfc-profiles")
        for name in required:
            if not (args.app / "Contents" / name).is_file():
                raise SystemExit(f"Incomplete Profiles app: missing {name}")

    args.output.mkdir(parents=True)
    if args.platform == "windows":
        for name in required:
            shutil.copy2(args.prefix / name, args.output / Path(name).name)
    else:
        shutil.copytree(args.app, args.output / "STFC Community Mod.app", symlinks=True)
    shutil.copy2(ROOT / "docs/PROFILES_PREVIEW.md", args.output / "README.md")
    notices = args.output / "notices"
    notices.mkdir()
    shutil.copy2(ROOT / "LICENSE", notices / "mod-LICENSE")
    shutil.copy2(ROOT / "docs/THIRD_PARTY.md", notices / "THIRD_PARTY.md")
    shutil.copytree(args.prefix / "share/stfc-profiles", notices / "profiles")

    files = {}
    for path in sorted(args.output.rglob("*")):
        relative = path.relative_to(args.output).as_posix()
        if path.is_symlink():
            files[relative] = {"symlink": path.readlink().as_posix()}
        elif path.is_file():
            files[relative] = {"bytes": path.stat().st_size, "sha256": sha256(path)}
    receipt = {
        "schemaVersion": 1,
        "purpose": "Profiles integration test preview; not a production release",
        "mod": {"source": args.source, "base": args.base, "buildChannel": "ci"},
        "profiles": {"repository": "Guffawaffle/stfc-profiles", "revision": revision, "zigArchiveHash": archive_hash},
        "build": {
            "run": args.run,
            "zig": "0.16.0",
            "platform": args.platform,
            "architectures": ["x86_64"] if args.platform == "windows" else ["arm64", "x86_64"],
            "macosMinimum": "14.6" if args.platform == "macos" else None,
            "signing": "unsigned Windows test build" if args.platform == "windows" else "ad hoc; not notarized",
            "sourceStamp": "VERSION_COMMIT_HASH generated from mod.source",
        },
        "qualification": {
            "twoAccountLiveSmoke": "pending on each tester platform",
            "currentMacClientHookFit": "unverified; native build does not establish runtime fit",
            "scope": "Isolated PlayerPrefs and sign-in browser; not a complete game-file sandbox",
        },
        "files": files,
    }
    (args.output / "manifest.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"Staged {args.platform} Profiles preview: {len(files)} files; component {revision}")


if __name__ == "__main__":
    main()
