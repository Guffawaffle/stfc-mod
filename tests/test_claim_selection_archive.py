import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]


def module(name, file):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / file)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


archive = module("archive", "claim-selection-archive.py")
history = module("history", "claim-selection-history.py")


def record(seq, session="100_1"):
    return dict(schema=1, session=session, seq=seq, event="edit", details=dict(quantity=seq), utc_ms=seq)


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / "source"
        self.destination = Path(self.temp.name) / "archive"
        self.source.mkdir()
        self.destination.mkdir()

    def collector(self, budget=1024**2):
        return archive.Archive(self.source, self.destination, "home", budget, 0)

    def write(self, name, *events, partial=b""):
        (self.source / name).write_bytes(b"".join((json.dumps(e) + "\n").encode() for e in events) + partial)

    def archived(self):
        return list(history.journal_events(self.destination))

    def test_rotation_and_restart_preserve_exactly_one_copy(self):
        self.write("bulk_selection.jsonl", record(1), record(2))
        first = self.collector()
        self.assertEqual(first.poll(), 2)
        (self.source / "bulk_selection.jsonl").rename(self.source / "bulk_selection.1.jsonl")
        self.write("bulk_selection.jsonl", record(3))
        self.assertEqual(first.poll(), 1)
        restarted = self.collector()
        self.assertEqual(restarted.poll(), 0)
        self.assertEqual([e["seq"] for e in self.archived()], [1, 2, 3])
        self.assertEqual(restarted.state["gaps"], [])

    def test_partial_final_record_is_retried_without_loss(self):
        serialized = json.dumps(record(2)).encode()
        self.write("bulk_selection.jsonl", record(1), partial=serialized[:20])
        worker = self.collector()
        self.assertEqual(worker.poll(), 1)
        with (self.source / "bulk_selection.jsonl").open("ab") as stream:
            stream.write(serialized[20:] + b"\n")
        self.assertEqual(worker.poll(), 1)
        self.assertEqual([e["seq"] for e in self.archived()], [1, 2])

    def test_crash_between_batch_and_checkpoint_recovers(self):
        self.write("bulk_selection.jsonl", record(1), record(2))
        worker = self.collector()
        with patch.object(archive, "atomic_json", side_effect=OSError("simulated checkpoint failure")):
            with self.assertRaises(OSError):
                worker.poll()
        recovered = self.collector()
        self.assertEqual(recovered.state["records"], 2)
        self.assertEqual(recovered.poll(), 0)
        self.assertEqual([e["seq"] for e in self.archived()], [1, 2])

    def test_budget_pauses_and_can_resume_without_deleting(self):
        self.write("bulk_selection.jsonl", record(1))
        worker = self.collector(1)
        with self.assertRaises(archive.CapacityError):
            worker.poll()
        self.assertEqual(worker.state["records"], 0)
        worker.max_bytes = 1024**2
        self.assertEqual(worker.poll(), 1)
        before = list(self.destination.glob("batch-*.jsonl.gz"))
        worker.max_bytes = 1
        with (self.source / "bulk_selection.jsonl").open("a") as stream:
            stream.write(json.dumps(record(2)) + "\n")
        with self.assertRaises(archive.CapacityError):
            worker.poll()
        self.assertEqual(list(self.destination.glob("batch-*.jsonl.gz")), before)

    def test_missing_retained_prefix_is_explicit(self):
        self.write("bulk_selection.jsonl", record(9))
        worker = self.collector()
        worker.poll()
        self.assertEqual(worker.state["gaps"], [dict(session="100_1", first_missing=1, last_missing=8)])

    def test_different_client_cannot_share_destination(self):
        self.collector()
        with self.assertRaises(ValueError):
            archive.Archive(self.source, self.destination, "work", 1024**2, 0)

    def test_bad_complete_record_does_not_advance_cursor(self):
        self.write("bulk_selection.jsonl", record(1), partial=b"bad-json\n")
        worker = self.collector()
        with self.assertRaises(ValueError):
            worker.poll()
        self.assertEqual(worker.state["records"], 0)
        self.assertEqual(worker.offsets, {})

    def test_second_collector_cannot_publish_to_same_destination(self):
        with archive.exclusive(self.destination):
            with self.assertRaises(OSError):
                with archive.exclusive(self.destination):
                    self.fail("A second collector acquired the destination lock")


if __name__ == "__main__":
    unittest.main()
