#!/usr/bin/env python3
"""Advance one downstream update channel after its GitHub release is public."""

import argparse
import json
from pathlib import Path
import re
import urllib.request


REPOSITORY = "Guffawaffle/stfc-mod"
CHANNELS = ("release", "pre", "dev")
PLATFORM_ASSETS = ("stfc-community-mod.zip", "stfc-community-mod-installer.dmg")
TAG = re.compile(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\."
                 r"(0|[1-9][0-9]*)-guffa\.([1-9][0-9]*)(?:-dev\.([1-9][0-9]*))?")


def version(tag: str, channel: str) -> tuple[int, ...]:
    match = TAG.fullmatch(tag)
    if not match or (match[6] is not None) != (channel == "dev"):
        raise ValueError(f"{tag!r} is not a canonical {channel} tag")
    parts = tuple(int(part) for part in match.groups() if part is not None)
    if any(part > 0xffffffff for part in parts):
        raise ValueError(f"{tag!r} contains an out-of-range version component")
    return parts


def fetch_json(url: str) -> dict:
    request = urllib.request.Request(url, headers={"User-Agent": "stfc-mod-release-index"})
    with urllib.request.urlopen(request, timeout=15) as response:
        body = response.read(512 * 1024 + 1)
    if len(body) > 512 * 1024:
        raise ValueError("GitHub response exceeds 512 KiB")
    value = json.loads(body)
    if not isinstance(value, dict):
        raise ValueError("Expected a JSON object from GitHub")
    return value


def verify_published(tag: str, channel: str) -> None:
    release = fetch_json(f"https://api.github.com/repos/{REPOSITORY}/releases/tags/{tag}")
    prerelease = channel != "release"
    if (release.get("tag_name") != tag or release.get("draft") is not False
            or release.get("prerelease") is not prerelease or not release.get("published_at")):
        raise ValueError(f"{tag} is not published in the {channel} channel")

    manifest_name = "dev-release.json" if channel == "dev" else "release-manifest.json"
    assets = {asset["name"]: asset for asset in release["assets"] if asset.get("state") == "uploaded"}
    for name in (*PLATFORM_ASSETS, manifest_name):
        if not assets.get(name, {}).get("size"):
            raise ValueError(f"{tag} is missing the published asset {name}")

    manifest = fetch_json(f"https://github.com/{REPOSITORY}/releases/download/{tag}/{manifest_name}")
    if manifest.get("repository") != REPOSITORY or manifest.get("tag") != tag:
        raise ValueError("The published manifest repository or tag does not match")
    if channel == "dev":
        if manifest.get("schema") != "stfc-dev-release/v1" or manifest.get("channel") != "dev":
            raise ValueError("The published manifest does not identify a DEV package")
        hashes = {name: asset["sha256"] for name, asset in manifest["assets"].items()}
    else:
        if manifest.get("prerelease") is not prerelease:
            raise ValueError(f"The published manifest does not match the {channel} channel")
        hashes = manifest["payloadSha256"]
    for name in PLATFORM_ASSETS:
        digest = hashes.get(name, "")
        if not re.fullmatch(r"[0-9a-f]{64}", digest) or assets[name].get("digest") != f"sha256:{digest}":
            raise ValueError(f"The published digest for {name} does not match the release manifest")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("channel", choices=CHANNELS)
    parser.add_argument("tag")
    parser.add_argument("--index", type=Path,
                        default=Path(__file__).resolve().parents[1] / "releases" / "update-index.json")
    args = parser.parse_args()
    try:
        candidate = version(args.tag, args.channel)
        index = json.loads(args.index.read_text(encoding="utf-8"))
        if type(index["schema"]) is not int or index["schema"] != 1 or set(index["channels"]) != set(CHANNELS):
            raise ValueError("Expected a schema-1 index with Release, Pre, and DEV entries")
        channels = index["channels"]
        for channel in CHANNELS:
            previous = version(channels[channel], channel)
            if channel == args.channel and candidate < previous:
                raise ValueError(f"{args.tag} is older than {channels[channel]} in the {channel} channel")
        verify_published(args.tag, args.channel)
        channels[args.channel] = args.tag
        args.index.write_bytes((json.dumps(index, indent=2) + "\n").encode("utf-8"))
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Update index: {error}\n")


if __name__ == "__main__":
    main()
