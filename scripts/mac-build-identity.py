#!/usr/bin/env python3
"""Generate or stamp a Mac launcher identity before code signing.

Release/Pre are explicit promotion decisions. Pre may identify an exact CI
candidate without a public tag. These labels do not attest a code signature.
"""

import argparse
import hashlib
import json
import os
import plistlib
import re
import stat
import struct
import tempfile
from pathlib import Path


CHANNEL_LABELS = {"ci": "CI", "dev": "DEV", "pre": "Pre", "release": "Release"}
BASE_PATTERN = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)")
TAG_PATTERN = re.compile(r"v((?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*))-guffa\.([1-9][0-9]*)")
IDENTITY_KEYS = (
    "CFBundleShortVersionString", "CFBundleVersion", "CFBundleGetInfoString",
    "STFCBaseVersion", "STFCDisplayVersion", "STFCBuildChannel", "STFCReleaseTag",
    "STFCSourceCommit", "STFCBuildRunID", "STFCBuildRunNumber", "STFCBuildRunAttempt", "STFCForkRevision",
)
RECEIPT_SCHEMA = "stfc-mac-build-identity/v1"


def numeric(value, label, minimum):
    if isinstance(value, bool) or not isinstance(value, (str, int)):
        raise ValueError(f"{label} must be an integer")
    text = str(value)
    if not re.fullmatch(r"0|[1-9][0-9]*", text) or int(text) < minimum or int(text) >= 2**63:
        raise ValueError(f"{label} must be an integer from {minimum} through {2**63 - 1}")
    return int(text)


def build_metadata(base_version, *, channel="ci", release_tag=None, source=None, fork_revision=None,
                   run_id=None, run_number=0, run_attempt=1):
    """Return all owned plist fields from explicit, validated build inputs."""
    if not isinstance(base_version, str) or not BASE_PATTERN.fullmatch(base_version):
        raise ValueError("base version must contain four canonical numeric components")
    if channel not in CHANNEL_LABELS:
        raise ValueError("channel must be ci, dev, pre, or release")
    if source is not None and (not isinstance(source, str) or not re.fullmatch(r"[0-9a-fA-F]{40}", source)):
        raise ValueError("source must be a full 40-character hexadecimal commit")
    run_number = numeric(run_number, "run number", 0)
    run_attempt = numeric(run_attempt, "run attempt", 1)
    run_id = "" if run_id is None else str(numeric(run_id, "run ID", 1))
    fork_revision = 0 if fork_revision is None else numeric(fork_revision, "fork revision", 0)
    version = f"{base_version}-guffa.{fork_revision}" if fork_revision else base_version
    parts = base_version.split(".")
    if channel in ("ci", "dev"):
        if release_tag is not None:
            raise ValueError(f"{CHANNEL_LABELS[channel]} builds must not supply a release tag")
        number = str(run_number) if run_number else run_id
        label = CHANNEL_LABELS[channel] + " " + (f"b{number}.{run_attempt}" if number else "local")
        bundle_version = f"{parts[3]}.{run_number}.{run_attempt}"
    elif channel == "pre" and release_tag is None:
        if not source or not run_id or run_number <= 0:
            raise ValueError("unnamed Pre requires source commit, positive run ID, and positive run number")
        label = f"Pre b{run_number}.{run_attempt}"
        bundle_version = f"{parts[3]}.{run_number}.{run_attempt}"
    else:
        match = TAG_PATTERN.fullmatch(release_tag) if isinstance(release_tag, str) else None
        if not match or match.group(1) != base_version:
            raise ValueError("Release/Pre requires a v<base-version>-guffa.<positive number> tag matching the base")
        fork_revision = numeric(match.group(2), "release revision", 1)
        version = release_tag[1:]
        label = CHANNEL_LABELS[channel]
        bundle_version = f"{parts[3]}.{match.group(2)}"
    display = f"{version} · {label}"
    return {
        "CFBundleShortVersionString": ".".join(parts[:3]),
        "CFBundleVersion": bundle_version,
        "CFBundleGetInfoString": display,
        "STFCBaseVersion": base_version,
        "STFCDisplayVersion": display,
        "STFCBuildChannel": channel,
        "STFCReleaseTag": release_tag or "",
        "STFCSourceCommit": source.lower() if source else "",
        "STFCBuildRunID": run_id,
        "STFCBuildRunNumber": run_number,
        "STFCBuildRunAttempt": run_attempt,
        "STFCForkRevision": fork_revision,
    }


def reject_placeholders(value):
    if isinstance(value, str) and "${" in value:
        raise ValueError("unsubstituted template placeholder in plist")
    if isinstance(value, dict):
        for key, item in value.items():
            reject_placeholders(key)
            reject_placeholders(item)
    elif isinstance(value, list):
        for item in value:
            reject_placeholders(item)


def validate_bundle_fields(plist):
    if not isinstance(plist, dict):
        raise ValueError("Info.plist must contain a dictionary")
    for key in ("CFBundleIdentifier", "CFBundleExecutable", "CFBundleName"):
        if not isinstance(plist.get(key), str) or not plist[key].strip():
            raise ValueError(f"{key} must be a nonempty string")
    executable = plist["CFBundleExecutable"]
    if executable in (".", "..") or "/" in executable or "\\" in executable:
        raise ValueError("CFBundleExecutable must be a filename")
    if plist.get("CFBundleIconFile") not in ("launcher", "launcher.icns"):
        raise ValueError("launcher bundle must use the fixed launcher.icns resource")
    if "CFBundleIconName" in plist:
        raise ValueError("CFBundleIconName would override the selected launcher.icns resource")


def validate_plist(plist):
    """Validate an already generated/stamped plist; return its canonical identity."""
    validate_bundle_fields(plist)
    reject_placeholders(plist)
    for key in IDENTITY_KEYS:
        expected_type = int if key in ("STFCBuildRunNumber", "STFCBuildRunAttempt", "STFCForkRevision") else str
        if type(plist.get(key)) is not expected_type:
            raise ValueError(f"{key} has a missing or invalid value type")
    expected = build_metadata(
        plist["STFCBaseVersion"], channel=plist["STFCBuildChannel"],
        release_tag=plist["STFCReleaseTag"] or None, source=plist["STFCSourceCommit"] or None,
        run_id=plist["STFCBuildRunID"] or None, run_number=plist["STFCBuildRunNumber"],
        run_attempt=plist["STFCBuildRunAttempt"],
        fork_revision=plist["STFCForkRevision"],
    )
    for key, value in expected.items():
        if plist[key] != value:
            raise ValueError(f"{key} does not match the declared build identity")
    return expected


def regular_file(path):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or not stat.S_ISREG(path.stat().st_mode):
        raise ValueError(f"expected a regular file, not a symlink: {path}")
    return path


def directory(path):
    path = Path(path)
    if path.is_symlink() or getattr(path, "is_junction", lambda: False)() or not path.is_dir():
        raise ValueError(f"expected a directory, not a symlink: {path}")
    return path


def writable_target(path):
    path = Path(path)
    directory(path.parent)
    if path.is_symlink() or path.exists():
        regular_file(path)
    return path


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_plist(path):
    raw = regular_file(path).read_bytes()
    if len(raw) > 1024 * 1024:
        raise ValueError("Info.plist is unexpectedly large")
    try:
        value = plistlib.loads(raw)
    except (plistlib.InvalidFileException, ValueError, TypeError) as error:
        raise ValueError("invalid Info.plist") from error
    validate_bundle_fields(value)
    return raw, value


def icon_bytes(path):
    raw = regular_file(path).read_bytes()
    if len(raw) < 16 or raw[:4] != b"icns" or struct.unpack(">I", raw[4:8])[0] != len(raw):
        raise ValueError("selected icon is not an ICNS container")
    position = 8
    while position < len(raw):
        if len(raw) - position < 8:
            raise ValueError("truncated ICNS element")
        length = struct.unpack(">I", raw[position + 4:position + 8])[0]
        if length < 8 or position + length > len(raw):
            raise ValueError("invalid ICNS element length")
        position += length
    return raw


def replace_files(updates):
    """Prepare every output before replacing any destination; replace each atomically."""
    staged = []
    try:
        for path, data in updates:
            path = writable_target(path)
            mode = stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o644
            with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".stfc-identity-", delete=False) as file:
                temporary = Path(file.name)
                staged.append((temporary, path))
                file.write(data)
                file.flush()
                os.fsync(file.fileno())
            os.chmod(temporary, mode)
        for temporary, path in staged:
            os.replace(temporary, path)
    finally:
        for temporary, _ in staged:
            if temporary.exists():
                temporary.unlink()


def generate(template, output, *, base_version, macos_min, **identity):
    if not isinstance(macos_min, str) or not re.fullmatch(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", macos_min):
        raise ValueError("minimum macOS version must be numeric")
    _, plist = read_plist(template)
    plist.update(build_metadata(base_version, **identity))
    plist["LSMinimumSystemVersion"] = macos_min
    validate_plist(plist)
    output = writable_target(output)
    if output.resolve() == Path(template).resolve():
        raise ValueError("generated output must not replace its template")
    replace_files([(output, plistlib.dumps(plist, sort_keys=False))])
    return plist


def describe(output, *, base_version, **identity):
    """Write the same identity without requiring a Mac app or artwork."""
    fields = build_metadata(base_version, **identity)
    replace_files([(writable_target(output),
                    (json.dumps(fields, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))])
    return fields


def stamp_app(app, *, icons_dir, receipt, **identity):
    app = directory(app)
    contents = directory(app / "Contents")
    resources = directory(contents / "Resources")
    macos = directory(contents / "MacOS")
    plist_path = writable_target(contents / "Info.plist")
    old_plist, plist = read_plist(plist_path)
    if "STFCBaseVersion" in plist:
        validate_plist(plist)
        base = plist["STFCBaseVersion"]
        producing_revision = plist["STFCForkRevision"]
    else:
        if any(key.startswith("STFC") for key in plist):
            raise ValueError("incomplete existing STFC identity")
        base = plist.get("CFBundleShortVersionString")
        if not isinstance(base, str) or not BASE_PATTERN.fullmatch(base) or plist.get("CFBundleVersion") != base:
            raise ValueError("legacy launcher must declare the same four-part numeric version in both version fields")
        reject_placeholders(plist)
        producing_revision = 0
    if identity.get("release_tag") is None:
        requested_revision = identity.get("fork_revision")
        if (requested_revision is not None
                and numeric(requested_revision, "fork revision", 0) != producing_revision):
            raise ValueError("unnamed promotion must retain the producing app's fork revision")
        identity["fork_revision"] = producing_revision
    fields = build_metadata(base, **identity)
    plist.update(fields)
    validate_plist(plist)
    icons_dir = directory(icons_dir)
    selected_icon = icons_dir / f"launcher-{fields['STFCBuildChannel']}.icns"
    new_icon = icon_bytes(selected_icon)
    icon_path = writable_target(resources / "launcher.icns")
    old_icon = icon_bytes(icon_path)
    binaries = (macos / plist["CFBundleExecutable"], contents / "stfc-community-mod-loader",
                contents / "libstfc-community-mod.dylib")
    binary_hashes = {path.relative_to(contents).as_posix(): digest(regular_file(path).read_bytes()) for path in binaries}
    new_plist = plistlib.dumps(plist, sort_keys=False)
    receipt = writable_target(receipt)
    protected = (plist_path, icon_path, *binaries, selected_icon)
    if receipt.resolve() in {path.resolve() for path in protected}:
        raise ValueError("receipt must not replace a bundle payload or selected icon")
    record = {
        "schema": RECEIPT_SCHEMA,
        "app": str(app.absolute()),
        "identity": fields,
        "selectedIcon": selected_icon.name,
        "files": {
            "Info.plist": {"beforeSha256": digest(old_plist), "afterSha256": digest(new_plist)},
            "Resources/launcher.icns": {"beforeSha256": digest(old_icon), "afterSha256": digest(new_icon)},
        },
        "binarySha256": binary_hashes,
    }
    prior = None
    if receipt.exists():
        prior = json.loads(receipt.read_text(encoding="utf-8"))
        if (not isinstance(prior, dict) or prior.get("schema") != RECEIPT_SCHEMA
                or prior.get("app") != record["app"] or not isinstance(prior.get("files"), dict)
                or any(not isinstance(prior["files"].get(name), dict)
                       or any(not isinstance(prior["files"][name].get(key), str)
                              or not re.fullmatch(r"[0-9a-f]{64}", prior["files"][name][key])
                              for key in ("beforeSha256", "afterSha256"))
                       for name in record["files"])):
            raise ValueError("existing receipt is not an identity receipt for this app")
    if old_plist == new_plist and old_icon == new_icon and prior is not None:
        if (prior.get("identity") == fields
                and prior.get("selectedIcon") == selected_icon.name and prior.get("binarySha256") == binary_hashes
                and all(prior.get("files", {}).get(name, {}).get("afterSha256") == hashes["afterSha256"]
                        for name, hashes in record["files"].items())):
            return prior
    replace_files([(icon_path, new_icon), (plist_path, new_plist),
                   (receipt, (json.dumps(record, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))])
    return record


def identity_arguments(parser):
    parser.add_argument("--channel", choices=CHANNEL_LABELS, default="ci")
    parser.add_argument("--release-tag")
    parser.add_argument("--fork-revision", help="declared producing fork revision; zero means unknown")
    parser.add_argument("--source")
    parser.add_argument("--run-id")
    parser.add_argument("--run-number", default="0")
    parser.add_argument("--run-attempt", default="1")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    generate_parser = commands.add_parser("generate", help="generate a validated launcher Info.plist")
    identity_arguments(generate_parser)
    generate_parser.add_argument("--template", type=Path, required=True)
    generate_parser.add_argument("--base-version", required=True)
    generate_parser.add_argument("--macos-min", required=True)
    generate_parser.add_argument("--output", type=Path, required=True)
    stamp_parser = commands.add_parser("stamp", help="stamp an extracted app before signing")
    identity_arguments(stamp_parser)
    stamp_parser.add_argument("--app", type=Path, required=True)
    stamp_parser.add_argument("--icons-dir", type=Path, required=True)
    stamp_parser.add_argument("--receipt", type=Path, required=True)
    describe_parser = commands.add_parser("describe", help="write canonical build identity JSON without an app")
    identity_arguments(describe_parser)
    describe_parser.add_argument("--base-version", required=True)
    describe_parser.add_argument("--output", type=Path, required=True)
    args = vars(parser.parse_args())
    command = args.pop("command")
    try:
        result = (generate(**args) if command == "generate" else
                  stamp_app(**args) if command == "stamp" else describe(**args))
    except (OSError, ValueError, TypeError, OverflowError) as error:
        parser.error(str(error))
    fields = result["identity"] if command == "stamp" else result
    print(fields["STFCDisplayVersion"])


if __name__ == "__main__":
    main()
