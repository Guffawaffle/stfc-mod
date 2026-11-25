"""Canonical downstream package tags during the independent-version transition."""

import re
from typing import NamedTuple


UINT32_MAX = 0xffffffff
PE_COMPONENT_MAX = 65535
NUMBER = r"(?:0|[1-9][0-9]*)"
POSITIVE = r"[1-9][0-9]*"
LEGACY_BASE = re.compile(rf"{NUMBER}\.{NUMBER}\.{NUMBER}\.{NUMBER}")
MODERN_BASE = re.compile(rf"{NUMBER}\.{NUMBER}\.{NUMBER}")
LEGACY_TAG = re.compile(rf"v({LEGACY_BASE.pattern})-guffa\.({POSITIVE})(?:-dev\.({POSITIVE}))?")
MODERN_TAG = re.compile(rf"v({MODERN_BASE.pattern})(?:-(rc|dev)\.({POSITIVE}))?")


class ReleaseVersion(NamedTuple):
    base: str
    parts: tuple[int, ...]
    legacy: bool
    stage: str
    sequence: int
    fork_revision: int

    def order(self) -> tuple[int, ...]:
        # All independent versions follow the legacy line. Compare prerelease
        # sequences numerically and keep final above rc above dev for one core.
        if self.legacy:
            return (0, *self.parts, self.fork_revision, self.sequence)
        return (1, *self.parts, {"dev": 0, "rc": 1, "": 2}[self.stage], self.sequence)


def base_parts(base: str, *, packaged: bool = False) -> tuple[int, ...]:
    if not isinstance(base, str):
        raise ValueError("base version must be a canonical numeric version")
    legacy = LEGACY_BASE.fullmatch(base)
    if not legacy and not MODERN_BASE.fullmatch(base):
        raise ValueError("base version must contain three or four canonical numeric components")
    parts = tuple(int(part) for part in base.split("."))
    if not legacy and parts[0] < 2:
        raise ValueError("independent base versions must have major version 2 or greater")
    limit = PE_COMPONENT_MAX if packaged and not legacy else UINT32_MAX
    if any(part > limit for part in parts):
        raise ValueError(f"base version component exceeds {limit}")
    return parts


def canonical_base(base: str, *, packaged: bool = False) -> str:
    parts = base_parts(base, packaged=packaged)
    if len(parts) == 4 and parts[0] >= 2:
        if parts[3] != 0:
            raise ValueError("independent fixed versions must have a zero fourth component")
        base = ".".join(str(part) for part in parts[:3])
        base_parts(base, packaged=packaged)
    return base


def parse_tag(tag: str, channel: str | None = None) -> ReleaseVersion:
    if not isinstance(tag, str):
        raise ValueError("release tag must be a string")
    match = LEGACY_TAG.fullmatch(tag)
    if match:
        base, fork, sequence = match.groups()
        result = ReleaseVersion(base, base_parts(base), True, "dev" if sequence else "",
                                int(sequence or 0), int(fork))
        if result.fork_revision > UINT32_MAX or result.sequence > UINT32_MAX:
            raise ValueError("release tag contains an out-of-range version component")
        channels = ("dev",) if sequence else ("release", "pre")
    else:
        match = MODERN_TAG.fullmatch(tag)
        if not match:
            raise ValueError("release tag is not a canonical legacy or independent version")
        base, stage, sequence = match.groups()
        result = ReleaseVersion(base, base_parts(base), False, stage or "", int(sequence or 0), 0)
        if result.sequence > UINT32_MAX:
            raise ValueError("release tag contains an out-of-range version component")
        channels = ({"": "release", "rc": "pre", "dev": "dev"}[result.stage],)
    if channel is not None and channel not in channels:
        raise ValueError(f"{tag!r} is not a canonical {channel} tag")
    return result
