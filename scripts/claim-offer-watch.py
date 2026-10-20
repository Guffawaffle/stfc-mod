#!/usr/bin/env python3
"""Keep a passive name catalogue for Augment offer variants; join it to the existing selection archive."""

import argparse
import hashlib
import html
import importlib.util
import json
import os
import re
import shutil
import sys
import time
import unicodedata
from collections import Counter
from pathlib import Path


KIND = "stfc-named-offer-catalogue"
MAX_LOCALE = 32 * 1024**2
MAX_VARIANTS = 512


def sibling(name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), Path(__file__).with_name(name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def alias(value):
    a, b = 14695981039346656037, 7809847782465536322
    for byte in str(value).encode("ascii"):
        a = ((a ^ byte) * 1099511628211) & ((1 << 64) - 1)
        b = ((b ^ byte) * 14029467366897019727) & ((1 << 64) - 1)
    return f"b{a:016x}{b:016x}"


def family(text):
    text = " ".join(unicodedata.normalize("NFKC", html.unescape(re.sub(r"<[^>]*>", "", text))).casefold().split())
    if re.fullmatch(r"priority\s*1 mega claim", text):
        return "priority-1-mega-claim"
    if text == "forbidden tech mega claim":
        return "forbidden-tech-mega-claim"
    if re.fullmatch(r"planetary charts(?:\s*\(max\s+[0-9][0-9,. ]*\))?", text):
        return "planetary-charts"
    return None


def catalogue(cache):
    """Inspect the generated LocalizationCacheData schema, never arbitrary extracted strings."""
    if cache.language != "en":
        raise ValueError("Name matching requires the English localization catalogue")
    categories = [c for c in cache.categories.values() if c.info.name == "bundle"]
    if not categories:
        raise ValueError("Localization catalogue has no bundle category")
    offers = {}
    for category in categories:
        if len(category.translations) > 500000:
            raise ValueError("Bundle localization exceeds the observation bound")
        for translation in category.translations.values():
            if not re.fullmatch(r"-?[0-9]{1,19}", translation.id):
                continue
            value = int(translation.id)
            if not -(2**63) <= value < 2**63:
                continue
            match = family(translation.text) if len(translation.text) <= 2048 else None
            if match:
                row = offers.setdefault(alias(value), dict(catalogue_id=value, families=set(), names=set(), translations=[]))
                row["families"].add(match)
                row["names"].add(translation.text)
        for translation in category.translations.values():
            if not re.fullmatch(r"-?[0-9]{1,19}", translation.id):
                continue
            row = offers.get(alias(int(translation.id)))
            if row is not None:
                if len(translation.text) > 32768 or len(row["translations"]) >= 128:
                    raise ValueError("Watched offer localization exceeds the observation bound")
                row["translations"].append(dict(key=str(translation.key), text=translation.text))
    if len(offers) > MAX_VARIANTS:
        raise ValueError("Too many matching offer variants; preserve and investigate")
    return {key: dict(row, families=sorted(row["families"]), names=sorted(row["names"]),
                      translations=sorted(row["translations"], key=lambda item: (item["key"], item["text"])))
            for key, row in sorted(offers.items())}


def decode(raw, proto_dir):
    sys.path.insert(0, str(proto_dir.resolve(strict=True)))
    from Digit.Client import Localization_pb2
    cache = Localization_pb2.LocalizationCacheData()
    cache.ParseFromString(raw)
    return catalogue(cache)


class Catalogue:
    def __init__(self, root, max_bytes=100 * 1024**2, reserve_bytes=10 * 1024**3):
        self.root, self.max_bytes, self.reserve_bytes = root, max_bytes, reserve_bytes
        self.io = sibling("claim-selection-archive")
        self.root.mkdir(parents=True, exist_ok=True)
        self.index = dict(schema=1, kind=KIND, offers={}, locale_hashes=[])
        self.index_path = root / "catalogue.json"
        existing = None
        if self.index_path.exists():
            existing = json.loads(self.index_path.read_text(encoding="utf-8"))
            if existing.get("schema") != 1 or existing.get("kind") != KIND:
                raise ValueError("Unsupported catalogue; preserve and investigate")
        # Immutable snapshots are authoritative if a crash interrupted the index update.
        for path in sorted(root.glob("locale-*.json")):
            snapshot = json.loads(path.read_text(encoding="utf-8"))
            if path.name != f"locale-{snapshot.get('locale_sha256')}.json":
                raise ValueError("Locale snapshot filename/digest mismatch")
            self.merge(snapshot)
        if existing and not set(existing["locale_hashes"]).issubset(self.index["locale_hashes"]):
            raise ValueError("Indexed locale evidence is missing; preserve and investigate")
        self.io.atomic_json(self.index_path, self.index)

    def merge(self, snapshot):
        if snapshot.get("schema") != 1 or snapshot.get("kind") != KIND:
            raise ValueError("Unsupported locale snapshot")
        digest = snapshot["locale_sha256"]
        if not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError("Invalid locale snapshot digest")
        if digest in self.index["locale_hashes"]:
            return
        for label, row in snapshot["offers"].items():
            if alias(row["catalogue_id"]) != label or not row["families"]:
                raise ValueError("Invalid offer identity in locale snapshot")
            target = self.index["offers"].setdefault(label, dict(catalogue_id=row["catalogue_id"], families=[], names=[], locale_hashes=[]))
            for field in ("families", "names"):
                target[field] = sorted(set(target[field]) | set(row[field]))
            target["locale_hashes"].append(digest)
        if len(self.index["offers"]) > MAX_VARIANTS:
            raise ValueError("Too many historical variants; preserve and investigate")
        self.index["locale_hashes"].append(digest)

    def capture(self, path, proto_dir, decoder=decode):
        with path.open("rb") as stream:
            raw = stream.read(MAX_LOCALE + 1)
        if len(raw) > MAX_LOCALE:
            raise ValueError("Locale file exceeds 32 MiB observation bound")
        digest = hashlib.sha256(raw).hexdigest()
        if digest in self.index["locale_hashes"]:
            return False
        offers = decoder(raw, proto_dir)
        snapshot = dict(schema=1, kind=KIND, locale_sha256=digest, offers=offers)
        encoded = (json.dumps(snapshot, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        used = sum(p.stat().st_size for p in self.root.glob("*.json"))
        # Allow room for both index replacement and its temporary file.
        estimate = len(encoded) + 2 * (self.index_path.stat().st_size + len(encoded))
        if used + estimate > self.max_bytes or shutil.disk_usage(self.root).free - estimate < self.reserve_bytes:
            raise ValueError("Catalogue budget/free-space reserve reached; existing evidence retained")
        self.io.atomic_json(self.root / f"locale-{digest}.json", snapshot)
        self.merge(snapshot)
        self.io.atomic_json(self.index_path, self.index)
        return True

    def status(self, state, error=None):
        self.io.atomic_json(self.root / "offer_watch_status.json", dict(
            schema=1, state=state, error=error, pid=os.getpid(), utc_ms=int(time.time() * 1000),
            catalogue_variants=len(self.index["offers"]), locale_versions=len(self.index["locale_hashes"]),
            scope="Localized offer-family catalogue; actual encounters remain in the separate selection archive"))


def report(index, events, limit=30):
    known = index["offers"]
    # Learn bundle/localization relationships before correlating preference aliases.
    links = dict(known)
    observations = {}
    # Stream archives twice; never retain the potentially 50 GiB archive in memory.
    iterate = events if callable(events) else lambda: iter(events)
    history = sibling("claim-selection-history")
    for event in iterate():
        detail = event.get("details") or {}
        available = detail.get("available") or {}
        candidates = available.get("items", []) if event.get("event") == "prune" and available.get("supported") else []
        if isinstance(detail.get("bundle"), dict):
            candidates = [*candidates, detail["bundle"]]
        for bundle in candidates:
            label = bundle.get("bundle")
            source = known.get(bundle.get("localization"))
            if label and source:
                # Keep ambiguities visible rather than choosing an equivalence.
                target = links.get(label)
                if target and target["families"] != source["families"]:
                    source = dict(source, families=sorted(set(target["families"]) | set(source["families"])))
                if target:
                    source = dict(source, names=sorted(set(target["names"]) | set(source["names"])))
                links[label] = source

    def add(label, event, kind, **detail):
        if label not in links:
            return
        source = links[label]
        item = observations.setdefault(label, dict(bundle=label,
            bundle_id=known[label]["catalogue_id"] if label in known else None,
            families=source["families"], names=source["names"], counts=Counter(), recent=[]))
        reference = {key: event.get(key) for key in ("utc_ms", "session", "seq")}
        row = dict(reference, kind=kind, **detail)
        item["counts"][kind] += 1
        if kind == "available":
            item.setdefault("first_available", row)
            item["last_available"] = row
        item["recent"].append(row)
        item["recent"] = item["recent"][-limit:]

    unknown = Counter()
    for event in iterate():
        method = event.get("event")
        detail = event.get("details") or {}
        available = detail.get("available") or {}
        if method == "prune" and not available.get("supported"):
            unknown["available_offer_layout"] += 1
        unknown["selection_scopes"] += len(event.get("selection_unknown", []))
        if method == "prune" and available.get("supported"):
            for bundle in available.get("items", []):
                add(bundle.get("bundle"), event, "available", category=detail.get("category"), localization=bundle.get("localization"), waiting_for_refresh=bundle.get("waiting_for_refresh"))
        if method == "edit":
            add((detail.get("bundle") or {}).get("bundle"), event, "edit", quantity=detail.get("quantity"))
        for delta in event.get("selection_changes", []):
            for change in delta["changes"]:
                add(change["bundle"], event, "selection_change", scope=delta["scope"], before=change["before"], after=change["after"])
        if method in ["preferences_get", "preferences_set"] and detail.get("key") == "FactionSelection":
            values = history.quantities(detail.get("read", detail.get("written")), "bundle")
            if values is not None and not event.get("unwind"):
                for label in links:
                    add(label, event, method, lane=detail["key"], quantity=values.get(label, 0))
            else:
                unknown["faction_preference_list"] += 1
    return dict(schema=1, kind="stfc-named-offer-history", offers=list(observations.values()),
                recent_limit=limit, unknown_observations=dict(unknown), limits="Names group candidates, not equivalent purchases. Localized text is not a live price/reward/eligibility or server snapshot. Missing encounter records do not prove absence; catalogue membership is not an encounter. Session/client labels do not establish account identity.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["watch", "report"])
    parser.add_argument("--output", type=Path, required=True, help="Private named-catalogue directory")
    parser.add_argument("--locale", type=Path)
    parser.add_argument("--seed-locale", type=Path, action="append", default=[])
    parser.add_argument("--proto-dir", type=Path, help="Matching generated Digit.Client.Localization protobuf modules; requires protobuf Python runtime")
    parser.add_argument("--archive-dir", type=Path)
    parser.add_argument("--limit", type=int, default=30)
    parser.add_argument("--interval", type=float, default=30)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.limit <= 1000 or not 1 <= args.interval <= 300:
        parser.error("Invalid recent limit or polling interval")
    if args.action == "report":
        if not args.archive_dir:
            parser.error("report requires --archive-dir")
        index = json.loads((args.output / "catalogue.json").read_text(encoding="utf-8"))
        if index.get("schema") != 1 or index.get("kind") != KIND:
            raise ValueError("Unsupported catalogue")
        result = report(index, lambda: sibling("claim-selection-history").journal_events(args.archive_dir), args.limit)
        state_path = args.archive_dir / "archive_state.json"
        if state_path.exists():
            state = json.loads(state_path.read_text(encoding="utf-8"))
            result["archive"] = {key: state.get(key) for key in ("client", "source", "gaps", "last_event_utc_ms")}
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 0
    if not args.locale or not args.proto_dir:
        parser.error("watch requires --locale and --proto-dir")
    with sibling("claim-selection-archive").exclusive(args.output):
        watch = Catalogue(args.output)
        try:
            for seed in args.seed_locale:
                watch.capture(seed, args.proto_dir)
        except Exception as error:
            watch.status("error", str(error))
            return 1
        while True:
            try:
                watch.capture(args.locale, args.proto_dir)
                watch.status("idle" if args.once else "running")
            except Exception as error:
                watch.status("error", str(error))
                if args.once:
                    return 1
            if args.once:
                return 0
            time.sleep(args.interval)


if __name__ == "__main__":
    raise SystemExit(main())
