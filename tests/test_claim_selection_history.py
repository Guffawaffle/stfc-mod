import importlib.util
import unittest
from pathlib import Path


spec = importlib.util.spec_from_file_location("selection_history", Path(__file__).resolve().parents[1] / "scripts/claim-selection-history.py")
history = importlib.util.module_from_spec(spec)
spec.loader.exec_module(history)


def selection(**values):
    return dict(supported=True, count=len(values), entries=[dict(bundle=key, quantity=value) for key, value in values.items()])


def preference(method, values, *, session="one", seq=1, key="RefineSelection", cloud=True):
    return dict(event="preferences_" + method, session=session, seq=seq,
                details=dict(key=key, **{dict(set="written", get="read")[method]: values}),
                after=dict(cloud_loaded=cloud))


class SelectionHistoryTests(unittest.TestCase):
    def test_deselection_reappears_after_restart(self):
        result = history.analyze([
            preference("set", selection(reliant=0, other=2)),
            preference("get", selection(reliant=1, other=2), session="two")])
        findings = result["reads_differing_from_last_observed_write"]
        self.assertEqual(findings[0]["changes"], [dict(bundle="reliant", before=0, after=1)])
        self.assertFalse(findings[0]["same_session"])

    def test_omitted_zero_is_not_a_reset(self):
        result = history.analyze([
            preference("set", selection(reliant=0, other=2)),
            preference("get", selection(other=2))])
        self.assertEqual(result["reads_differing_from_last_observed_write"], [])

    def test_unreadable_write_invalidates_baseline(self):
        result = history.analyze([
            preference("set", selection(augment=1)),
            preference("set", dict(supported=False)),
            preference("get", selection())])
        self.assertEqual(result["reads_differing_from_last_observed_write"], [])
        self.assertEqual(len(result["unknown_observations"]), 1)

    def test_separate_preference_lanes(self):
        result = history.analyze([
            preference("set", selection(augment=1), key="FactionSelection"),
            preference("set", selection(augment=2), key="RefineSelection"),
            preference("get", selection(augment=1), key="FactionSelection")])
        self.assertEqual(result["reads_differing_from_last_observed_write"], [])

    def test_unsupported_entry_cannot_be_mistaken_for_an_empty_list(self):
        invalid = dict(supported=True, count=1, entries=[dict(supported=False)])
        result = history.analyze([
            preference("set", selection(augment=1)), preference("get", invalid)])
        self.assertEqual(result["reads_differing_from_last_observed_write"], [])
        self.assertEqual(len(result["unknown_observations"]), 1)

    def test_native_prune_retains_quantity_and_identifies_removal(self):
        def state(**values):
            return dict(pending=dict(supported=True, count=len(values),
                                     entries=[dict(key=key, quantity=value) for key, value in values.items()]))
        result = history.analyze([dict(event="prune", before=state(charts=1, other=2), after=state(other=2))])
        self.assertEqual(result["state_changes"][0]["changes"], [dict(bundle="charts", before=1, after=0)])

    def test_group_removal_requires_complete_collection_evidence(self):
        selected = dict(supported=True, count=1, entries=[dict(key="charts", quantity=1)])
        before = dict(groups=dict(supported=True, entries=[dict(key="augment", selected=selected, editing=selected)]))
        after = dict(groups=dict(supported=True, entries=[]))
        result = history.analyze([dict(event="parse", before=before, after=after)])
        self.assertEqual(len(result["state_changes"]), 2)
        unknown = history.analyze([dict(event="parse", before=before, after=dict(groups=dict(supported=False)))])
        self.assertEqual(unknown["state_changes"], [])
        self.assertTrue(unknown["unknown_observations"])


    def test_compact_native_deltas_and_unknowns(self):
        delta = dict(scope="categories/1/selected", changes=[dict(bundle="charts", before=1, after=0)])
        result = history.analyze([dict(event="prune", selection_changes=[delta], selection_unknown=["pending"])])
        self.assertEqual(result["state_changes"][0]["changes"], delta["changes"])
        self.assertEqual(result["unknown_observations"][0]["scope"], "pending")

    def test_checkpoint_survives_rotation_of_original_write(self):
        event = preference("get", selection(reliant=1), session="two")
        event["checkpoint"] = dict(basis="write", session="one", values=selection())
        result = history.analyze([event])
        self.assertEqual(result["reads_differing_from_last_observed_write"], [])
        self.assertEqual(result["reads_differing_from_checkpoint"][0]["changes"],
                         [dict(bundle="reliant", before=0, after=1)])


if __name__ == "__main__":
    unittest.main()
