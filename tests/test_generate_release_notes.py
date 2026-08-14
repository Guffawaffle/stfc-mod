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
        for tag in (
            "v2.1.0-guffa.10",
            "v2.1.1-guffa.rc1",
            "v2.1.1.alpha.1",
            "v2.1.1.beta.1",
            "v0.6.1",
        ):
            with self.subTest(tag=tag):
                notes_dir = self.root / "docs" / "release-notes"
                notes_dir.mkdir(parents=True, exist_ok=True)
                source = notes_dir / f"{tag}.highlights.md"
                source.write_text("## Highlights\n\n- Clear player-facing identity.\n", encoding="utf-8")

                notes = resolve_curated_notes(tag, self.root, None, True)

                self.assertEqual("## Highlights\n\n- Clear player-facing identity.", notes)

    def test_required_curated_fragment_fails_when_missing(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "curated release notes not found"):
            resolve_curated_notes("v2.1.0-guffa.10", self.root, None, True)

    def test_curated_fragment_requires_exact_highlights_heading(self) -> None:
        source = self.root / "notes.md"
        for heading in ("## High~lights", "## ~Highlights", "## ~Highlights~"):
            with self.subTest(heading=heading), self.assertRaisesRegex(RuntimeError, "must start with"):
                validate_curated_notes(f"{heading}\n\n- Player-facing change.\n", source)

    def test_curated_fragment_rejects_generator_owned_sections(self) -> None:
        source = self.root / "notes.md"
        for fragment in (
            "## Release Assets",
            "## Included Changes ###",
            "## Technical References\t##",
            "### Merged PRs",
            "### Issues Fixed",
            "Release Assets\n--------------",
            "## Release **Assets**",
            "## Release ~~Assets~~",
            "## ~Release Assets~",
            "> ## Release Assets",
            "[Release Assets]: https://example.com\n\n## [Release Assets]",
            "## [Release Assets](https://example.com/a_(b)_tail)",
        ):
            with self.subTest(fragment=fragment), self.assertRaisesRegex(RuntimeError, "generator-owned heading"):
                validate_curated_notes(f"## Highlights\n\n{fragment}\n", source)

    def test_curated_fragment_rejects_markdown_release_titles(self) -> None:
        source = self.root / "notes.md"
        for fragment in (
            "#\tDuplicate release title",
            "Duplicate release title\n=======================",
            "> # Duplicate release title",
        ):
            with self.subTest(fragment=fragment), self.assertRaisesRegex(RuntimeError, "must not define the release title"):
                validate_curated_notes(f"## Highlights\n\n{fragment}\n", source)

    def test_curated_fragment_rejects_raw_html_headings(self) -> None:
        source = self.root / "notes.md"
        for fragment in (
            "<h2>Release Assets</h2>",
            "Text <h2>Release Assets</h2>",
            "<h2/>Release Assets</h2>",
        ):
            with self.subTest(fragment=fragment), self.assertRaisesRegex(RuntimeError, "must not use raw HTML headings"):
                validate_curated_notes(f"## Highlights\n\n{fragment}\n", source)

    def test_curated_fragment_rejects_content_free_fragments(self) -> None:
        source = self.root / "notes.md"
        for fragment in (
            "## Upgrade Notes",
            "<!-- Add release highlights here. -->",
            "---",
            "-",
            "[details]: https://example.com",
            "```text\n```",
            "[](https://example.com)",
            "[](https://example.com/a_(b)_tail)",
            "1.",
        ):
            with self.subTest(fragment=fragment), self.assertRaisesRegex(RuntimeError, "contain no player-facing content"):
                validate_curated_notes(f"## Highlights\n\n{fragment}\n", source)

    def test_curated_fragment_rejects_unterminated_markup(self) -> None:
        source = self.root / "notes.md"
        with self.assertRaisesRegex(RuntimeError, "unterminated"):
            validate_curated_notes("## Highlights\n\n<!-- unfinished\n", source)

    def test_curated_fragment_ignores_literal_headings_in_fenced_code(self) -> None:
        source = self.root / "notes.md"
        for content in (
            "# literal comment\n<h2>literal markup</h2>\n<!-- literal comment opener",
            "1.",
            "[](https://example.com/a_(b)_tail)",
        ):
            with self.subTest(content=content):
                notes = f"## Highlights\n\n```text\n{content}\n```\n"
                self.assertEqual(notes.strip(), validate_curated_notes(notes, source))

    def test_curated_fragment_ignores_literal_markup_in_inline_code(self) -> None:
        source = self.root / "notes.md"
        for content in ("Do not emit `<h2>` tags.", "Avoid `<!--` in output."):
            with self.subTest(content=content):
                notes = f"## Highlights\n\n- {content}\n"
                self.assertEqual(notes.strip(), validate_curated_notes(notes, source))

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
