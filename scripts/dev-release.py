#!/usr/bin/env python3
"""Validate, package and publish the explicitly selected, pinned DEV product."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
REPOSITORY = "Guffawaffle/stfc-mod"
SPEC = importlib.util.spec_from_file_location("mac_identity", ROOT / "scripts/mac-build-identity.py")
IDENTITY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(IDENTITY)
SCHEMA = "stfc-dev-release/v1"


def command(*args, **kwargs):
    return subprocess.check_output(args, text=True, **kwargs).strip()


def sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def source_version():
    header = (ROOT / "src/mod/version.h").read_text(encoding="utf-8")
    values = [re.findall(rf"^#define\s+{name}\s+([0-9]+)\s*$", header, re.MULTILINE)
              for name in ("VERSION_MAJOR", "VERSION_MINOR", "VERSION_REVISION", "VERSION_PATCH",
                           "VERSION_FORK_REVISION")]
    if any(len(value) != 1 for value in values):
        raise ValueError("missing or ambiguous compiled version")
    return ".".join(value[0] for value in values[:4]), int(values[4][0])


def validate_tag(tag, base, revision):
    fields = IDENTITY.build_metadata(base, channel="dev", release_tag=tag or None, fork_revision=revision)
    if tag and int(tag.rsplit(".", 1)[1]) > 2**32 - 1:
        raise ValueError("DEV sequence exceeds the native build limit")
    return fields


def resolve(ref, source, output):
    if os.environ.get("GITHUB_REPOSITORY") != REPOSITORY:
        raise ValueError("private DEV workflow is restricted to its owner repository")
    if not re.fullmatch(r"[0-9a-f]{40}", source):
        raise ValueError("source must be an exact commit")
    tip = command("git", "rev-parse", "refs/remotes/origin/play-dev^{commit}", cwd=ROOT)
    if command("git", "rev-parse", "HEAD", cwd=ROOT) != source or tip != source:
        raise ValueError("DEV source must equal the current play-dev tip")
    base, revision = source_version()
    if ref == "refs/heads/play-dev":
        tag = ""
    elif ref.startswith("refs/tags/"):
        tag = ref.removeprefix("refs/tags/")
        validate_tag(tag, base, revision)
        if command("git", "rev-parse", f"{ref}^{{commit}}", cwd=ROOT) != source:
            raise ValueError("tag does not identify the selected source")
    else:
        raise ValueError("only play-dev and canonical DEV tags may select private source")
    pins = re.findall(r'^pub const pinned_revision = "([0-9a-f]{40})";',
                      (ROOT / "zig/aai.zig").read_text(encoding="utf-8"), re.MULTILINE)
    if len(pins) != 1:
        raise ValueError("missing or ambiguous immutable AAI pin")
    context = dict(schema=SCHEMA, repository=REPOSITORY, source=source, aai_pin=pins[0], tag=tag,
                   base=base, fork_revision=revision, channel="dev", mode="releasedbg",
                   aai_host=True, aai_ui_lab=True,
                   run_id=str(IDENTITY.numeric(os.environ["GITHUB_RUN_ID"], "run ID", 1)),
                   run_number=IDENTITY.numeric(os.environ["GITHUB_RUN_NUMBER"], "run number", 1),
                   run_attempt=IDENTITY.numeric(os.environ["GITHUB_RUN_ATTEMPT"], "run attempt", 1))
    output.write_text(json.dumps(context, indent=2) + "\n", encoding="utf-8")
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as stream:
        for key in ("source", "aai_pin", "tag"):
            stream.write(f"{key}={context[key]}\n")
    return context


def metadata(context):
    return IDENTITY.build_metadata(
        context["base"], channel="dev", release_tag=context["tag"] or None,
        fork_revision=context["fork_revision"], source=context["source"],
        run_id=context["run_id"], run_number=context["run_number"], run_attempt=context["run_attempt"])


def verify_mac(context, prefixes):
    expected = metadata(context)
    icon_hash = sha256(ROOT / "assets/launcher-icons/launcher-dev.icns")
    for prefix in prefixes:
        app = prefix / "macOSLauncher.app"
        _, fields = IDENTITY.read_plist(app / "Contents/Info.plist")
        if IDENTITY.validate_plist(fields) != expected:
            raise ValueError("Mac app identity differs from this DEV run")
        if sha256(app / "Contents/Resources/launcher.icns") != icon_hash:
            raise ValueError("Mac app is missing the DEV icon")


def verify_windows(context, dll):
    invocation = f"github:{context['run_id']}:{context['run_attempt']}:windows"
    marker = (f"stfc-identity-v1;distribution=netniv.stfc-community-mod;source=git:{context['source']};"
              f"base={context['source']};build={invocation};mode=releasedbg;channel=dev")
    subprocess.run([os.sys.executable, str(ROOT / "scripts/verify-windows-binary.py"),
                    str(dll), "--identity", marker], check=True)
    data = dll.read_bytes()
    product = context["tag"].removeprefix("v") if context["tag"] else (
        f"{context['base']}-guffa.{context['fork_revision']}")
    if (product + "\0").encode("utf-16le") not in data:
        raise ValueError("Windows ProductVersion does not match the DEV version")
    # The PE certificate table must be empty; DEV never passes through Authenticode signing.
    import struct
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    optional = pe + 24
    if struct.unpack_from("<H", data, optional)[0] != 0x20b:
        raise ValueError("expected PE32+")
    if struct.unpack_from("<II", data, optional + 112 + 4 * 8) != (0, 0):
        raise ValueError("DEV Windows DLL unexpectedly has a certificate table")


def payload_names(tag):
    names = {"stfc-community-mod.zip", "stfc-community-mod-windows-x64.tar.zst",
             "stfc-community-mod-windows-x64.tar.zst.sha256", "stfc-community-mod-macos-universal.tar.zst",
             "stfc-community-mod-macos-universal.tar.zst.sha256", "stfc-community-mod-installer.dmg"}
    if tag:
        names.add(f"stfc-community-mod-{tag}.zip")
    return names


def package(context, windows, app, output):
    verify_windows(context, windows)
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("package output must be empty")
    with zipfile.ZipFile(output / "stfc-community-mod.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(windows, "version.dll")
    if context["tag"]:
        (output / f"stfc-community-mod-{context['tag']}.zip").write_bytes(
            (output / "stfc-community-mod.zip").read_bytes())
    for name, file, member in (("windows-x64", windows, "version.dll"),
                               ("macos-universal", app / "Contents/libstfc-community-mod.dylib",
                                "libstfc-community-mod.dylib")):
        path = output / f"stfc-community-mod-{name}.tar.zst"
        with tempfile.TemporaryDirectory() as temporary:
            tar = Path(temporary) / "payload.tar"
            with tarfile.open(tar, "w") as archive:
                archive.add(file, arcname=member, recursive=False)
            subprocess.run(["zstd", "-15", "-T0", str(tar), "-o", str(path)], check=True)
        path.with_suffix(path.suffix + ".sha256").write_text(sha256(path) + "\n", encoding="utf-8")
    subprocess.run(["bash", str(ROOT / "scripts/create-mac-dmg.sh"), str(app),
                    str(output / "stfc-community-mod-installer.dmg")], check=True)
    manifest = dict(context, display_version=metadata(context)["STFCDisplayVersion"],
                    signing={"windows": "unsigned", "macos": "ad-hoc; not notarized"},
                    assets={path.name: {"sha256": sha256(path), "size": path.stat().st_size}
                            for path in sorted(output.iterdir())})
    (output / "dev-release.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (output / "SHA256SUMS").write_text("".join(f"{sha256(path)}  {path.name}\n"
                                              for path in sorted(output.iterdir())), encoding="utf-8")
    verify_assets(context, output)


def verify_assets(context, output):
    manifest = json.loads((output / "dev-release.json").read_text(encoding="utf-8"))
    if any(manifest.get(key) != value for key, value in context.items()):
        raise ValueError("release provenance differs from the selecting job")
    expected = payload_names(context["tag"])
    if set(manifest["assets"]) != expected or {p.name for p in output.iterdir()} != (
            expected | {"dev-release.json", "SHA256SUMS"}):
        raise ValueError("unexpected or missing release assets")
    for name, record in manifest["assets"].items():
        file = IDENTITY.regular_file(output / name)
        if record != dict(sha256=sha256(file), size=file.stat().st_size):
            raise ValueError(f"release payload changed: {name}")
    sums = "".join(f"{sha256(path)}  {path.name}\n" for path in sorted(output.iterdir())
                   if path.name != "SHA256SUMS")
    if (output / "SHA256SUMS").read_text(encoding="utf-8") != sums:
        raise ValueError("checksum manifest changed")
    return manifest


def api(path, method="GET", data=None):
    args = ["gh", "api", "--method", method, path]
    if data is not None:
        args += ["--input", "-"]
    return json.loads(command(*args, input=json.dumps(data) if data is not None else None) or "null")


def publish(context, output):
    verify_assets(context, output)
    tag = context["tag"]
    if not tag or os.environ.get("GITHUB_REPOSITORY") != REPOSITORY:
        raise ValueError("only the owner tag workflow may publish")
    # Resolve the live remote tag, including an annotated tag object.
    remote = api(f"repos/{REPOSITORY}/git/ref/tags/{tag}")["object"]
    if remote["type"] == "tag":
        remote = api(f"repos/{REPOSITORY}/git/tags/{remote['sha']}")["object"]
    if remote["type"] != "commit" or remote["sha"] != context["source"]:
        raise ValueError("remote tag no longer identifies the built source")
    # Look up drafts too; never replace an already published release.
    releases = json.loads(command("gh", "api", "--paginate", "--slurp",
                                 f"repos/{REPOSITORY}/releases?per_page=100"))
    matches = [release for page in releases for release in page if release["tag_name"] == tag]
    if matches:
        release = matches[0]
        if len(matches) != 1 or not release["draft"] or release["target_commitish"] != context["source"]:
            raise ValueError("refusing to overwrite an existing release")
    else:
        release = api(f"repos/{REPOSITORY}/releases", "POST", dict(
            tag_name=tag, target_commitish=context["source"], name=f"{tag} · DEV", draft=True,
            prerelease=True, make_latest="false", body=(
                "Explicit AAI development package. Windows is unsigned; macOS is ad hoc signed and not notarized.\n\n"
                f"Source: `{context['source']}`\nAAI pin: `{context['aai_pin']}`\n"
                f"[Build run](https://github.com/{REPOSITORY}/actions/runs/{context['run_id']})\n\n"
                "Use the full macOS installer for the DEV launcher and icon. See dev-release.json and SHA256SUMS for provenance.")))
    subprocess.run(["gh", "release", "upload", tag, "--repo", REPOSITORY, "--clobber",
                    *map(str, sorted(output.iterdir()))], check=True)
    assets = api(f"repos/{REPOSITORY}/releases/{release['id']}/assets?per_page=100")
    if {asset["name"] for asset in assets} != {path.name for path in output.iterdir()}:
        raise ValueError("remote release asset set differs from the package")
    for asset in assets:
        file = output / asset["name"]
        if asset["size"] != file.stat().st_size or asset.get("digest") != "sha256:" + sha256(file):
            raise ValueError(f"uploaded asset digest mismatch: {file.name}")
    api(f"repos/{REPOSITORY}/releases/{release['id']}", "PATCH",
        dict(draft=False, prerelease=True, make_latest="false"))
    print(f"Published https://github.com/{REPOSITORY}/releases/tag/{tag}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    select = commands.add_parser("resolve")
    select.add_argument("--ref", required=True)
    select.add_argument("--source", required=True)
    select.add_argument("--output", type=Path, required=True)
    for name in ("verify-mac", "package", "verify", "publish"):
        child = commands.add_parser(name)
        child.add_argument("--context", type=Path, required=True)
        if name == "verify-mac":
            child.add_argument("prefixes", type=Path, nargs="+")
        else:
            child.add_argument("--output", type=Path, required=True)
        if name == "package":
            child.add_argument("--windows", type=Path, required=True)
            child.add_argument("--app", type=Path, required=True)
    arguments = vars(parser.parse_args())
    name = arguments.pop("command")
    if "context" in arguments:
        arguments["context"] = json.loads(arguments["context"].read_text(encoding="utf-8"))
    try:
        {"resolve": resolve, "verify-mac": verify_mac, "package": package,
         "verify": verify_assets, "publish": publish}[name](**arguments)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
