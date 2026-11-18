#!/usr/bin/env python3
"""Check a launcher's numeric versions and full declared build identity."""
import argparse
import importlib.util
import plistlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("mac_build_identity", ROOT / "scripts/mac-build-identity.py")
IDENTITY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(IDENTITY)


def header_version() -> str:
    """The dotted MAJOR.MINOR.REVISION.PATCH version from src/mod/version.h."""
    header = (ROOT / "src/mod/version.h").read_text()
    parts = []
    for component in ("MAJOR", "MINOR", "REVISION", "PATCH"):
        match = re.search(rf"^#define\s+VERSION_{component}\s+(\d+)\s*$", header, re.MULTILINE)
        if not match:
            raise SystemExit(f"Missing VERSION_{component}")
        parts.append(match.group(1))
    return ".".join(parts)


def header_fork_revision() -> int:
    header = (ROOT / "src/mod/version.h").read_text()
    match = re.search(r"^#define\s+VERSION_FORK_REVISION\s+([1-9][0-9]*)\s*$", header, re.MULTILINE)
    if not match:
        raise SystemExit("Missing VERSION_FORK_REVISION")
    return int(match.group(1))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plist", nargs="?", default="zig-out/macOSLauncher.app/Contents/Info.plist")
    parser.add_argument("--base-version", help="expected four-part core version; defaults to the version header")
    parser.add_argument("--channel", choices=IDENTITY.CHANNEL_LABELS, help="expected build channel")
    parser.add_argument("--release-tag", help="expected full promotion tag")
    parser.add_argument("--source", help="expected full source commit")
    parser.add_argument("--fork-revision", type=int, help="expected fork revision; defaults to the version header")
    args = parser.parse_args()

    version = args.base_version or header_version()
    try:
        _, plist = IDENTITY.read_plist(Path(args.plist))
        identity = IDENTITY.validate_plist(plist)
    except (OSError, ValueError, TypeError, plistlib.InvalidFileException) as error:
        raise SystemExit(str(error)) from error
    expected = {"STFCBaseVersion": version}
    expected["STFCForkRevision"] = args.fork_revision if args.fork_revision is not None else header_fork_revision()
    if args.channel is not None:
        expected["STFCBuildChannel"] = args.channel
    if args.release_tag is not None:
        expected["STFCReleaseTag"] = args.release_tag
    if args.source is not None:
        if not re.fullmatch(r"[0-9a-fA-F]{40}", args.source):
            raise SystemExit("source must be a full 40-character hexadecimal commit")
        expected["STFCSourceCommit"] = args.source.lower()
    for key, value in expected.items():
        if identity[key] != value:
            raise SystemExit(f"{key}: expected {value}, found {identity[key]!r}")
    print(f"Verified launcher {identity['STFCDisplayVersion']} (bundle {identity['CFBundleVersion']})")


if __name__ == "__main__":
    main()
