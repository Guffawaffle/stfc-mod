import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/mac-release-context.py"
spec = importlib.util.spec_from_file_location("mac_release_context", SCRIPT)
context = importlib.util.module_from_spec(spec)
spec.loader.exec_module(context)


class MacReleaseContext(unittest.TestCase):
    def setUp(self):
        self.run = {"run_number": 987, "run_attempt": 2, "head_branch": "play", "event": "push",
                    "status": "completed", "conclusion": "success"}
        self.tag = "v1.1.9.5-guffa.6"

    def resolve(self, operation="sign-run", tag="", prerelease=True):
        return context.resolve(operation, tag, prerelease, self.run)

    def test_untagged_signed_download_is_pre_and_never_dev(self):
        result = self.resolve()
        self.assertEqual(result["channel"], "pre")
        self.assertEqual(result["selection_mode"], "test")
        self.assertTrue(result["prerelease"])
        with self.assertRaisesRegex(ValueError, "full Release requires"):
            self.resolve(prerelease=False)

    def test_explicit_pre_flag_not_tag_spelling_selects_named_channel(self):
        pre = self.resolve(tag=self.tag)
        release = self.resolve(tag=self.tag, prerelease=False)
        self.assertEqual(pre["release_tag"], release["release_tag"])
        self.assertEqual(pre["channel"], "pre")
        self.assertEqual(release["channel"], "release")
        self.assertEqual(release["selection_mode"], "release")

    def test_optional_pre_download_keeps_partial_and_failed_ci_policy(self):
        for status, conclusion in (("in_progress", None), ("completed", "failure")):
            self.run.update(status=status, conclusion=conclusion)
            self.assertEqual(self.resolve()["channel"], "pre")
            for prerelease in (True, False):
                with self.assertRaises(ValueError):
                    self.resolve(tag=self.tag, prerelease=prerelease)

    def test_successful_named_pre_can_use_existing_same_repo_test_sources(self):
        self.run.update(head_branch="feature/icons", event="pull_request")
        self.assertEqual(self.resolve(tag=self.tag)["channel"], "pre")
        self.assertEqual(self.resolve(tag=self.tag)["selection_mode"], "test")
        # Repository/workflow/artifact trust remains ci-signing-input.py's job.
        with self.assertRaises(ValueError):
            self.resolve(tag=self.tag, prerelease=False)

    def test_release_and_automatic_sign_keep_successful_play_push_policy(self):
        for operation in ("sign", "sign-run"):
            for key, value in (("head_branch", "feature/icons"), ("event", "pull_request")):
                original = copy.deepcopy(self.run)
                self.run[key] = value
                with self.assertRaises(ValueError):
                    self.resolve(operation=operation, tag=self.tag, prerelease=False)
                self.run = original
        self.run.update(head_branch="feature/icons")
        with self.assertRaises(ValueError):
            self.resolve(operation="sign", tag=self.tag, prerelease=True)

    def test_automatic_sign_requires_an_exact_planned_tag(self):
        for prerelease in (True, False):
            with self.assertRaises(ValueError):
                self.resolve(operation="sign", prerelease=prerelease)
        for tag in ("v1.1.9.5", "1.1.9.5-guffa.6", "v1.1.9.5-guffa.0", "v1.1.9.5-guffa.6\nrelease_tag=x",
                    "$(echo bad)", "../v1.1.9.5-guffa.6", "v1.1.9.5-guffa.6 "):
            with self.assertRaises(ValueError):
                self.resolve(tag=tag)

    def test_malformed_context_never_becomes_a_release(self):
        for key in ("run_number", "run_attempt"):
            for value in (None, "", 0, -1, True, "01", "1\nx=y"):
                original = copy.deepcopy(self.run)
                self.run[key] = value
                with self.assertRaises(ValueError):
                    self.resolve(tag=self.tag)
                self.run = original
        for prerelease in ("false", 1, None):
            with self.assertRaises(ValueError):
                self.resolve(tag=self.tag, prerelease=prerelease)
        with self.assertRaises(ValueError):
            context.resolve("sign-run", "", True, [])

    def test_cli_records_the_same_context_and_safe_workflow_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_file, result_file, outputs_file = root / "run.json", root / "context.json", root / "outputs"
            run_file.write_text(json.dumps(self.run), encoding="utf-8")
            result = subprocess.run([sys.executable, str(SCRIPT), "--operation", "sign-run",
                                     "--release-tag", self.tag, "--prerelease", "false",
                                     "--run-json", str(run_file), "--output", str(result_file),
                                     "--github-output", str(outputs_file)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result_file.read_text()), self.resolve(tag=self.tag, prerelease=False))
            outputs = dict(line.split("=", 1) for line in outputs_file.read_text().splitlines())
            self.assertEqual(outputs["build_channel"], "release")
            self.assertEqual(outputs["release_tag"], self.tag)
            self.assertEqual(outputs["run_number"], "987")
            self.assertEqual(outputs["run_attempt"], "2")

    def test_cli_optional_signed_test_reports_pre_without_a_named_tag(self):
        self.run.update(status="in_progress", conclusion=None)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_file, result_file, outputs_file = root / "run.json", root / "context.json", root / "outputs"
            run_file.write_text(json.dumps(self.run), encoding="utf-8")
            result = subprocess.run([sys.executable, str(SCRIPT), "--operation", "sign-run",
                                     "--prerelease", "true", "--run-json", str(run_file),
                                     "--output", str(result_file), "--github-output", str(outputs_file)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result_file.read_text()), self.resolve())
            outputs = dict(line.split("=", 1) for line in outputs_file.read_text().splitlines())
            self.assertEqual(outputs["build_channel"], "pre")
            self.assertEqual(outputs["release_tag"], "")
            self.assertEqual(outputs["prerelease"], "true")

    def test_workflows_verify_context_without_changing_the_trust_boundary(self):
        dispatch = (ROOT / ".github/workflows/validate-macos-signing.yaml").read_text()
        sign = (ROOT / ".github/workflows/sign-macos.yaml").read_text()
        self.assertIn("python3 scripts/mac-release-context.py", dispatch)
        self.assertIn("--mode \"$mode\"", dispatch)
        self.assertIn("release_tag: ${{ needs.resolve.outputs.release_tag }}", dispatch)
        self.assertIn("prerelease: ${{ fromJSON(needs.resolve.outputs.prerelease) }}", dispatch)
        self.assertIn("--expected-attempt \"$EXPECTED_ATTEMPT\" --expected-artifact \"$EXPECTED_ARTIFACT\"", sign)
        self.assertLess(sign.index("python3 scripts/mac-release-context.py"), sign.index("MACOS_CERTIFICATE_P12_BASE64:"))
        self.assertIn('if [[ "$mode" == release ]]; then', sign)
        self.assertIn('compare/$SOURCE_SHA...play', sign)

    def test_metadata_stamping_precedes_keys_and_covers_payload_provenance(self):
        script = (ROOT / "scripts/sign-macos-release.sh").read_text()
        stamp = script.index('python3 "$scripts/mac-build-identity.py" "${stamp_args[@]}"')
        self.assertLess(script.index("unsigned_library_hash="), stamp)
        self.assertLess(stamp, script.index('security create-keychain'))
        self.assertLess(stamp, script.index('codesign --force'))
        for binary in ("library", "loader", "launcher"):
            self.assertIn(f'== "$unsigned_{binary}_hash"', script)
        self.assertIn('--icons-dir "$scripts/../assets/launcher-icons"', script)
        self.assertIn('stamp_args=(stamp --app "$app" --channel "$BUILD_CHANNEL" --source "$embedded_source"', script)
        self.assertIn('if [[ -n "${RELEASE_TAG:-}" ]]; then stamp_args+=(--release-tag "$RELEASE_TAG"); fi', script)
        self.assertLess(script.index('python3 "$scripts/ci-signing-input.py" "${source_check[@]}"'), stamp)
        self.assertIn('sourceCommit:$producingSource,sourceRelationship:$relationship', script)
        self.assertNotIn('--release-tag "${RELEASE_TAG:-}"', script)
        self.assertEqual(script.count("launcherBuildIdentity:$identity[0]"), 2)
        self.assertLess(script.index('xcrun SetFile -a C "$mount"'), script.index('notarize "$output" dmg'))
        self.assertIn('== "$selected_icon_hash"', script)
        self.assertIn('.sha256 == $hash', script)
        self.assertIn('.cdhash == $hash', script)


if __name__ == "__main__":
    unittest.main()
