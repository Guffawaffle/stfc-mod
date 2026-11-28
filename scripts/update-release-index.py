#!/usr/bin/env python3
"""Advance one downstream update channel after its GitHub release is public."""

import argparse
import json
from pathlib import Path
import re
import urllib.request

from release_version import parse_tag

REPOSITORY = "Guffawaffle/stfc-mod"
CHANNELS = ("release", "pre", "dev")
PLATFORM_ASSETS = ("stfc-community-mod.zip", "stfc-community-mod-installer.dmg")


def version(tag: str, channel: str) -> tuple[int, ...]:
    return parse_tag(tag, channel).order()


def default_index(tag: str, channel: str) -> Path:
    name = "update-index.json" if parse_tag(tag, channel).legacy else "update-index-v2.json"
    return Path(__file__).resolve().parents[1] / "releases" / name


def validate_index(index: dict, legacy_feed: bool) -> dict:
    """Validate the shared feed schema and every channel's tag grammar."""
    if (not isinstance(index, dict) or type(index.get("schema")) is not int or index["schema"] != 1
            or not isinstance(index.get("channels"), dict) or set(index["channels"]) != set(CHANNELS)):
        raise ValueError("Expected a schema-1 index with Release, Pre, and DEV entries")
    channels = index["channels"]
    for entry_channel in CHANNELS:
        version(channels[entry_channel], entry_channel)
        if legacy_feed and not parse_tag(channels[entry_channel], entry_channel).legacy:
            raise ValueError("the legacy feed must contain only legacy tags")
    return channels


def advance_index(path: Path, channel: str, tag: str) -> None:
    """Validate all entries and publication before writing one selected feed."""
    candidate = version(tag, channel)
    legacy_feed = path.resolve().name.casefold() == "update-index.json"
    if legacy_feed and not parse_tag(tag, channel).legacy:
        raise ValueError("independent tags must use update-index-v2.json; the legacy feed stays legacy")
    index = json.loads(path.read_text(encoding="utf-8"))
    channels = validate_index(index, legacy_feed)
    if candidate < version(channels[channel], channel):
        raise ValueError(f"{tag} is older than {channels[channel]} in the {channel} channel")
    verify_published(tag, channel)
    channels[channel] = tag
    path.write_bytes((json.dumps(index, indent=2) + "\n").encode("utf-8"))


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
    parser.add_argument("--index", type=Path, help="explicit feed path; otherwise route by tag version")
    args = parser.parse_args()
    try:
        advance_index(args.index or default_index(args.tag, args.channel), args.channel, args.tag)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Update index: {error}\n")


if __name__ == "__main__":
    main()
