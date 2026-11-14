#!/usr/bin/env python3
"""Exercise signer shell boundaries without certificates, Apple calls or game binaries."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
SOURCE = (ROOT / 'scripts/sign-macos-release.sh').read_text(encoding='utf-8')
BASH = os.environ.get('STFC_TEST_BASH') or shutil.which('bash')
if not BASH and os.name == 'nt':
    BASH = 'C:/Program Files/Git/bin/bash.exe'


def section(start, end):
    first = SOURCE.index(start)
    return SOURCE[first:SOURCE.index(end, first)]


class SignerShellTests(unittest.TestCase):
    def run_shell(self, script, **values):
        self.assertTrue(BASH and Path(BASH).is_file(), 'A local Bash is required')
        with tempfile.TemporaryDirectory(prefix='stfc-signing-shell-') as folder:
            # All fixture files belong to this verified temporary directory.
            self.assertTrue(Path(folder).resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()))
            env = dict(os.environ, work=folder.replace('\\', '/'), RUNNER_TEMP=folder.replace('\\', '/'),
                       scripts='/trusted/scripts', app='/fixture/Test.app', keychain='/fixture/sign.keychain',
                       SOURCE_SHA='4' * 40, embedded_source='4' * 40, SOURCE_KIND='push-run-head',
                       GITHUB_REPOSITORY='Guffawaffle/stfc-mod', BUILD_CHANNEL='pre', BUILD_RUN_ID='123',
                       BUILD_RUN_NUMBER='9', BUILD_RUN_ATTEMPT='2', RELEASE_TAG='')
            env.update(values)
            return subprocess.run([BASH, '-c', 'set -euo pipefail\n' + script], env=env,
                                  capture_output=True, text=True, timeout=15)

    def cleanup_run(self, ending, *, keychain=False, security_status=0):
        body = section('cleanup() {', '\ntrap cleanup EXIT')
        setup = f'''mount=/fixture/mount
keychain_created={'true' if keychain else 'false'}
signing_complete=false
restore_keychains=(security list-keychains -d user -s)
hdiutil() {{ return 0; }}
security() {{ return {security_status}; }}
rm() {{ return 0; }}
'''
        return self.run_shell(setup + body + '\ntrap cleanup EXIT\n' + ending)

    def test_incomplete_zero_exit_is_failure(self):
        self.assertNotEqual(self.cleanup_run('exit 0').returncode, 0)

    def test_expansion_abort_cannot_be_success(self):
        self.assertNotEqual(self.cleanup_run('unset missing; : "$missing"').returncode, 0)

    def test_completed_package_preserves_success(self):
        self.assertEqual(self.cleanup_run('signing_complete=true').returncode, 0)

    def test_completed_package_preserves_primary_failure(self):
        self.assertEqual(self.cleanup_run('signing_complete=true; exit 23').returncode, 23)

    def test_keychain_cleanup_failure_cannot_be_success(self):
        self.assertNotEqual(self.cleanup_run('signing_complete=true', keychain=True, security_status=1).returncode, 0)

    def test_push_source_has_no_optional_commit_arguments(self):
        block = section('source_check=(', '\nsource_relationship=')
        result = self.run_shell('python3() { printf "%s\\n" "$@"; }\n' + block)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('--commit-json', result.stdout.splitlines())
        self.assertIn('verify-source', result.stdout.splitlines())

    def test_pr_source_passes_exact_commit_file(self):
        block = section('source_check=(', '\nsource_relationship=')
        result = self.run_shell('python3() { printf "%s\\n" "$@"; }; gh() { return 0; }\n' + block,
                                embedded_source='a' * 40, SOURCE_KIND='pull-request-run-head')
        self.assertEqual(result.returncode, 0, result.stderr)
        arguments = result.stdout.splitlines()
        self.assertIn('--commit-json', arguments)
        self.assertTrue(arguments[arguments.index('--commit-json') + 1].endswith('/producer-commit.json'))

    def test_unnamed_and_named_pre_keep_their_own_tag_arguments(self):
        block = section('stamp_args=(', '\n[[ "$(shasum')
        for tag in ('', 'v1.1.9.5-guffa.7'):
            with self.subTest(tag=tag):
                result = self.run_shell('python3() { printf "%s\\n" "$@"; }\n' + block, RELEASE_TAG=tag)
                self.assertEqual(result.returncode, 0, result.stderr)
                arguments = result.stdout.splitlines()
                self.assertEqual('--release-tag' in arguments, bool(tag))
                if tag:
                    self.assertEqual(arguments[arguments.index('--release-tag') + 1], tag)

    def test_failed_keychain_query_stops_before_mutation(self):
        block = section('keychain_list=$(', '\nprintf \'%s\' "$MACOS_CERTIFICATE')
        result = self.run_shell('security() { return 17; }\n' + block + '\nprintf "created\\n"')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('created', result.stdout)

    def test_empty_or_spaced_keychain_lists_preserve_argument_boundaries(self):
        block = section('keychain_list=$(', '\nprintf \'%s\' "$MACOS_CERTIFICATE')
        for output, count in (('', 5), ('    "/fixture/key chain.keychain-db"', 6)):
            with self.subTest(output=output):
                setup = '''restore_keychains=(security list-keychains -d user -s)
signing_keychains=(security list-keychains -d user -s "$keychain")
security() { printf '%s\\n' "$QUERY_OUTPUT"; }
'''
                result = self.run_shell(setup + block + '\nprintf "%s %s\\n" "${#restore_keychains[@]}" "${#signing_keychains[@]}"', QUERY_OUTPUT=output)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), f'{count} {count + 1}')


if __name__ == '__main__':
    unittest.main(verbosity=2)
