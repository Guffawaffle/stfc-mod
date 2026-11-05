#!/usr/bin/env python3
"""Render the human feature catalog from the existing parity inventory."""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INVENTORY = ROOT / "docs/migration/play-functional-parity.json"
OUTPUT = ROOT / "docs/migration/FEATURE-CATALOG.md"


def cell(value):
    return str(value).replace("|", "\\|").replace("\n", " ").replace("\r", "")


def render(data):
    features = data["capabilities"]
    ids = [feature["id"] for feature in features]
    if len(ids) != len(set(ids)):
        raise ValueError("Duplicate capability ID")
    lines = ["# Community Mod feature catalog", "",
             "Generated from [the functional inventory](play-functional-parity.json) by",
             "`python scripts/render-feature-catalog.py`. Edit the inventory, then regenerate;",
             "`--check` detects a stale view. This is an incremental product catalog, not a",
             "claim that every branch or workflow has been inspected or verified.", "",
             "Branch presence refers to the exact **local revision inspected**. Consumer-only",
             "means the TOML/gameplay path exists but the named in-game control does not.",
             "Not assessed never means absent. Source presence, automated checks, builds,",
             "and player-confirmed behavior are separate evidence.", "",
             "## Features examined through the migration procedure", "",
             "| ID / feature | Player entry point | Upstream dev | play | play-dev | Refactor | Current qualification |",
             "| --- | --- | --- | --- | --- | --- | --- |"]
    assessed = [feature for feature in features if feature.get("branchObservations")]
    for feature in assessed:
        branches = {}
        for observation in feature["branchObservations"]:
            revision = observation["revision"]
            if not re.fullmatch(r"[0-9a-f]{40}", revision):
                raise ValueError(f"Unpinned branch observation for {feature['id']}")
            branches[observation["refRole"]] = observation
        values = []
        for role in ("upstream-dev", "play", "play-dev", "refactor"):
            observation = branches.get(role)
            values.append("not assessed" if not observation else
                          observation["presence"] + " @ `" + observation["revision"][:8] + "`")
        contract = feature.get("contract")
        label = cell(feature["id"] + " — " + feature["name"])
        if contract:
            if not (ROOT / contract).is_file():
                raise ValueError(f"Missing contract {contract}")
            label = f"[{label}]({Path(contract).name if '/' not in contract else 'features/' + Path(contract).name})"
        status = feature.get("catalogStatus", feature["classification"])
        row = [label, cell(feature.get("userEntryPoint", "not yet recorded")), *map(cell, values), cell(status)]
        lines.append("| " + " | ".join(row) + " |")
    lines += ["", "Full pinned paths, symbols, differences and history are in each inventory entry.",
              "Frozen play-dev remains `1baa55fedf1758b19f843cc59707b8901a0b4998`.", "",
              "## Other recorded capabilities", "",
              "These include the earlier PLAY reconciliation inventory. Their historical",
              "dispositions are retained; they have **not** all been rescanned against frozen",
              "play-dev or the current refactor. Newly discovered entries remain visible even",
              "when their migration is deferred.", "",
              "| ID | Capability | Recorded disposition | Source evidence |", "| --- | --- | --- | --- |"]
    for feature in features:
        if feature in assessed:
            continue
        evidence = feature.get("baseline", {})
        lines.append("| " + " | ".join(map(cell, [feature["id"], feature["name"], feature["classification"],
                                                 evidence.get("path", "not assessed")])) + " |")
    lines += ["", "The JSON remains the sole machine-readable inventory. This document adds no",
              "independent migration status or execution authority.", ""]
    if active := data.get("activeMigration"):
        lines += ["## Active migration", "", "Branch: `" + active["branch"] + "`. " + active["status"],
                  "See [branch routing and preserved checkpoints](PLAY-PARITY.md#active-ripper-migration-branch--2026-10-26).", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = render(json.loads(INVENTORY.read_text(encoding="utf-8")))
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != expected:
            raise SystemExit("Feature catalog is stale; regenerate from the inventory.")
        print("Feature catalog matches inventory; IDs, branch revisions and contract links checked.")
    else:
        OUTPUT.write_text(expected, encoding="utf-8")
        print(OUTPUT)


if __name__ == "__main__":
    main()
