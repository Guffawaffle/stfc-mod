#!/usr/bin/env python3
"""Build the selected DEV product on a native GitHub runner."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("dev_release", ROOT / "scripts/dev-release.py")
DEV = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DEV)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--context", type=Path, required=True)
    parser.add_argument("--aai-source", type=Path, required=True)
    parser.add_argument("--target", required=True)
    args = parser.parse_args()
    context = json.loads(args.context.read_text(encoding="utf-8"))
    source = args.aai_source.resolve(strict=True)
    if DEV.command("git", "rev-parse", "HEAD", cwd=source) != context["aai_pin"]:
        raise ValueError("private checkout differs from the selected pin")
    if DEV.command("git", "status", "--porcelain", "--untracked-files=all", cwd=source):
        raise ValueError("private checkout is dirty")
    subprocess.run([sys.executable, "scripts/stamp-version-hash.py", context["source"]], check=True)
    windows = args.target == "x86_64-windows-msvc"
    tests = ["test-toolchains", "test-codegen", "test-deps", "test-il2cpp", "test-mod", "test-sync",
             "test-aai-host", "test-aai"]
    if not windows:
        tests += ["test-launcher"]
    if args.target.startswith("x86_64"):
        tests += ["test-spud"]
    build = ["zig", "build", "install", *([] if windows else ["launcher"]), *tests,
             f"-Dtarget={args.target}", "-Doptimize=ReleaseFast", "-Dtest-optimize=Debug", "-Dmoddbg=true",
             "-Daai-host=true", "-Daai-ui-lab=true", f"-Daai-source={source}", "--summary", "all"]
    if windows:
        build += [f"-Dstfc_source_state_id=git:{context['source']}",
                  f"-Dstfc_base_commit={context['source']}",
                  f"-Dstfc_build_invocation_id=github:{context['run_id']}:{context['run_attempt']}:windows",
                  "-Dstfc_build_channel=dev", f"-Dstfc_development_tag={context['tag']}"]
    else:
        build += ["-Dlauncher_channel=dev", f"-Dlauncher_source={context['source']}",
                  f"-Dlauncher_run_id={context['run_id']}", f"-Dlauncher_run_number={context['run_number']}",
                  f"-Dlauncher_run_attempt={context['run_attempt']}",
                  f"-Dlauncher_release_tag={context['tag']}"]
    subprocess.run([sys.executable, "scripts/ci-build-retry.py", "--", *build], check=True)
    if windows:
        DEV.verify_windows(context, ROOT / "zig-out/bin/version.dll")
    else:
        DEV.verify_mac(context, [ROOT / "zig-out"])


if __name__ == "__main__":
    main()
