#!/usr/bin/env python3
"""Audit unmanaged gameplay hook-like call sites.

This is intentionally a cheap static tripwire. It does not understand IL2CPP
semantics; it makes raw detour growth visible so review can enforce the
gameplay seam governance policy.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".mm"}
DEFAULT_SCAN_ROOTS = ("mods/src",)
DEFAULT_BASELINE = "manifests/gameplay_seam_unmanaged_baseline.json"


PATTERNS = (
    ("registry_spud_static_detour", re.compile(r"\bHOOK_REGISTRY_SPUD_STATIC_DETOUR\s*\("), "approved_registry"),
    ("raw_spud_static_detour", re.compile(r"\bSPUD_STATIC_DETOUR\s*\("), "unmanaged"),
    ("raw_minhook_create", re.compile(r"\bMH_CreateHook\s*\("), "unmanaged"),
    ("raw_minhook_enable", re.compile(r"\bMH_EnableHook\s*\("), "unmanaged"),
)


@dataclass(frozen=True)
class Finding:
    kind: str
    classification: str
    path: str
    line: int
    normalized_text: str
    id: str

    def to_json(self) -> dict[str, object]:
        return {
            "id": self.id,
            "kind": self.kind,
            "classification": self.classification,
            "path": self.path,
            "line": self.line,
            "normalized_text": self.normalized_text,
        }


def repo_relative(path: Path, repo_root: Path) -> str:
    return path.resolve().relative_to(repo_root.resolve()).as_posix()


def normalize_line(line: str) -> str:
    return re.sub(r"\s+", " ", line.strip())


def strip_comments_by_line(text: str) -> list[str]:
    """Remove C/C++ comments while preserving line count."""
    result: list[str] = []
    in_block = False

    for line in text.splitlines():
        out = []
        i = 0
        while i < len(line):
            if in_block:
                end = line.find("*/", i)
                if end == -1:
                    i = len(line)
                else:
                    in_block = False
                    i = end + 2
                continue

            block = line.find("/*", i)
            slash = line.find("//", i)
            candidates = [pos for pos in (block, slash) if pos != -1]
            if not candidates:
                out.append(line[i:])
                break

            first = min(candidates)
            out.append(line[i:first])
            if slash == first:
                break

            in_block = True
            i = first + 2

        result.append("".join(out))

    return result


def iter_source_files(repo_root: Path, scan_roots: Iterable[str]) -> Iterable[Path]:
    for root_text in scan_roots:
        root = (repo_root / root_text).resolve()
        if not root.exists():
            continue
        for path in sorted(root.rglob("*")):
            if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
                yield path


def classify_raw_spud(path: str, normalized: str) -> str:
    if path == "mods/src/patches/hook_registry.h" and normalized == "SPUD_STATIC_DETOUR(hook_registry_addr, fn); \\":
        return "approved_internal_wrapper"
    return "unmanaged"


def scan(repo_root: Path, scan_roots: Iterable[str]) -> list[Finding]:
    raw_findings: list[tuple[str, str, str, int, str]] = []

    for path in iter_source_files(repo_root, scan_roots):
        rel = repo_relative(path, repo_root)
        try:
            original_lines = path.read_text(encoding="utf-8").splitlines()
            stripped_lines = strip_comments_by_line(path.read_text(encoding="utf-8"))
        except UnicodeDecodeError:
            original_lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            stripped_lines = strip_comments_by_line(path.read_text(encoding="utf-8", errors="replace"))

        for line_number, stripped in enumerate(stripped_lines, start=1):
            normalized_stripped = normalize_line(stripped)
            if not normalized_stripped:
                continue
            if normalized_stripped.startswith("#define "):
                continue

            original = original_lines[line_number - 1] if line_number - 1 < len(original_lines) else stripped
            normalized_original = normalize_line(original)

            for kind, pattern, default_classification in PATTERNS:
                if not pattern.search(stripped):
                    continue
                classification = default_classification
                if kind == "raw_spud_static_detour":
                    classification = classify_raw_spud(rel, normalized_original)
                raw_findings.append((kind, classification, rel, line_number, normalized_original))

    occurrence: dict[tuple[str, str, str], int] = {}
    findings: list[Finding] = []
    for kind, classification, rel, line_number, normalized in raw_findings:
        key = (kind, rel, normalized)
        occurrence[key] = occurrence.get(key, 0) + 1
        digest = hashlib.sha1(f"{kind}\0{rel}\0{normalized}".encode("utf-8")).hexdigest()[:12]
        finding_id = f"{kind}:{rel}:{digest}:{occurrence[key]}"
        findings.append(Finding(kind, classification, rel, line_number, normalized, finding_id))

    return sorted(findings, key=lambda item: (item.path, item.line, item.kind, item.id))


def unmanaged(findings: Iterable[Finding]) -> list[Finding]:
    return [finding for finding in findings if finding.classification == "unmanaged"]


def baseline_payload(findings: list[Finding], scan_roots: Iterable[str]) -> dict[str, object]:
    accepted = []
    for finding in unmanaged(findings):
        accepted.append(
            {
                "id": finding.id,
                "kind": finding.kind,
                "path": finding.path,
                "normalized_text": finding.normalized_text,
                "category": "legacy_unmanaged",
                "reason": "Grandfathered unmanaged hook-like site from the manual 2026-06 gameplay seam baseline.",
            }
        )

    return {
        "schema_version": 1,
        "baseline_kind": "manual_current_state",
        "scan_roots": list(scan_roots),
        "accepted_unmanaged_findings": accepted,
    }


def load_baseline(path: Path) -> dict[str, object]:
    if not path.exists():
        raise FileNotFoundError(f"baseline not found: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1:
        raise ValueError(f"unsupported baseline schema_version in {path}")
    if not isinstance(data.get("accepted_unmanaged_findings"), list):
        raise ValueError(f"baseline missing accepted_unmanaged_findings list: {path}")
    return data


def compare(findings: list[Finding], baseline: dict[str, object]) -> tuple[list[Finding], list[dict[str, object]]]:
    current = {finding.id: finding for finding in unmanaged(findings)}
    accepted = baseline["accepted_unmanaged_findings"]
    accepted_ids = {str(entry["id"]) for entry in accepted if isinstance(entry, dict) and "id" in entry}

    new_findings = [finding for finding_id, finding in current.items() if finding_id not in accepted_ids]
    stale_entries = [entry for entry in accepted if isinstance(entry, dict) and str(entry.get("id")) not in current]
    return sorted(new_findings, key=lambda item: item.id), sorted(stale_entries, key=lambda item: str(item.get("id")))


def print_text_report(findings: list[Finding], new_findings: list[Finding], stale_entries: list[dict[str, object]]) -> None:
    unmanaged_count = len(unmanaged(findings))
    approved_count = len([finding for finding in findings if finding.classification != "unmanaged"])

    print("Gameplay seam scanner")
    print(f"  unmanaged findings: {unmanaged_count}")
    print(f"  approved/internal findings: {approved_count}")
    print(f"  new unmanaged findings: {len(new_findings)}")
    print(f"  stale baseline entries: {len(stale_entries)}")

    if new_findings:
        print("")
        print("New unmanaged hook-like findings:")
        for finding in new_findings:
            print(f"  {finding.path}:{finding.line} [{finding.kind}] {finding.normalized_text}")

    if stale_entries:
        print("")
        print("Stale baseline entries:")
        for entry in stale_entries:
            print(f"  {entry.get('path')} [{entry.get('kind')}] {entry.get('normalized_text')}")


def run_self_test(repo_root: Path) -> int:
    fixture = """\
#include <spud/detour.h>

void InstallGood(void* ptr)
{
  HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, descriptor, ptr, Good_Hook);
  // SPUD_STATIC_DETOUR(ptr, Commented_Hook);
}

void InstallBad(void* ptr)
{
  SPUD_STATIC_DETOUR(ptr, Bad_Hook);
}
"""

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_root = Path(temp_dir)
        source_dir = temp_root / "mods" / "src"
        source_dir.mkdir(parents=True)
        (source_dir / "fixture.cc").write_text(fixture, encoding="utf-8")

        findings = scan(temp_root, DEFAULT_SCAN_ROOTS)
        empty_baseline = {"schema_version": 1, "accepted_unmanaged_findings": []}
        new_findings, stale_entries = compare(findings, empty_baseline)

        if len(new_findings) != 1 or stale_entries:
            print_text_report(findings, new_findings, stale_entries)
            print("self-test failed: expected exactly one new unmanaged finding")
            return 1

        generated = baseline_payload(findings, DEFAULT_SCAN_ROOTS)
        accepted_new, accepted_stale = compare(findings, generated)
        if accepted_new or accepted_stale:
            print_text_report(findings, accepted_new, accepted_stale)
            print("self-test failed: generated baseline should accept fixture findings")
            return 1

    print("self-test passed: unmanaged raw hook fixture is detected and generated baseline accepts it")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="Repository root. Defaults to current directory.")
    parser.add_argument("--scan-root", action="append", dest="scan_roots", help="Relative source root to scan.")
    parser.add_argument("--baseline", default=DEFAULT_BASELINE, help="Baseline JSON path relative to root.")
    parser.add_argument("--write-baseline", action="store_true", help="Write the current unmanaged findings as baseline JSON.")
    parser.add_argument("--format", choices=("text", "json"), default="text", help="Report format.")
    parser.add_argument("--self-test", action="store_true", help="Run an internal fixture acceptance proof.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    repo_root = Path(args.root).resolve()
    scan_roots = tuple(args.scan_roots or DEFAULT_SCAN_ROOTS)

    if args.self_test:
        return run_self_test(repo_root)

    findings = scan(repo_root, scan_roots)
    baseline_path = (repo_root / args.baseline).resolve()

    if args.write_baseline:
        payload = baseline_payload(findings, scan_roots)
        baseline_path.parent.mkdir(parents=True, exist_ok=True)
        baseline_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"wrote baseline: {baseline_path}")
        return 0

    baseline = load_baseline(baseline_path)
    new_findings, stale_entries = compare(findings, baseline)

    if args.format == "json":
        print(
            json.dumps(
                {
                    "ok": not new_findings and not stale_entries,
                    "summary": {
                        "total_findings": len(findings),
                        "unmanaged_findings": len(unmanaged(findings)),
                        "approved_or_internal_findings": len(
                            [finding for finding in findings if finding.classification != "unmanaged"]
                        ),
                        "new_unmanaged_findings": len(new_findings),
                        "stale_baseline_entries": len(stale_entries),
                    },
                    "new_unmanaged_findings": [finding.to_json() for finding in new_findings],
                    "stale_baseline_entries": stale_entries,
                },
                indent=2,
            )
        )
    else:
        print_text_report(findings, new_findings, stale_entries)

    return 1 if new_findings or stale_entries else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
