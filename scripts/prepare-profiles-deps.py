#!/usr/bin/env python3
"""Build/verify the optional Profiles C dependency prefix. Never downloads or activates products.

This is a source-pinned build provisioner, not a runtime storage or package service.
Windows needs an x64 Visual Studio developer environment; macOS needs an explicit
SDK and compiler. All build/install paths must be supplied and remain disposable.
"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import time


PROFILE_REVISION = "c5c4ba911f16f3376e828761714d72a26ebe2a96"
PROFILE_SHA256 = "f120590499bb51844c1a2cd274f5bd58dbae0626b72c2d9f800041fabd08fd63"
PINS = {
    "libarchive": ("3.8.9", "libarchive-3.8.9.tar.gz", "f5a6539059cf5e597dbeda37bfa4874b1e8dea063c8d93bf85a2b44af90a5bd4"),
    "xz": ("5.8.3", "xz-5.8.3.tar.gz", "3d3a1b973af218114f4f889bbaa2f4c037deaae0c8e815eec381c3d546b974a0"),
    "zlib": ("1.3.2", "zlib-1.3.2.tar.gz", "b99a0b86c0ba9360ec7e78c4f1e43b1cbdf1e6936c8fa0f6835c0cd694a495a1"),
}
ARCHIVE_OPTIONS = {
    "BUILD_SHARED_LIBS": "OFF", "ENABLE_LZMA": "ON", "ENABLE_ZLIB": "ON",
    "ENABLE_OPENSSL": "OFF", "ENABLE_MBEDTLS": "OFF", "ENABLE_NETTLE": "OFF",
    "ENABLE_BZip2": "OFF", "ENABLE_LZ4": "OFF", "ENABLE_LZO": "OFF", "ENABLE_ZSTD": "OFF",
    "ENABLE_TEST": "OFF", "ENABLE_CAT": "OFF", "ENABLE_TAR": "OFF", "ENABLE_CPIO": "OFF", "ENABLE_UNZIP": "OFF",
}


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def absolute(value):
    path = Path(value)
    if not path.is_absolute():
        raise ValueError(f"An explicit absolute path is required: {value}")
    return path.resolve(strict=True)


def archive_files(archive, expected):
    if digest(archive) != expected:
        raise ValueError(f"Source archive SHA-256 mismatch: {archive}")
    entries = {}
    with tarfile.open(archive, "r:gz") as package:
        roots = set()
        for entry in package.getmembers():
            parts = PurePosixPath(entry.name).parts
            if not parts or parts[0] in {".", "..", "/"}:
                raise ValueError("Unsafe source package root")
            roots.add(parts[0])
            if entry.isdir():
                continue
            if not entry.isfile() or len(parts) < 2 or any(x in {".", ".."} or ":" in x or "\\" in x for x in parts):
                raise ValueError(f"Non-regular or unsafe package member: {entry.name}")
            relative = "/".join(parts[1:])
            if relative in entries:
                raise ValueError(f"Duplicate package member: {relative}")
            entries[relative] = hashlib.sha256(package.extractfile(entry).read()).hexdigest()
        if len(roots) != 1:
            raise ValueError("Source package must have exactly one root")
    return entries


def tree_files(root):
    entries = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"Symlinks are not accepted in qualified source/prefix trees: {path}")
        if path.is_file():
            entries[path.relative_to(root).as_posix()] = digest(path)
    return entries


def verify_source(root, archive, expected):
    entries = archive_files(archive, expected)
    actual = tree_files(root)
    if entries != actual:
        missing = sorted(set(entries) - set(actual))[:8]
        extra = sorted(set(actual) - set(entries))[:8]
        changed = sorted(x for x in set(entries) & set(actual) if entries[x] != actual[x])[:8]
        raise ValueError(f"Extracted source differs from its pinned archive: missing={missing}; extra={extra}; changed={changed}")
    return {"archive": str(archive), "archive_sha256": expected, "root": str(root), "files": entries}


def tool(value):
    found = shutil.which(value)
    if not found:
        raise ValueError(f"Required tool is unavailable: {value}")
    path = Path(found).resolve(strict=True)
    return {"path": str(path), "sha256": digest(path)}


def run(command, output, commands):
    log = output / f"{len(commands):02d}-{Path(command[0]).stem}.log"
    started = time.perf_counter()
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, text=True)
    commands.append({"argv": command, "exit_code": result.returncode, "wall_seconds": time.perf_counter() - started,
                     "log": str(log), "log_sha256": digest(log)})
    print(f"{log.name}: exit={result.returncode}", flush=True)
    if result.returncode:
        print(log.read_text(encoding="utf-8", errors="replace")[-5000:], file=sys.stderr)
        raise RuntimeError(f"Dependency command failed; see {log}")


def build(args):
    sources = absolute(args.sources)
    output = Path(args.output)
    if not output.is_absolute():
        raise ValueError("The dependency output must be an explicit absolute path")
    output = output.resolve()
    if output == sources or output in sources.parents or sources in output.parents:
        raise ValueError("The dependency output and source roots must be disjoint")
    output.mkdir(parents=True, exist_ok=True)
    prefix = output / "prefix"
    commands = []
    windows = args.target == "x86_64-windows-msvc"
    if windows and sys.platform != "win32":
        raise ValueError("The Windows C dependency producer requires native Windows/MSVC")
    if not windows and (sys.platform != "darwin" or args.target not in {"aarch64-macos.14.6", "x86_64-macos.14.6"}):
        raise ValueError("Supported producers are native Windows x64 and macOS arm64/x64")
    tools = {name: tool(getattr(args, name)) for name in ("cmake", "ninja", "cc")}
    verified = {}
    for name, (version, filename, sha) in PINS.items():
        verified[name] = verify_source(absolute(str(sources / f"{name}-{version}")), absolute(str(sources / filename)), sha)
    cc = tools["cc"]["path"]
    common = ["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']['path']).as_posix()}", f"-DCMAKE_C_COMPILER={Path(cc).as_posix()}",
              "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_TRY_COMPILE_CONFIGURATION=Release", "-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
              "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded", f"-DCMAKE_INSTALL_PREFIX={prefix.as_posix()}", "-DCMAKE_INSTALL_LIBDIR=lib",
              "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
    if windows:
        common += ["-DCMAKE_C_FLAGS_RELEASE=/O2 /DNDEBUG /MT"]
    else:
        sdk = absolute(args.sdk or "")
        common += [f"-DCMAKE_OSX_SYSROOT={sdk.as_posix()}", "-DCMAKE_OSX_DEPLOYMENT_TARGET=14.6",
                   f"-DCMAKE_OSX_ARCHITECTURES={'arm64' if args.target.startswith('aarch64') else 'x86_64'}"]
    library_names = ["archive.lib", "lzma.lib", "zs.lib"] if windows else ["libarchive.a", "liblzma.a", "libz.a"]
    recipe = [
        ("zlib", ["-DZLIB_BUILD_SHARED=OFF", "-DZLIB_BUILD_STATIC=ON", "-DZLIB_BUILD_TESTING=OFF", "-DZLIB_INSTALL=ON"]),
        ("xz", ["-DXZ_NLS=OFF", "-DXZ_TOOL_XZDEC=OFF", "-DXZ_TOOL_LZMADEC=OFF", "-DXZ_TOOL_LZMAINFO=OFF",
                "-DXZ_TOOL_XZ=OFF", "-DXZ_TOOL_SYMLINKS=OFF", "-DXZ_TOOL_SYMLINKS_LZMA=OFF", "-DXZ_TOOL_SCRIPTS=OFF",
                "-DXZ_DOXYGEN=OFF", "-DXZ_DOC=OFF", "-DBUILD_TESTING=OFF"]),
        ("libarchive", ["-DENABLE_TEST=OFF", "-DENABLE_CAT=OFF", "-DENABLE_TAR=OFF", "-DENABLE_CPIO=OFF", "-DENABLE_UNZIP=OFF",
                        "-DENABLE_PCREPOSIX=OFF", "-DENABLE_PCRE2POSIX=OFF", "-DPOSIX_REGEX_LIB=NONE", "-DENABLE_LIBGCC=OFF",
                        "-DENABLE_ICONV=OFF", "-DENABLE_ACL=OFF", "-DENABLE_EXPAT=OFF", "-DENABLE_LIBXML2=OFF", "-DENABLE_LIBB2=OFF",
                        "-DENABLE_OPENSSL=OFF", "-DENABLE_MBEDTLS=OFF", "-DENABLE_NETTLE=OFF", "-DENABLE_LZMA=ON", "-DENABLE_ZLIB=ON",
                        "-DENABLE_BZip2=OFF", "-DENABLE_LZ4=OFF", "-DENABLE_LZO=OFF", "-DENABLE_ZSTD=OFF",
                        f"-DENABLE_CNG={'ON' if windows else 'OFF'}", "-DMSVC_USE_STATIC_CRT=ON",
                        f"-DLIBLZMA_INCLUDE_DIR={(prefix / 'include').as_posix()}", f"-DLIBLZMA_LIBRARY={(prefix / 'lib' / library_names[1]).as_posix()}",
                        f"-DZLIB_INCLUDE_DIR={(prefix / 'include').as_posix()}", f"-DZLIB_LIBRARY={(prefix / 'lib' / library_names[2]).as_posix()}",
                        f"-DZLIB_LIBRARY_RELEASE={(prefix / 'lib' / library_names[2]).as_posix()}"]),
    ]
    for name, config in recipe:
        directory = output / "build" / name
        run([tools["cmake"]["path"], "-S", verified[name]["root"], "-B", str(directory), *common, *config], output, commands)
        run([tools["cmake"]["path"], "--build", str(directory), "--parallel", str(args.jobs)], output, commands)
        run([tools["cmake"]["path"], "--install", str(directory)], output, commands)
    for filename in library_names:
        if not (prefix / "lib" / filename).is_file():
            raise ValueError(f"Expected static library was not installed: {filename}")
    for name, entry in verified.items():
        verify_source(Path(entry["root"]), Path(entry["archive"]), entry["archive_sha256"])
        notices = prefix / "share" / "licenses" / name
        notices.mkdir(parents=True, exist_ok=True)
        for filename in ["COPYING", "COPYING.0BSD", "LICENSE", "README"]:
            source = Path(entry["root"]) / filename
            if source.is_file():
                shutil.copyfile(source, notices / filename)
    cache = output / "build/libarchive/CMakeCache.txt"
    cache_values = {line.split(":", 1)[0]: line.split("=", 1)[1] for line in cache.read_text(encoding="utf-8").splitlines()
                    if line and not line.startswith(("#", "//")) and ":" in line and "=" in line}
    expected_options = {**ARCHIVE_OPTIONS, "ENABLE_CNG": "ON" if windows else "OFF"}
    for key, value in expected_options.items():
        if cache_values.get(key) != value:
            raise ValueError(f"Unexpected libarchive feature configuration: {key}={cache_values.get(key)}")
    config_text = (output / "build/libarchive/config.h").read_text(encoding="utf-8")
    for feature in ["HAVE_LIBLZMA", "HAVE_LIBZ"]:
        if f"#define {feature} 1" not in config_text:
            raise ValueError(f"Required libarchive feature did not qualify: {feature}")
    receipt = {"schema": 1, "kind": "stfc-profiles-c-deps", "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
               "target": args.target, "configuration": "Release", "runtime": "MT" if windows else "native-libc", "prefix": str(prefix),
               "sources": verified, "tools": tools, "producer_script_sha256": digest(__file__), "commands": commands,
               "environment": {name: os.environ.get(name) for name in ["VCToolsVersion", "WindowsSDKVersion", "INCLUDE", "LIB"]},
               "static_libraries": ["lib/" + x for x in library_names], "defines": ["LIBARCHIVE_STATIC", "LZMA_API_STATIC"] if windows else [],
               "system_libraries": ["advapi32", "bcrypt", "crypt32", "shlwapi", "user32", "ws2_32"] + (["xmllite", "uuid"] if "#define HAVE_XMLLITE_H 1" in config_text else []) if windows else [],
               "feature_options": expected_options, "qualified_features": ["HAVE_LIBLZMA", "HAVE_LIBZ"],
               "cache_values": cache_values, "cache_sha256": digest(cache), "config_sha256": digest(output / "build/libarchive/config.h"), "files": tree_files(prefix)}
    path = output / "receipt.json"
    path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"Qualified prefix: {prefix}\nReceipt: {path}", flush=True)


def verify(args):
    if args.profiles_source or args.profiles_archive:
        if not args.profiles_source or not args.profiles_archive:
            raise ValueError("Profiles source and its pinned archive must be supplied together")
        verify_source(absolute(args.profiles_source), absolute(args.profiles_archive), PROFILE_SHA256)
    prefix = absolute(args.prefix)
    receipt = json.loads(absolute(args.receipt).read_text(encoding="utf-8"))
    if receipt.get("schema") != 1 or receipt.get("kind") != "stfc-profiles-c-deps" or receipt.get("target") != args.target:
        raise ValueError("Wrong dependency receipt schema/kind/target")
    if receipt.get("configuration") != "Release" or receipt.get("runtime") != ("MT" if "windows" in args.target else "native-libc"):
        raise ValueError("Dependency configuration/CRT mismatch")
    if set(receipt.get("sources", {})) != set(PINS):
        raise ValueError("Incomplete dependency source closure")
    for name, (_, _, sha) in PINS.items():
        if receipt["sources"][name].get("archive_sha256") != sha:
            raise ValueError(f"Dependency source pin mismatch: {name}")
    expected_options = {**ARCHIVE_OPTIONS, "ENABLE_CNG": "ON" if "windows" in args.target else "OFF"}
    if receipt.get("feature_options") != expected_options or receipt.get("qualified_features") != ["HAVE_LIBLZMA", "HAVE_LIBZ"]:
        raise ValueError("Dependency feature qualification mismatch")
    for key, value in expected_options.items():
        if receipt.get("cache_values", {}).get(key) != value:
            raise ValueError(f"Dependency cache does not confirm feature option: {key}")
    expected_libraries = ["lib/archive.lib", "lib/lzma.lib", "lib/zs.lib"] if "windows" in args.target else ["lib/libarchive.a", "lib/liblzma.a", "lib/libz.a"]
    if receipt.get("static_libraries") != expected_libraries or any(x not in receipt.get("files", {}) for x in expected_libraries):
        raise ValueError("Dependency static library closure mismatch")
    if tree_files(prefix) != receipt.get("files"):
        raise ValueError("Dependency prefix differs from its qualified receipt")
    source_note = f"Profiles {PROFILE_REVISION} source and " if args.profiles_source else ""
    print(f"Verified {source_note}{args.target} C dependency prefix", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    producer = commands.add_parser("build")
    producer.add_argument("--sources", required=True)
    producer.add_argument("--output", required=True)
    producer.add_argument("--target", required=True)
    producer.add_argument("--cmake", required=True)
    producer.add_argument("--ninja", required=True)
    producer.add_argument("--cc", required=True)
    producer.add_argument("--sdk")
    producer.add_argument("--jobs", type=int, default=4)
    consumer = commands.add_parser("verify")
    consumer.add_argument("--prefix", required=True)
    consumer.add_argument("--receipt", required=True)
    consumer.add_argument("--target", required=True)
    consumer.add_argument("--profiles-source")
    consumer.add_argument("--profiles-archive")
    args = parser.parse_args()
    try:
        (build if args.command == "build" else verify)(args)
    except (OSError, ValueError, RuntimeError, tarfile.TarError) as error:
        parser.exit(1, f"Profiles dependency qualification failed: {error}\n")


if __name__ == "__main__":
    main()
