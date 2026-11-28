"""Conservative PR routing and unchanged release-producer/required-check boundaries."""

import copy
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location("ci_scope", ROOT / "scripts/ci-scope.py")
CI = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CI)
V2 = {"schema": 1, "channels": {"release": "v2.2.0", "pre": "v2.2.0-rc.1", "dev": "v2.2.0-dev.1"}}
V1 = {"schema": 1, "channels": {"release": "v1.1.9.5-guffa.13", "pre": "v1.1.9.5-guffa.12",
                                 "dev": "v1.1.9.5-guffa.7-dev.5"}}


def change(path, status="M", old="100644", new="100644"):
    return status, path, old, new


class Scope(unittest.TestCase):
    def route(self, paths, blobs=None, event="pull_request"):
        blobs = blobs or {}
        return CI.classify(event, paths, lambda side, path: blobs[(side, path)])

    def test_push_dispatch_and_empty_diffs_keep_native_producers(self):
        for event in ("push", "workflow_dispatch", "pull_request_target", "unknown"):
            with self.subTest(event=event):
                self.assertEqual(self.route([change("README.md")], event=event), "native")
        self.assertEqual(self.route([]), "native")

    def test_markdown_and_both_index_paths_are_conservative(self):
        for path in ("README.md", "CHANGELOG.md", "CONTRIBUTING.md", "docs/FEATURES.md", "docs/migration/new.md"):
            with self.subTest(path=path):
                self.assertEqual(self.route([change(path)]), "docs")
        self.assertEqual(self.route([change(path) for path in sorted(CI.INDEXES)]), "index")
        for path in ("src/mod/config.h", "build.zig", "build.zig.zon", "zig/mod.zig", "scripts/release_version.py",
                     ".github/workflows/ci.yaml", "AGENTS.md", "CLAUDE.md", "docs/THIRD_PARTY.md",
                     "docs/migration/catalog.json", "docs/image.svg", "unrecognized.md"):
            with self.subTest(path=path):
                self.assertEqual(self.route([change(path)]), "native")
                self.assertEqual(self.route([change("README.md"), change(path)]), "native")
        self.assertEqual(self.route([change("README.md"), change("releases/update-index-v2.json")]), "native")

    def test_rename_delete_type_mode_and_unsafe_names_keep_native(self):
        for item in (change("README.md", "D"), change("docs/new.md", "R100"),
                     change("docs/link.md", new="120000"), change("docs/executable.md", new="100755"),
                     change("docs/new.md", "T"), change("docs/x.md", old="120000"),
                     change("docs/../README.md"), change("docs/x\n.md"), change("docs\\x.md"),
                     change("releases/update-index-v2.json", "A", old="000000")):
            with self.subTest(change=item):
                self.assertEqual(self.route([item]), "native")
        self.assertEqual(self.route([change("docs/new.md", "A", old="000000")]), "docs")

    def test_only_semantically_unchanged_example_comments_can_skip_native(self):
        path = "docs/configuration/example_community_patch_settings_en.toml"
        base = b"[ui]\nenabled=true\ncount=1\n"
        for value, mode in ((b"# Comment\n[ui]\ncount = 1\nenabled = true\n", "docs"),
                            (b"[ui]\nenabled=1\ncount=1\n", "native"),
                            (b"[ui]\nenabled=true\ncount=1.0\n", "native"),
                            (b"[ui]\nenabled=false\ncount=1\n", "native"),
                            (b"[ui\n", "native")):
            with self.subTest(value=value):
                self.assertEqual(self.route([change(path)], {("base", path): base, ("head", path): value}), mode)

    def validate(self, previous, candidate, path="releases/update-index-v2.json"):
        CI.validate_feeds([change(path)], lambda side, _: json.dumps(previous if side == "base" else candidate).encode())

    def test_checks_only_changed_channels_and_all_changed_feeds(self):
        candidate = copy.deepcopy(V2)
        candidate["channels"].update(release="v2.3.0", dev="v2.3.0-dev.1")
        with patch.object(CI.INDEX, "verify_published") as verify:
            self.validate(V2, candidate)
            self.assertEqual(verify.call_args_list, [unittest.mock.call("v2.3.0", "release"),
                                                    unittest.mock.call("v2.3.0-dev.1", "dev")])
        with patch.object(CI.INDEX, "verify_published") as verify:
            self.validate(V2, V2)
            verify.assert_not_called()
        older = copy.deepcopy(V1)
        older["channels"]["release"] = "v1.1.9.5-guffa.12"
        for path, before, after in (("releases/update-index.json", older, V1),
                                    ("releases/update-index-v2.json", V2, candidate)):
            with self.subTest(path=path), patch.object(CI.INDEX, "verify_published") as verify:
                self.validate(before, after, path)
                self.assertTrue(verify.called)

    def test_bad_schema_unchanged_bad_channel_and_regression_fail_closed(self):
        for alter in (lambda d: d.update(schema=True), lambda d: d["channels"].pop("dev"),
                      lambda d: d["channels"].update(pre="v2.2.0"),
                      lambda d: d["channels"].update(release="v2.1.0")):
            candidate = copy.deepcopy(V2)
            alter(candidate)
            with self.subTest(candidate=candidate), patch.object(CI.INDEX, "verify_published") as verify:
                with self.assertRaises(ValueError):
                    self.validate(V2, candidate)
                verify.assert_not_called()
        with self.assertRaises(ValueError):
            self.validate(V1, V2, "releases/update-index.json")
        with self.assertRaises(ValueError):
            CI.feed_json(b'{"schema":1,"schema":1,"channels":{}}')

    def test_multiple_index_changes_validate_each_changed_channel(self):
        before = {"releases/update-index.json": copy.deepcopy(V1),
                  "releases/update-index-v2.json": copy.deepcopy(V2)}
        after = copy.deepcopy(before)
        after["releases/update-index.json"]["channels"]["pre"] = "v1.1.9.5-guffa.14"
        after["releases/update-index-v2.json"]["channels"]["release"] = "v2.3.0"
        with patch.object(CI.INDEX, "verify_published") as verify:
            CI.validate_feeds([change(path) for path in sorted(before)],
                              lambda side, path: json.dumps((before if side == "base" else after)[path]).encode())
            self.assertCountEqual(verify.call_args_list,
                                  [unittest.mock.call("v1.1.9.5-guffa.14", "pre"),
                                   unittest.mock.call("v2.3.0", "release")])

    def test_existing_publication_verifier_rejects_bad_asset_digest(self):
        digest = "a" * 64
        tag = "v2.3.0"
        candidate = copy.deepcopy(V2)
        candidate["channels"]["release"] = tag
        release = {"tag_name": tag, "draft": False, "prerelease": False, "published_at": "now",
                   "assets": [{"name": name, "state": "uploaded", "size": 1, "digest": f"sha256:{digest}"}
                              for name in (*CI.INDEX.PLATFORM_ASSETS, "release-manifest.json")]}
        manifest = {"repository": CI.INDEX.REPOSITORY, "tag": tag, "prerelease": False,
                    "payloadSha256": {name: digest for name in CI.INDEX.PLATFORM_ASSETS}}
        with patch.object(CI.INDEX, "fetch_json", side_effect=[release, manifest]):
            self.validate(V2, candidate)
        release["assets"][0]["digest"] = f"sha256:{'b' * 64}"
        with patch.object(CI.INDEX, "fetch_json", side_effect=[release, manifest]), self.assertRaises(ValueError):
            self.validate(V2, candidate)
        release["draft"] = True
        with patch.object(CI.INDEX, "fetch_json", return_value=release), self.assertRaises(ValueError):
            self.validate(V2, candidate)

    def test_real_git_blobs_are_data_and_whitespace_validation_can_fail(self):
        with tempfile.TemporaryDirectory(prefix="stfc-ci-scope-") as temporary:
            root = Path(temporary)
            def git(*args):
                return CI.git(*args, root=root).decode().strip()
            git("init", "-q")
            git("config", "user.email", "fixture@example.invalid")
            git("config", "user.name", "Fixture")
            git("config", "core.autocrlf", "false")
            (root / "README.md").write_text("Before\n", newline="\n")
            git("add", "README.md")
            git("commit", "-qm", "base")
            base = git("rev-parse", "HEAD")
            (root / "README.md").write_text("After  \n", newline="\n")
            git("commit", "-qam", "candidate")
            head = git("rev-parse", "HEAD")
            changed = CI.changes(base, head, root)
            self.assertEqual(changed, [change("README.md")])
            read = lambda side, path: CI.blob(base if side == "base" else head, path, root)
            self.assertEqual(CI.classify("pull_request", changed, read), "docs")
            with self.assertRaises(subprocess.CalledProcessError):
                CI.validate_docs(changed, read, base, head, root)
            with self.assertRaises(ValueError):
                CI.changes("--invalid", head, root)

    def test_documentation_checks_reject_nontext_and_invalid_utf8(self):
        changed = [change("README.md")]
        for data in (b"not\0text", b"\xff"):
            with self.subTest(data=data), patch.object(CI.subprocess, "run"):
                with self.assertRaises(ValueError):
                    CI.validate_docs(changed, lambda *_: data, "a" * 40, "b" * 40)

    def test_required_jobs_keep_producer_identity_and_fail_on_scope_failure(self):
        workflow = (ROOT / ".github/workflows/ci.yaml").read_text()
        jobs = dict(re.findall(r"^  ([a-z][a-z-]*):\n(.*?)(?=^  [a-z][a-z-]*:|\Z)", workflow,
                               flags=re.M | re.S))
        self.assertIn("run_attempt: ${{ steps.build_identity.outputs.run_attempt }}", jobs["build-win"])
        self.assertIn("name: build-mac (${{ matrix.arch }})", jobs["build-mac"])
        self.assertIn("'ubuntu-24.04' || 'windows-2025-vs2026'", jobs["build-win"])
        self.assertIn("'ubuntu-24.04' || matrix.runner", jobs["build-mac"])
        for job in ("build-win", "build-mac"):
            body = jobs[job]
            self.assertIn("if: ${{ always() }}", body)
            self.assertIn("github.event_name == 'pull_request' && (needs.scope.outputs.mode == 'docs'", body)
            first, *native = re.findall(r"^      - .*?(?=^      - |\Z)", body, flags=re.M | re.S)
            self.assertIn("Require successful scope validation", first)
            self.assertIn('"$SCOPE_RESULT" == success', first)
            self.assertIn("^(native|docs|index)$", first)
            for step in native:
                self.assertIn("needs.scope.outputs.mode == 'native'", step)
        scope = jobs["scope"]
        self.assertIn("github.event.pull_request.base.sha || github.sha", scope)
        self.assertIn("persist-credentials: false", scope)
        self.assertIn('"$EVENT" != pull_request', scope)
        self.assertIn('! -f scripts/ci-scope.py', scope)
        self.assertIn('echo "mode=native"', scope)


if __name__ == "__main__":
    unittest.main()
