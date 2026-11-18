#!/usr/bin/env python3
"""Stage or verify the matching Windows Profiles download without executing it."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parent.parent
PROFILE_FILES = {"source.txt", "LICENSE"}
PROFILE_FILES.update("notices/windows/libarchive/" + name for name in ("COPYING", "BSD-3-Clause-notices.txt"))
PROFILE_FILES.update("notices/windows/xz/" + name for name in
                     ("COPYING", "COPYING.0BSD", "COPYING.GPLv2", "COPYING.GPLv3", "COPYING.LGPLv2.1", "AUTHORS"))
PROFILE_FILES.add("notices/windows/zlib/LICENSE")
PAYLOAD_FILES = {"version.dll", "stfc-profiles.exe", "notices/mod-LICENSE", "notices/THIRD_PARTY.md"}
PAYLOAD_FILES.update("notices/profiles/" + name for name in PROFILE_FILES)
ARCHIVE_FILES = PAYLOAD_FILES | {"manifest.json"}


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def pin():
    zon = (ROOT / "build.zig.zon").read_text(encoding="utf-8")
    dependency = re.search(r"\.stfc_profiles\s*=\s*\.\{(.*?)\n\s*\},", zon, re.S)
    revision = re.search(r"/archive/([0-9a-f]{40})\.tar\.gz", dependency[1])[1]
    declared = re.search(r'pub const revision = "([0-9a-f]{40})";',
                         (ROOT / "zig/profiles.zig").read_text(encoding="utf-8"))[1]
    if revision != declared:
        raise ValueError("Profiles package and build pin disagree")
    return revision


def identity(source, base, run):
    if not all(re.fullmatch(r"[0-9a-f]{40}", value) for value in (source, base)):
        raise ValueError("Full lowercase mod source/base commits are required")
    if not re.fullmatch(r"[1-9][0-9]*:[1-9][0-9]*", run):
        raise ValueError("An exact positive run:attempt is required")


def file_map(root):
    result = {}
    for path in Path(root).rglob("*"):
        if path.is_symlink():
            raise ValueError("Profiles package cannot contain symlinks")
        if path.is_file():
            result[path.relative_to(root).as_posix()] = {"bytes": path.stat().st_size, "sha256": digest(path)}
    return result


def validate_files(files):
    if set(files) != PAYLOAD_FILES:
        raise ValueError("Incomplete or unexpected Windows Profiles payload")
    for info in files.values():
        if (set(info) != {"bytes", "sha256"} or type(info["bytes"]) is not int or info["bytes"] <= 0
                or not isinstance(info["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", info["sha256"])):
            raise ValueError("Invalid Profiles file receipt")


def stage(prefix, output, source, base, run):
    identity(source, base, run)
    revision = pin()
    prefix, output = Path(prefix), Path(output)
    if output.exists():
        raise ValueError("Profiles output must be a new directory")
    receipt = prefix / "share/stfc-profiles/source.txt"
    if receipt.read_text(encoding="utf-8").splitlines() != ["Guffawaffle/stfc-profiles", revision]:
        raise ValueError("Installed Profiles source receipt does not match the build pin")
    sources = {"version.dll": prefix / "bin/version.dll", "stfc-profiles.exe": prefix / "bin/stfc-profiles.exe",
               "notices/mod-LICENSE": ROOT / "LICENSE", "notices/THIRD_PARTY.md": ROOT / "docs/THIRD_PARTY.md"}
    sources.update({"notices/profiles/" + name: prefix / "share/stfc-profiles" / name for name in PROFILE_FILES})
    for path in sources.values():
        if not path.is_file() or path.is_symlink() or not path.stat().st_size:
            raise ValueError(f"Missing or unsafe Profiles input: {path}")
    output.mkdir(parents=True)
    for name, path in sources.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    files = file_map(output)
    validate_files(files)
    manifest = {"schemaVersion": 1, "mod": {"source": source, "base": base, "buildChannel": "ci"},
                "profiles": {"repository": "Guffawaffle/stfc-profiles", "revision": revision},
                "build": {"run": run, "platform": "windows"}, "files": files}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def verify(payload, dll, source, run):
    identity(source, source, run)
    payload = Path(payload)
    manifest = json.loads((payload / "manifest.json").read_text(encoding="utf-8"))
    if (manifest.get("schemaVersion") != 1
            or manifest.get("mod") != {"source": source, "base": source, "buildChannel": "ci"}
            or manifest.get("build") != {"run": run, "platform": "windows"}
            or manifest.get("profiles") != {"repository": "Guffawaffle/stfc-profiles", "revision": pin()}):
        raise ValueError("Profiles package does not match the selected producing source/run and trusted pin")
    files = manifest.get("files", {})
    validate_files(files)
    actual = file_map(payload)
    actual.pop("manifest.json", None)
    if actual != files:
        raise ValueError("Profiles payload differs from its unsigned manifest")
    if digest(dll) != files["version.dll"]["sha256"]:
        raise ValueError("Profiles DLL differs from the independently selected canonical DLL")
    if (payload / "notices/profiles/source.txt").read_text(encoding="utf-8").splitlines() != [
            "Guffawaffle/stfc-profiles", pin()]:
        raise ValueError("Profiles installed source receipt differs from its trusted pin")
    return manifest


def package(payload, signed_dll, unsigned_receipt, output, signing_receipt):
    payload, signed_dll, output = Path(payload), Path(signed_dll), Path(output)
    manifest = json.loads(Path(unsigned_receipt).read_text(encoding="utf-8"))
    validate_files(manifest["files"])
    files = file_map(payload)
    if set(files) != ARCHIVE_FILES:
        raise ValueError("Signed Profiles package has unexpected or missing files")
    for name in PAYLOAD_FILES - {"version.dll", "stfc-profiles.exe"}:
        if files[name] != manifest["files"][name]:
            raise ValueError("Profiles notices changed after input verification")
    # The canonical DLL and helper have now been separately Authenticode-verified.
    files["version.dll"] = {"bytes": signed_dll.stat().st_size, "sha256": digest(signed_dll)}
    files.pop("manifest.json")
    signed = dict(manifest, files=files, unsignedFiles=manifest["files"],
                  signing=json.loads(Path(signing_receipt).read_text(encoding="utf-8")))
    if output.exists():
        raise ValueError("Signed Profiles archive already exists")
    with zipfile.ZipFile(output, "x", zipfile.ZIP_DEFLATED) as archive:
        archive.write(signed_dll, "version.dll")
        for name in sorted(PAYLOAD_FILES - {"version.dll"}):
            archive.write(payload / name, name)
        archive.writestr("manifest.json", json.dumps(signed, indent=2) + "\n")
    return signed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    build = commands.add_parser("stage")
    for name in ("prefix", "output", "source", "run"):
        build.add_argument("--" + name, required=True)
    build.add_argument("--base")
    check = commands.add_parser("verify")
    for name in ("payload", "dll", "source", "run", "receipt"):
        check.add_argument("--" + name, required=True)
    pack = commands.add_parser("package")
    for name in ("payload", "signed-dll", "unsigned-receipt", "output", "signing-receipt"):
        pack.add_argument("--" + name, required=True)
    args = parser.parse_args()
    if args.operation == "stage":
        stage(args.prefix, args.output, args.source, args.base or args.source, args.run)
    elif args.operation == "verify":
        receipt = verify(args.payload, args.dll, args.source, args.run)
        Path(args.receipt).write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    else:
        package(args.payload, args.signed_dll, args.unsigned_receipt, args.output, args.signing_receipt)


if __name__ == "__main__":
    main()
