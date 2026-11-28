#!/usr/bin/env python3
"""Validate conservative docs/feed-only PRs; all other events retain native CI."""

import argparse
import importlib.util
import json
import math
from pathlib import Path, PurePosixPath
import re
import subprocess
import tomllib


ROOT = Path(__file__).resolve().parents[1]
INDEXES = {"releases/update-index.json", "releases/update-index-v2.json"}
ROOT_DOCS = {"README.md", "CHANGELOG.md", "CONTRIBUTING.md"}
SHA = re.compile(r"[0-9a-f]{40}")
FEED_LIMIT = 4096  # Keep aligned with mod_updates::max_index_bytes in release_index.h.
SPEC = importlib.util.spec_from_file_location("update_release_index", ROOT / "scripts/update-release-index.py")
INDEX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INDEX)


def git(*arguments: str, root: Path = ROOT) -> bytes:
    return subprocess.check_output(["git", "-C", str(root), *arguments])


def blob(revision: str, path: str, root: Path = ROOT) -> bytes:
    identity = f"{revision}:{path}"
    if int(git("cat-file", "-s", identity, root=root)) > 2 * 1024 * 1024:
        raise ValueError(f"Fast-path input exceeds 2 MiB: {path}")
    return git("show", identity, root=root)


def changes(base: str, head: str, root: Path = ROOT) -> list[tuple[str, str, str, str]]:
    if not SHA.fullmatch(base) or not SHA.fullmatch(head):
        raise ValueError("Expected exact base and head commit SHAs")
    records = git("diff", "--no-ext-diff", "--no-textconv", "--raw", "--no-abbrev", "--no-renames",
                  "-z", base, head, root=root).split(b"\0")
    result = []
    for index in range(0, len(records) - 1, 2):
        old_mode, new_mode, _, _, status = records[index].decode("ascii").removeprefix(":").split()
        result.append((status, records[index + 1].decode("utf-8"), old_mode, new_mode))
    return result


def same_toml(left, right) -> bool:
    # Python considers True == 1; the settings reader does not.
    if type(left) is not type(right):
        return False
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(same_toml(value, right[key]) for key, value in left.items())
    if isinstance(left, list):
        return len(left) == len(right) and all(same_toml(a, b) for a, b in zip(left, right))
    return left == right


def doc(path: str) -> bool:
    pure = PurePosixPath(path)
    return path in ROOT_DOCS or (pure.parts[0] == "docs" and pure.suffix == ".md"
                                 and path.casefold() != "docs/third_party.md")


def example(path: str) -> bool:
    pure = PurePosixPath(path)
    return (pure.parent == PurePosixPath("docs/configuration")
            and pure.name.startswith("example_community_patch_settings") and pure.suffix == ".toml")


def portable(path: str) -> bool:
    for part in path.split("/"):
        if not part or part.endswith((".", " ")) or any(char in '<>:"|?*' for char in part):
            return False
        stem = part.split(".", 1)[0].rstrip(" ").upper()
        if stem in {"CON", "PRN", "AUX", "NUL", "CLOCK$"} or re.fullmatch(r"(?:COM|LPT)[1-9¹²³]", stem):
            return False
    return True


def classify(event: str, changed: list[tuple[str, str, str, str]], read_blob) -> str:
    if event != "pull_request" or not changed:
        return "native"
    kinds = set()
    for status, path, old_mode, new_mode in changed:
        if (status not in ("A", "M") or new_mode != "100644"
                or (status == "M" and old_mode != "100644")
                or not path or any(ord(char) < 32 or ord(char) == 127 for char in path)
                or "\\" in path or ".." in PurePosixPath(path).parts or not portable(path)):
            return "native"
        if path in INDEXES:
            if status != "M":
                return "native"
            kinds.add("index")
        elif doc(path):
            kinds.add("docs")
        elif example(path) and status == "M":
            try:
                before = tomllib.loads(read_blob("base", path).decode("utf-8"))
                after = tomllib.loads(read_blob("head", path).decode("utf-8"))
            except (ValueError, UnicodeError):
                return "native"
            if not same_toml(before, after):
                return "native"
            kinds.add("docs")
        else:
            return "native"
    return next(iter(kinds)) if len(kinds) == 1 else "native"


def feed_json(data: bytes) -> dict:
    if not data or len(data) > FEED_LIMIT:
        raise ValueError("Update feed must contain 1–4096 bytes")
    def finite(value):
        try:
            supported = math.isfinite(float(value))
        except OverflowError:
            supported = False
        if not supported:
            raise ValueError("Update feed number exceeds the JSON consumer range")
        return value
    def constant(_):
        raise ValueError("Update feed contains a non-JSON numeric constant")
    def unique(pairs):
        value = {}
        for key, item in pairs:
            if key in value:
                raise ValueError(f"Duplicate feed key: {key}")
            value[key] = item
        return value
    value = json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique, parse_constant=constant,
                       parse_int=lambda text: finite(int(text)), parse_float=lambda text: finite(float(text)))
    # JSON escapes must also describe valid Unicode; Python otherwise retains lone surrogates.
    json.dumps(value, ensure_ascii=False, allow_nan=False).encode("utf-8")
    return value


def validate_feeds(changed, read_blob) -> None:
    for _, path, _, _ in changed:
        legacy = path.endswith("/update-index.json")
        previous = INDEX.validate_index(feed_json(read_blob("base", path)), legacy)
        candidate = INDEX.validate_index(feed_json(read_blob("head", path)), legacy)
        for channel in INDEX.CHANNELS:
            tag = candidate[channel]
            if tag == previous[channel]:
                continue
            if INDEX.version(tag, channel) < INDEX.version(previous[channel], channel):
                raise ValueError(f"{path}: {channel} moves backward")
            INDEX.verify_published(tag, channel)
            print(f"Verified {path}: {channel} -> {tag}")


def validate_docs(changed, read_blob, base: str, head: str, root: Path = ROOT) -> None:
    subprocess.run(["git", "-C", str(root), "diff", "--no-ext-diff", "--no-textconv", "--check", base, head],
                   check=True)
    for _, path, _, _ in changed:
        text = read_blob("head", path).decode("utf-8")
        if "\0" in text:
            raise ValueError(f"Documentation contains NUL bytes: {path}")
        if example(path):
            tomllib.loads(text)
        print(f"Validated documentation: {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event", required=True)
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", required=True)
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()
    try:
        changed = changes(args.base, args.head) if args.event == "pull_request" else []
        read_blob = lambda side, path: blob(args.base if side == "base" else args.head, path)
        mode = classify(args.event, changed, read_blob)
        if mode == "index":
            validate_feeds(changed, read_blob)
        elif mode == "docs":
            validate_docs(changed, read_blob, args.base, args.head)
        print(f"CI scope: {mode}")
        if args.github_output:
            with args.github_output.open("a", encoding="utf-8") as output:
                output.write(f"mode={mode}\n")
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"CI scope: {error}\n")


if __name__ == "__main__":
    main()
