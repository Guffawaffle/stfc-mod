from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from scripts.generate_release_notes import render_release_notes, resolve_curated_notes, validate_curated_notes


class ReleaseNotesTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_resolves_default_curated_fragment(self) -> None:
        notes_dir = self.root / "docs" / "release-notes"
        notes_dir.mkdir(parents=True)
        source = notes_dir / "v2.1.0-guffa.10.highlights.md"
        source.write_text("## Highlights\n\n- Clear player-facing identity.\n", encoding="utf-8")

        notes = resolve_curated_notes("v2.1.0-guffa.10", self.root, None, True)

        self.assertEqual("## Highlights\n\n- Clear player-facing identity.", notes)

    def test_required_curated_fragment_fails_when_missing(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "curated release notes not found"):
            resolve_curated_notes("v2.1.0-guffa.10", self.root, None, True)

    def test_curated_fragment_rejects_generator_owned_sections(self) -> None:
        source = self.root / "notes.md"
        with self.assertRaisesRegex(RuntimeError, "generator-owned heading"):
            validate_curated_notes("## Highlights\n\n## Release Assets\n", source)

    def test_curated_notes_precede_generated_inventory(self) -> None:
        notes = render_release_notes(
            tag="v2.1.0-guffa.10",
            previous_tag="v2.1.0-guffa.9",
            repo="Guffawaffle/stfc-mod",
            prs=[],
            issues=[],
            curated_notes="## Highlights\n\n- Clear player-facing identity.",
        )

        self.assertLess(notes.index("## Highlights"), notes.index("## Release Assets"))
        self.assertLess(notes.index("## Release Assets"), notes.index("## Included Changes"))


if __name__ == "__main__":
    unittest.main()
