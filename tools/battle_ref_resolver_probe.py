#!/usr/bin/env python3
"""Read-only scout for resolving battle runtime refs against local catalog/dump files.

The probe is intentionally evidence-first. It reports exact local matches and likely
domains, but it does not promote runtime ability candidates into stable analytics.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import sqlite3
import sys
import tempfile
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


DEFAULT_REFS = [
    "4290764940",
    "1120204726",
    "3426564736",
    "182221633",
    "2974230331",
    "1426126747",
    "2813724537",
    "2241990218",
    "3308805436",
    "1761806598",
    "473132032",
    "405335503",
    "2573953069",
    "1280858269",
    "1720277001",
    "298753785",
    "3280907524",
    "4273474013",
    "2490276629",
]

SUPPORTED_TEXT_SUFFIXES = {
    ".csv",
    ".cs",
    ".h",
    ".hpp",
    ".json",
    ".jsonl",
    ".md",
    ".proto",
    ".txt",
}

ID_KEY_FRAGMENTS = (
    "id",
    "ref",
    "key",
    "hash",
    "loca",
    "component",
    "hull",
    "officer",
    "ability",
    "buff",
    "effect",
    "modifier",
)

DISPLAY_KEYS = (
    "name",
    "displayName",
    "display_name",
    "title",
    "locaKey",
    "loca_key",
    "LocaStringId",
    "description",
    "display",
)

REF_FIELD_NAMES = {
    "sourceRef",
    "effectRef",
    "refAExact",
    "refBExact",
    "shipIdExact",
    "fleetIdExact",
    "componentIdExact",
    "ownerShipId",
    "targetShipId",
}

REF_ARRAY_FIELD_NAMES = {
    "shipIdsExact",
    "hullIdsExact",
    "componentIdsExact",
    "ship_ids_exact",
    "hull_ids_exact",
    "component_ids_exact",
}

DOMAIN_HINTS = (
    ("officer_ability", ("officerability", "officer_ability", "captainmaneuver", "belowdecks")),
    ("officer", ("officer", "crew")),
    ("forbidden_tech", ("forbiddentech", "forbidden_tech", "chaostech", "chaos_tech")),
    ("buff", ("buff", "debuff", "effect", "modifier")),
    ("ship_ability", ("shipbonus", "ship_bonus", "shipability", "ship_ability")),
    ("hull", ("hull", "shiptype")),
    ("component", ("component", "weapon", "torpedo", "phaser", "disruptor")),
    ("resource", ("resource",)),
    ("localization", ("localization", "loca", "stringliteral", "string_literal")),
    ("il2cpp_metadata", ("il2cpp", "dump.cs", "script.json", "dump-corpus", "symbol")),
)


@dataclass(frozen=True)
class Match:
    ref: str
    candidate_domain: str
    matched_record_key_path: str
    matched_display_name: str | None
    confidence: str
    source_path: str
    provenance: str
    evidence: str

    def as_json(self) -> dict[str, Any]:
        return {
            "ref": self.ref,
            "candidateDomain": self.candidate_domain,
            "matchedRecordKeyPath": self.matched_record_key_path,
            "matchedDisplayName": self.matched_display_name,
            "confidence": self.confidence,
            "sourcePath": self.source_path,
            "provenance": self.provenance,
            "evidence": self.evidence,
        }


def normalize_ref(value: str) -> str:
    return value.strip()


def is_id_like_key(key: str) -> bool:
    lowered = key.lower()
    return any(fragment in lowered for fragment in ID_KEY_FRAGMENTS)


def infer_domain(*parts: str) -> str:
    text = " ".join(part.lower().replace("\\", "/") for part in parts if part)
    for domain, hints in DOMAIN_HINTS:
        if any(hint in text for hint in hints):
            return domain
    return "unknown"


def confidence_for(path: str, key: str, value: Any, object_key_match: bool = False) -> str:
    if object_key_match:
        return "hash_key_match" if not is_id_like_key(key) else "exact_id_match"
    if isinstance(value, str) and any(name in key.lower() for name in ("name", "loca", "display", "title")):
        return "localized_name_match"
    if is_id_like_key(key):
        return "exact_id_match"
    if path.endswith(".db"):
        return "hash_key_match"
    return "domain_ambiguous"


def json_path(parent: str, key: str | int) -> str:
    if isinstance(key, int):
        return f"{parent}[{key}]"
    safe = key if key.isidentifier() else json.dumps(key)
    return f"{parent}.{safe}" if parent else f"$.{safe}"


def find_display_name(obj: Any) -> str | None:
    if isinstance(obj, dict):
        for key in DISPLAY_KEYS:
            value = obj.get(key)
            if isinstance(value, (str, int, float)) and str(value).strip():
                return str(value)
        for value in obj.values():
            found = find_display_name(value)
            if found:
                return found
    return None


def scalar_matches(value: Any, refs: set[str]) -> str | None:
    if isinstance(value, bool) or value is None:
        return None
    if isinstance(value, int):
        text = str(value)
        return text if text in refs else None
    if isinstance(value, str):
        text = value.strip()
        return text if text in refs else None
    return None


def walk_json(value: Any, refs: set[str], source_path: Path, provenance: str, path: str = "$", parent: Any = None) -> list[Match]:
    matches: list[Match] = []
    if isinstance(value, dict):
        for key, entry in value.items():
            key_text = str(key)
            if key_text in refs:
                matches.append(
                    Match(
                        ref=key_text,
                        candidate_domain=infer_domain(str(source_path), path, key_text),
                        matched_record_key_path=json_path(path, key_text),
                        matched_display_name=find_display_name(entry) or find_display_name(value),
                        confidence=confidence_for(str(source_path), key_text, entry, object_key_match=True),
                        source_path=str(source_path),
                        provenance=provenance,
                        evidence="json_object_key",
                    )
                )
            if ref := scalar_matches(entry, refs):
                matches.append(
                    Match(
                        ref=ref,
                        candidate_domain=infer_domain(str(source_path), path, key_text),
                        matched_record_key_path=json_path(path, key_text),
                        matched_display_name=find_display_name(value),
                        confidence=confidence_for(str(source_path), key_text, entry),
                        source_path=str(source_path),
                        provenance=provenance,
                        evidence=f"json_scalar:{key_text}",
                    )
                )
            matches.extend(walk_json(entry, refs, source_path, provenance, json_path(path, key_text), value))
    elif isinstance(value, list):
        for index, entry in enumerate(value):
            if ref := scalar_matches(entry, refs):
                matches.append(
                    Match(
                        ref=ref,
                        candidate_domain=infer_domain(str(source_path), path),
                        matched_record_key_path=json_path(path, index),
                        matched_display_name=find_display_name(parent),
                        confidence="domain_ambiguous",
                        source_path=str(source_path),
                        provenance=provenance,
                        evidence="json_array_scalar",
                    )
                )
            matches.extend(walk_json(entry, refs, source_path, provenance, json_path(path, index), parent))
    return matches


def scan_json_file(path: Path, refs: set[str], max_file_bytes: int) -> list[Match]:
    if path.stat().st_size > max_file_bytes:
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return scan_text_file(path, refs, max_file_bytes)
    return walk_json(data, refs, path, "json")


def scan_jsonl_file(path: Path, refs: set[str], max_file_bytes: int) -> list[Match]:
    if path.stat().st_size > max_file_bytes:
        return []
    matches: list[Match] = []
    try:
        with path.open("r", encoding="utf-8") as handle:
            for line_index, line in enumerate(handle, 1):
                if not any(ref in line for ref in refs):
                    continue
                try:
                    data = json.loads(line)
                except Exception:
                    matches.extend(scan_text_line(path, refs, line, line_index))
                    continue
                matches.extend(walk_json(data, refs, path, "jsonl", f"$line[{line_index}]"))
    except Exception:
        return []
    return matches


def scan_csv_file(path: Path, refs: set[str], max_file_bytes: int) -> list[Match]:
    if path.stat().st_size > max_file_bytes:
        return []
    matches: list[Match] = []
    try:
        with path.open("r", encoding="utf-8", newline="") as handle:
            for row_index, row in enumerate(csv.DictReader(handle), 2):
                for key, value in row.items():
                    if value and value.strip() in refs:
                        ref = value.strip()
                        matches.append(
                            Match(
                                ref=ref,
                                candidate_domain=infer_domain(str(path), key),
                                matched_record_key_path=f"$row[{row_index}].{key}",
                                matched_display_name=find_display_name(row),
                                confidence=confidence_for(str(path), key, value),
                                source_path=str(path),
                                provenance="csv",
                                evidence=f"csv_cell:{key}",
                            )
                        )
    except Exception:
        return scan_text_file(path, refs, max_file_bytes)
    return matches


def scan_text_line(path: Path, refs: set[str], line: str, line_index: int) -> list[Match]:
    matches: list[Match] = []
    for ref in refs:
        if ref in line:
            matches.append(
                Match(
                    ref=ref,
                    candidate_domain=infer_domain(str(path), line[:160]),
                    matched_record_key_path=f"$line[{line_index}]",
                    matched_display_name=None,
                    confidence="domain_ambiguous",
                    source_path=str(path),
                    provenance="text",
                    evidence=line.strip()[:240],
                )
            )
    return matches


def scan_text_file(path: Path, refs: set[str], max_file_bytes: int) -> list[Match]:
    if path.stat().st_size > max_file_bytes:
        return []
    matches: list[Match] = []
    try:
        with path.open("r", encoding="utf-8", errors="ignore") as handle:
            for line_index, line in enumerate(handle, 1):
                if any(ref in line for ref in refs):
                    matches.extend(scan_text_line(path, refs, line, line_index))
    except Exception:
        return []
    return matches


def scan_sqlite_db(path: Path, refs: set[str]) -> list[Match]:
    matches: list[Match] = []
    try:
        con = sqlite3.connect(path)
    except Exception:
        return matches
    try:
        for ref in refs:
            rows = con.execute(
                "select id, address, value from string_literals where value = ? limit 20",
                (ref,),
            ).fetchall()
            for row_id, address, value in rows:
                matches.append(
                    Match(
                        ref=ref,
                        candidate_domain="localization",
                        matched_record_key_path=f"string_literals[{row_id}]",
                        matched_display_name=str(value),
                        confidence="hash_key_match",
                        source_path=str(path),
                        provenance="ax_sqlite:string_literals",
                        evidence=f"address={address}",
                    )
                )
            try:
                numeric_ref = int(ref)
            except ValueError:
                continue
            enum_rows = con.execute(
                """
                select enum_values.id, classes.name, classes.namespace, enum_values.name, enum_values.value
                from enum_values
                join classes on classes.id = enum_values.class_id
                where enum_values.value = ?
                limit 20
                """,
                (numeric_ref,),
            ).fetchall()
            for row_id, class_name, namespace, enum_name, value in enum_rows:
                matches.append(
                    Match(
                        ref=ref,
                        candidate_domain=infer_domain(namespace or "", class_name or "", enum_name or ""),
                        matched_record_key_path=f"enum_values[{row_id}]",
                        matched_display_name=f"{class_name}.{enum_name}",
                        confidence="exact_id_match",
                        source_path=str(path),
                        provenance="ax_sqlite:enum_values",
                        evidence=f"{namespace}.{class_name}.{enum_name}={value}",
                    )
                )
    except Exception:
        return matches
    finally:
        con.close()
    return matches


def discover_files(roots: Iterable[Path], max_file_bytes: int) -> tuple[list[Path], list[str]]:
    files: list[Path] = []
    searched: list[str] = []
    for root in roots:
        if not root.exists():
            searched.append(f"{root} (missing)")
            continue
        searched.append(str(root))
        if root.is_file():
            files.append(root)
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [
                name
                for name in dirnames
                if name not in {"node_modules", "build", ".git", ".xmake", ".vs", "__pycache__"}
            ]
            for filename in filenames:
                path = Path(dirpath) / filename
                if path.suffix.lower() in SUPPORTED_TEXT_SUFFIXES or path.suffix.lower() in {".db", ".sqlite"}:
                    try:
                        if path.stat().st_size <= max_file_bytes or path.suffix.lower() in {".db", ".sqlite"}:
                            files.append(path)
                    except OSError:
                        continue
    return files, searched


def scan_files(files: Iterable[Path], refs: set[str], max_file_bytes: int) -> list[Match]:
    matches: list[Match] = []
    for path in files:
        suffix = path.suffix.lower()
        if suffix == ".json":
            matches.extend(scan_json_file(path, refs, max_file_bytes))
        elif suffix == ".jsonl":
            matches.extend(scan_jsonl_file(path, refs, max_file_bytes))
        elif suffix == ".csv":
            matches.extend(scan_csv_file(path, refs, max_file_bytes))
        elif suffix in {".db", ".sqlite"}:
            matches.extend(scan_sqlite_db(path, refs))
        else:
            matches.extend(scan_text_file(path, refs, max_file_bytes))
    return matches


def build_summary(refs: list[str], matches: list[Match], searched_roots: list[str], file_count: int) -> dict[str, Any]:
    by_ref: dict[str, list[Match]] = defaultdict(list)
    for match in matches:
        by_ref[match.ref].append(match)

    domain_counts = Counter(match.candidate_domain for match in matches)
    confidence_counts = Counter(match.confidence for match in matches)
    resolvable_domains = sorted(domain for domain, count in domain_counts.items() if count > 0 and domain != "unknown")
    unresolved = [ref for ref in refs if not by_ref.get(ref)]
    ambiguous = sorted({match.ref for match in matches if match.confidence == "domain_ambiguous"})

    return {
        "schema": "stfc.battle.ref_resolver_probe.v0",
        "status": "scout",
        "searchedRoots": searched_roots,
        "scannedFileCount": file_count,
        "refCount": len(refs),
        "matchedRefCount": len(refs) - len(unresolved),
        "unresolvedRefCount": len(unresolved),
        "resolvableDomainsNow": resolvable_domains,
        "missingOrUnprovenDomains": [
            "officer ability source/effect refs",
            "buff/effect refs",
            "ship ability refs",
            "forbidden tech refs",
        ],
        "sourceEffectPairsJoinMeaningfully": False,
        "sourceEffectPairJoinNotes": (
            "No sourceRef/effectRef pair should be treated as resolved unless both refs have explicit "
            "catalog/domain matches from the same compatible catalog family."
        ),
        "domainCounts": dict(sorted(domain_counts.items())),
        "confidenceCounts": dict(sorted(confidence_counts.items())),
        "unresolvedRefs": unresolved,
        "ambiguousRefs": ambiguous,
        "nonClaims": [
            "A text or metadata hit is not a resolved ability activation.",
            "IL2CPP metadata dumps expose code shape and literals, not complete gameplay catalog rows.",
            "Runtime candidates remain experimental and are not promoted into CSV parity ability rows by this probe.",
        ],
    }


def print_table(refs: list[str], matches: list[Match]) -> None:
    by_ref: dict[str, list[Match]] = defaultdict(list)
    for match in matches:
        by_ref[match.ref].append(match)
    print("ref | domain | confidence | path | display | source")
    print("--- | --- | --- | --- | --- | ---")
    for ref in refs:
        entries = by_ref.get(ref)
        if not entries:
            print(f"{ref} | unresolved | unresolved |  |  | ")
            continue
        for match in entries[:8]:
            print(
                " | ".join(
                    [
                        ref,
                        match.candidate_domain,
                        match.confidence,
                        match.matched_record_key_path,
                        (match.matched_display_name or "").replace("|", "/"),
                        match.source_path,
                    ]
                )
            )


def parse_refs(args: argparse.Namespace) -> list[str]:
    refs: list[str] = []
    for value in args.ref or []:
        refs.extend(part for part in value.replace(",", " ").split() if part.strip())
    if args.refs_file:
        for line in Path(args.refs_file).read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if stripped and not stripped.startswith("#"):
                refs.append(stripped)
    if args.demo_refs:
        refs.extend(DEFAULT_REFS)
    for path in args.refs_from_json or []:
        refs.extend(extract_refs_from_json_path(Path(path)))
    deduped: list[str] = []
    seen: set[str] = set()
    for ref in refs:
        normalized = normalize_ref(ref)
        if normalized and normalized not in seen:
            deduped.append(normalized)
            seen.add(normalized)
    return deduped


def extract_refs_from_json_value(value: Any, refs: list[str]) -> None:
    if isinstance(value, dict):
        for key, entry in value.items():
            if key in REF_FIELD_NAMES and isinstance(entry, (str, int)) and not isinstance(entry, bool):
                refs.append(str(entry))
            elif key in REF_ARRAY_FIELD_NAMES and isinstance(entry, list):
                for item in entry:
                    if isinstance(item, (str, int)) and not isinstance(item, bool):
                        refs.append(str(item))
            extract_refs_from_json_value(entry, refs)
    elif isinstance(value, list):
        for entry in value:
            extract_refs_from_json_value(entry, refs)


def extract_refs_from_json_path(path: Path) -> list[str]:
    refs: list[str] = []
    try:
        if path.suffix.lower() == ".jsonl":
            with path.open("r", encoding="utf-8") as handle:
                for line in handle:
                    if line.strip():
                        extract_refs_from_json_value(json.loads(line), refs)
        else:
            extract_refs_from_json_value(json.loads(path.read_text(encoding="utf-8")), refs)
    except Exception as exc:
        raise SystemExit(f"Failed to extract refs from {path}: {exc}") from exc
    return refs


def default_roots(repo_root: Path) -> list[Path]:
    return [
        repo_root / "tools" / "il2cpp-dump",
        repo_root / ".ax" / "cache" / "dump-corpus.jsonl",
        repo_root / ".ax" / "cache" / "stfc.db",
        repo_root / ".ax" / "tools" / "Il2CppDumper" / "stringliteral.json",
    ]


def run_probe(args: argparse.Namespace) -> dict[str, Any]:
    repo_root = Path(args.repo_root).resolve()
    refs = parse_refs(args)
    if not refs:
        raise SystemExit("No refs supplied. Use --ref, --refs-file, or --demo-refs.")
    roots = [Path(root).resolve() for root in args.root]
    if args.default_roots:
        roots.extend(default_roots(repo_root))
    max_file_bytes = args.max_file_mb * 1024 * 1024
    files, searched_roots = discover_files(roots, max_file_bytes)
    ref_input_paths = {Path(path).resolve() for path in args.refs_from_json or []}
    files = [path for path in files if path.resolve() not in ref_input_paths]
    matches = scan_files(files, set(refs), max_file_bytes)
    return {
        "summary": build_summary(refs, matches, searched_roots, len(files)),
        "refs": refs,
        "matches": [match.as_json() for match in matches],
    }


def run_self_test() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        fixture = {
            "officers": {
                "4290764940": {
                    "id": 4290764940,
                    "displayName": "SNW Test Officer",
                    "officerAbilityId": 1120204726,
                }
            },
            "components": [{"componentId": "1280858269", "name": "Probe Phaser"}],
        }
        (root / "catalog.json").write_text(json.dumps(fixture), encoding="utf-8")
        analytics = {
            "type": "battle.analytics",
            "analytics": {
                "attackRows": [
                    {
                        "componentIdExact": "1280858269",
                        "attacker": {"hullIdsExact": ["3426564736"]},
                        "runtimeAbilityRowCandidates": [
                            {"sourceRef": "4290764940", "effectRef": "1120204726"}
                        ],
                    }
                ]
            },
        }
        analytics_path = root / "analytics.json"
        analytics_path.write_text(json.dumps(analytics), encoding="utf-8")
        args = argparse.Namespace(
            repo_root=str(root),
            root=[str(root)],
            default_roots=False,
            ref=["999"],
            refs_file=None,
            refs_from_json=[str(analytics_path)],
            demo_refs=False,
            max_file_mb=5,
        )
        result = run_probe(args)
        summary = result["summary"]
        assert summary["matchedRefCount"] == 3, summary
        matches = result["matches"]
        assert any(match["ref"] == "4290764940" and match["confidence"] == "exact_id_match" for match in matches)
        assert any(match["ref"] == "1280858269" and match["candidateDomain"] == "component" for match in matches)
        assert "999" in summary["unresolvedRefs"]


def main() -> int:
    parser = argparse.ArgumentParser(description="Scout local catalog/dump files for exact battle runtime refs.")
    parser.add_argument("--repo-root", default=Path.cwd(), help="Repository root used for default dump paths.")
    parser.add_argument("--root", action="append", default=[], help="File or directory to scan. May be repeated.")
    parser.add_argument("--default-roots", action="store_true", help="Scan repo-local IL2CPP/AX dump surfaces.")
    parser.add_argument("--ref", action="append", help="Exact ref string(s), comma or space separated. May be repeated.")
    parser.add_argument("--refs-file", help="Text file containing one ref per line.")
    parser.add_argument("--refs-from-json", action="append", help="Battle analytics/report JSON or JSONL to extract refs from.")
    parser.add_argument("--demo-refs", action="store_true", help="Include the current runtime-discovery seed refs.")
    parser.add_argument("--max-file-mb", type=int, default=256, help="Maximum text/JSON file size to scan.")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON.")
    parser.add_argument("--self-test", action="store_true", help="Run built-in fixture tests and exit.")
    args = parser.parse_args()

    if args.self_test:
        run_self_test()
        print("battle_ref_resolver_probe self-test passed")
        return 0

    result = run_probe(args)
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(json.dumps(result["summary"], indent=2, sort_keys=True))
        print()
        print_table(result["refs"], [Match(**match) for match in result["matches"]])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
