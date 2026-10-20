import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace as N
from unittest.mock import patch


spec = importlib.util.spec_from_file_location("offer_watch", Path(__file__).resolve().parents[1] / "scripts/claim-offer-watch.py")
watch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(watch)


def cache(*rows, language="en", category="bundle"):
    return N(language=language, categories={1: N(info=N(name=category), translations={
        index: N(id=str(id_), key=index, text=text) for index, (id_, text) in enumerate(rows)})})


def catalogue(*rows):
    return dict(schema=1, kind=watch.KIND, offers=watch.catalogue(cache(*rows)))


def event(method, seq=1, **kwargs):
    return dict(event=method, session="100_1", seq=seq, utc_ms=seq, **kwargs)


class OfferWatchTests(unittest.TestCase):
    def test_matching_normalizes_markup_unicode_and_caps_without_description_substrings(self):
        self.assertEqual(watch.family("<color=#fac710>Priority\u00a01 Mega Claim</color>"), "priority-1-mega-claim")
        self.assertEqual(watch.family("Forbidden Tech Mega Claim"), "forbidden-tech-mega-claim")
        self.assertEqual(watch.family("Planetary Charts (Max 10,000)"), "planetary-charts")
        self.assertIsNone(watch.family("Used in Forbidden Tech Mega Claim"))
        self.assertIsNone(watch.family("Priority 1 Mega Claim Pack"))
        self.assertIsNone(watch.family("Planetary Charts Elite"))

    def test_schema_scope_and_unknown_language_fail_explicitly(self):
        with self.assertRaises(ValueError):
            watch.catalogue(cache((1, "Planetary Charts"), language="de"))
        with self.assertRaises(ValueError):
            watch.catalogue(cache((1, "Planetary Charts"), category="materials"))

    def test_exact_ids_keep_variants_separate_and_capture_only_target_metadata(self):
        rows = watch.catalogue(cache((1, "Planetary Charts (Max 5,000)"), (1, "Costs 100 tokens"),
                                     (2, "Planetary Charts (Max 10,000)"), (3, "Unrelated")))
        self.assertEqual(len(rows), 2)
        self.assertEqual([x["text"] for x in rows[watch.alias(1)]["translations"]],
                         ["Planetary Charts (Max 5,000)", "Costs 100 tokens"])
        self.assertEqual(watch.alias(207108020), "b209f1ffa8d73204b5e68383418ab5472")

    def test_revised_locale_retains_old_variant_and_repeated_bytes_are_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            locale = root / "locale.bin"
            store = watch.Catalogue(root / "out", reserve_bytes=0)
            locale.write_bytes(b"old")
            store.capture(locale, root, lambda *_: watch.catalogue(cache((1, "Priority 1 Mega Claim"))))
            self.assertFalse(store.capture(locale, root, lambda *_: self.fail("Repeated payload decoded")))
            locale.write_bytes(b"new")
            store.capture(locale, root, lambda *_: watch.catalogue(cache((2, "Priority 1 Mega Claim"))))
            self.assertEqual(set(store.index["offers"]), {watch.alias(1), watch.alias(2)})
            self.assertEqual(len(list((root / "out").glob("locale-*.json"))), 2)

    def test_bad_live_locale_does_not_overwrite_last_valid_catalogue(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            locale = root / "locale.bin"
            store = watch.Catalogue(root / "out", reserve_bytes=0)
            locale.write_bytes(b"old")
            store.capture(locale, root, lambda *_: watch.catalogue(cache((1, "Planetary Charts"))))
            before = store.index_path.read_bytes()
            locale.write_bytes(b"partial")
            def invalid(*_):
                raise ValueError("Partial protobuf")
            with self.assertRaises(ValueError):
                store.capture(locale, root, invalid)
            self.assertEqual(store.index_path.read_bytes(), before)

    def test_snapshot_survives_interrupted_index_update(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            locale = root / "locale.bin"
            store = watch.Catalogue(root / "out", reserve_bytes=0)
            locale.write_bytes(b"new")
            real_atomic = store.io.atomic_json
            def interrupted(path, value):
                if path == store.index_path:
                    raise OSError("Interrupted index write")
                real_atomic(path, value)
            with patch.object(store.io, "atomic_json", side_effect=interrupted):
                with self.assertRaises(OSError):
                    store.capture(locale, root, lambda *_: watch.catalogue(cache((1, "Planetary Charts"))))
            recovered = watch.Catalogue(root / "out", reserve_bytes=0)
            self.assertIn(watch.alias(1), recovered.index["offers"])
            snapshot = next((root / "out").glob("locale-*.json"))
            snapshot.unlink()
            before = recovered.index_path.read_bytes()
            with self.assertRaises(ValueError):
                watch.Catalogue(root / "out", reserve_bytes=0)
            self.assertEqual(recovered.index_path.read_bytes(), before)

    def test_capacity_pause_preserves_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            locale = root / "locale.bin"
            locale.write_bytes(b"old")
            store = watch.Catalogue(root / "out", max_bytes=1, reserve_bytes=0)
            before = store.index_path.read_bytes()
            with self.assertRaises(ValueError):
                store.capture(locale, root, lambda *_: watch.catalogue(cache((1, "Planetary Charts"))))
            self.assertEqual(store.index_path.read_bytes(), before)
            self.assertEqual(list((root / "out").glob("locale-*.json")), [])

    def test_report_joins_distinct_bundle_and_localization_ids_and_preserves_deselection(self):
        index = catalogue((10, "Planetary Charts"))
        bundle = watch.alias(11)
        events = [event("edit", details=dict(bundle=dict(bundle=bundle), quantity=0)),
                  event("prune", 2, details=dict(category=5, available=dict(supported=True, items=[
                      dict(bundle=bundle, localization=watch.alias(10), waiting_for_refresh=False)]))),
                  event("preferences_get", 3, details=dict(key="FactionSelection", read=dict(supported=True, count=0, entries=[])))]
        scans = []
        def replay():
            scans.append(True)
            yield from events
        result = watch.report(index, replay, limit=10)
        row = next(row for row in result["offers"] if row["bundle"] == bundle)
        self.assertIsNone(row["bundle_id"])
        self.assertEqual(row["counts"]["edit"], 1)
        self.assertEqual(row["recent"][0]["quantity"], 0)
        self.assertEqual(row["first_available"]["category"], 5)
        self.assertEqual(len(scans), 2)

    def test_catalogue_presence_and_unsupported_availability_are_not_encounters(self):
        index = catalogue((1, "Planetary Charts"))
        result = watch.report(index, [event("prune", details=dict(available=dict(supported=False, items=[dict(bundle=watch.alias(1))])))])
        self.assertEqual(result["offers"], [])
        self.assertEqual(result["unknown_observations"]["available_offer_layout"], 1)

    def test_unsupported_layout_cannot_supply_a_bundle_localization_join(self):
        index = catalogue((1, "Planetary Charts"))
        result = watch.report(index, [event("prune", details=dict(available=dict(supported=False, items=[
            dict(bundle=watch.alias(2), localization=watch.alias(1))]))),
            event("edit", 2, details=dict(bundle=dict(bundle=watch.alias(2)), quantity=1))])
        self.assertEqual(result["offers"], [])


if __name__ == "__main__":
    unittest.main()
