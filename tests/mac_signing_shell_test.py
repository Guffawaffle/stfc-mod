#!/usr/bin/env python3
"""Exercise signer shell boundaries without certificates, Apple calls or game binaries."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parent.parent
SOURCE = (ROOT / 'scripts/sign-macos-release.sh').read_text(encoding='utf-8')
WORKFLOW = (ROOT / '.github/workflows/sign-macos.yaml').read_text(encoding='utf-8')
GIT_BASH = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Git/bin/bash.exe'
BASH = os.environ.get('STFC_TEST_BASH') or (
    str(GIT_BASH) if os.name == 'nt' and GIT_BASH.is_file() else shutil.which('bash'))


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
            return subprocess.run([BASH, '-c', 'set -euo pipefail\ncd "$work"\nwork="$PWD"\n' + script], env=env,
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

    def profiles_fixture(self, *, helper=True, notices=True, revision=None):
        pin = re.search(r'^pub const revision = "([0-9a-f]{40})";',
                        (ROOT / 'zig/profiles.zig').read_text(), re.MULTILINE).group(1)
        setup = '''app="$work/Test.app"
scripts="$work/trusted/scripts"
mkdir -p "$app/Contents/MacOS" "$scripts" "$scripts/../zig"
library="$app/Contents/libstfc-community-mod.dylib"
loader="$app/Contents/stfc-community-mod-loader"
launcher="$app/Contents/MacOS/macOSLauncher"
for binary in "$library" "$loader" "$launcher"; do printf 'fixture' > "$binary"; done
lipo() { printf 'architecture-check:%s\\n' "$1"; }
if ! command -v shasum >/dev/null; then shasum() { sha256sum "$3"; }; fi
'''
        setup += f'''printf 'pub const revision = "{pin}";\\n' > "$scripts/../zig/profiles.zig"
'''
        if helper:
            setup += '''printf '#!/bin/sh\\nexit 0\\n' > "$app/Contents/stfc-profiles"
chmod +x "$app/Contents/stfc-profiles"
'''
        if notices:
            setup += '''mkdir -p "$app/Contents/Resources/notices/profiles"
for notice in mod-LICENSE THIRD_PARTY.md profiles/LICENSE; do
  printf 'notice fixture' > "$app/Contents/Resources/notices/$notice"
done
'''
            setup += f'''printf 'Guffawaffle/stfc-profiles\\n{revision or pin}\\n' > "$app/Contents/Resources/notices/profiles/source.txt"
'''
        return setup

    def inspect_profiles(self, *, helper=True, notices=True, revision=None, tag='', extra=''):
        block = section('helper="$app/Contents/stfc-profiles"', '\nunsigned_library_hash=')
        return self.run_shell(self.profiles_fixture(helper=helper, notices=notices, revision=revision)
                              + extra + block + '\nprintf "profiles-enabled:%s\\n" "$profiles_enabled"',
                              RELEASE_TAG=tag)

    def test_complete_profiles_input_checks_helper_architectures(self):
        result = self.inspect_profiles(tag='v1.1.9.5-guffa.8')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.count('architecture-check:'), 4)
        self.assertIn('/Contents/stfc-profiles', result.stdout)
        self.assertIn('profiles-enabled:true', result.stdout)

    def test_legacy_ordinary_input_remains_an_untagged_test_only(self):
        result = self.inspect_profiles(helper=False, notices=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.count('architecture-check:'), 3)
        self.assertIn('profiles-enabled:false', result.stdout)
        self.assertNotEqual(self.inspect_profiles(helper=False, notices=False,
                                                 tag='v1.1.9.5-guffa.8').returncode, 0)

    def test_partial_wrong_pin_or_nonexecutable_profiles_input_is_rejected(self):
        cases = [{'helper': False}, {'notices': False}, {'revision': 'a' * 40},
                 {'extra': 'rm "$app/Contents/Resources/notices/profiles/LICENSE"\n'}]
        if os.name != 'nt':  # Windows has no POSIX executable mode bit.
            cases.append({'extra': 'chmod -x "$app/Contents/stfc-profiles"\n'})
        for values in cases:
            with self.subTest(values=values):
                self.assertNotEqual(self.inspect_profiles(**values).returncode, 0)

    def test_identity_stamping_cannot_change_the_helper(self):
        block = section('[[ "$(shasum -a 256 "$library"', '\nselected_icon_hash=')
        setup = self.profiles_fixture() + '''profiles_enabled=true
unsigned_library_hash=$(shasum -a 256 "$library" | awk '{print $1}')
unsigned_loader_hash=$(shasum -a 256 "$loader" | awk '{print $1}')
unsigned_launcher_hash=$(shasum -a 256 "$launcher" | awk '{print $1}')
helper="$app/Contents/stfc-profiles"
unsigned_helper_hash=$(shasum -a 256 "$helper" | awk '{print $1}')
'''
        self.assertEqual(self.run_shell(setup + block).returncode, 0)
        self.assertNotEqual(self.run_shell(setup + 'printf changed >> "$helper"\n' + block).returncode, 0)

    def test_helper_is_explicitly_signed_and_verified_on_both_architectures(self):
        block = section('# Sign nested code explicitly', '\nnotarize() {')
        setup = self.profiles_fixture() + '''profiles_enabled=true
helper="$app/Contents/stfc-profiles"
APPLE_TEAM_ID=TEAM123456
MACOS_SIGNING_IDENTITY=fixture-identity
codesign() {
  printf '%s\\n' "$*" >> "$work/codesign-calls"
  if [[ "$1" == --display ]]; then
    printf 'TeamIdentifier=TEAM123456\\nAuthority=Developer ID Application: Fixture\\nTimestamp=fixture\\nflags=0x10000(runtime)\\n'
  else printf 'codesign:%s\\n' "$*"; fi
}
'''
        result = self.run_shell(setup + block + '\ncat "$work/codesign-calls"')
        self.assertEqual(result.returncode, 0, result.stderr)
        helper_calls = [line for line in result.stdout.splitlines()
                        if line.endswith('/Contents/stfc-profiles') and line.startswith('codesign:')]
        self.assertEqual(len(helper_calls), 2)  # Explicit sign plus strict verification.
        displays = [line for line in result.stdout.splitlines()
                    if line.endswith('/Contents/stfc-profiles') and line.startswith('--display')]
        self.assertEqual(len(displays), 2)
        self.assertTrue(any('--arch arm64' in line for line in displays))
        self.assertTrue(any('--arch x86_64' in line for line in displays))

    def test_helper_requires_ticket_coverage_for_both_slices(self):
        start = SOURCE.rindex('for binary in "${signed_binaries[@]}"; do')
        block = SOURCE[start:SOURCE.index('\nxcrun stapler staple', start)]
        setup = '''work=/fixture
output=signed-macos/stfc-community-mod-installer.dmg
signed_binaries=(/fixture/dmg-root/Test.app /fixture/dmg-root/Test.app/Contents/stfc-profiles)
verify_ticket() { printf '%s|%s|%s\\n' "$1" "$2" "$3"; }
'''
        result = self.run_shell(setup + block)
        self.assertEqual(result.returncode, 0, result.stderr)
        checks = [line for line in result.stdout.splitlines() if '/Contents/stfc-profiles|' in line]
        self.assertEqual(len(checks), 2)
        self.assertTrue(any(line.endswith('|arm64') for line in checks))
        self.assertTrue(any(line.endswith('|x86_64') for line in checks))

    def test_signed_helper_hash_and_notices_are_checked_after_dmg_copy(self):
        block = section('if [[ "$profiles_enabled" == true ]]; then\n  [[ "$(shasum -a 256 "$installed_app',
                        '\n# The standalone archive')
        setup = self.profiles_fixture() + '''profiles_enabled=true
installed_app="$work/Installed.app"
cp -R "$app" "$installed_app"
signed_helper_hash=$(shasum -a 256 "$app/Contents/stfc-profiles" | awk '{print $1}')
'''
        self.assertEqual(self.run_shell(setup + block).returncode, 0)
        changed = setup + 'printf changed >> "$installed_app/Contents/stfc-profiles"\n'
        self.assertNotEqual(self.run_shell(changed + block).returncode, 0)
        changed = setup + 'printf changed >> "$installed_app/Contents/Resources/notices/profiles/source.txt"\n'
        self.assertNotEqual(self.run_shell(changed + block).returncode, 0)

    def test_standalone_archive_keeps_helper_adjacent_and_notices_at_root(self):
        block = section('archive=signed-macos/', '\nshasum -a 256 "$archive"')
        for enabled in ('true', 'false'):
            with self.subTest(enabled=enabled):
                setup = self.profiles_fixture() + f'''profiles_enabled={enabled}
installed_app="$app"
zstd() {{ cat > "$work/archive.tar"; }}
'''
                result = self.run_shell(setup + block + '\ntar -tf "$work/archive.tar"')
                self.assertEqual(result.returncode, 0, result.stderr)
                files = result.stdout.splitlines()
                self.assertIn('libstfc-community-mod.dylib', files)
                self.assertEqual('stfc-profiles' in files, enabled == 'true')
                if enabled == 'true':
                    self.assertIn('notices/profiles/source.txt', files)
                    self.assertFalse(any(name.startswith('Resources/') for name in files))

    def package_gate(self, *, missing='', empty='', provenance='{"dmgNotarization":{"status":"Accepted"}}'):
        block = WORKFLOW.split('      - name: Require the complete notarized package\n', 1)[1]
        block = block.split('      - name: Upload signed macOS artifact\n', 1)[0]
        gate = textwrap.dedent(block.split('        run: |\n', 1)[1])
        # Execute the actual workflow block against isolated synthetic files.
        setup = '''cd "$work"
mkdir signed-macos
for file in stfc-community-mod-installer.dmg stfc-community-mod-macos-universal.tar.zst \\
  stfc-community-mod-macos-universal.tar.zst.sha256 macos-provenance.json ci-input.json \\
  mac-build-identity.json mac-source-identity.json; do
  if [[ "$file" != "$MISSING" ]]; then
    if [[ "$file" == "$EMPTY" ]]; then : > "signed-macos/$file"
    elif [[ "$file" == macos-provenance.json ]]; then printf '%s' "$PROVENANCE" > "signed-macos/$file"
    else printf 'fixture' > "signed-macos/$file"; fi
  fi
done
'''
        return self.run_shell(setup + gate, MISSING=missing, EMPTY=empty, PROVENANCE=provenance)

    def test_complete_accepted_package_passes_upload_gate(self):
        result = self.package_gate()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_missing_or_empty_package_member_blocks_upload(self):
        files = ('stfc-community-mod-installer.dmg', 'stfc-community-mod-macos-universal.tar.zst',
                 'stfc-community-mod-macos-universal.tar.zst.sha256', 'macos-provenance.json',
                 'ci-input.json', 'mac-build-identity.json', 'mac-source-identity.json')
        for file in files:
            for field in ('missing', 'empty'):
                with self.subTest(file=file, field=field):
                    self.assertNotEqual(self.package_gate(**{field: file}).returncode, 0)

    def test_missing_rejected_or_invalid_notarization_blocks_upload(self):
        for provenance in ('{}', '{"dmgNotarization":{"status":"Invalid"}}',
                           '{"dmgNotarization":{"status":"In Progress"}}', 'invalid json'):
            with self.subTest(provenance=provenance):
                self.assertNotEqual(self.package_gate(provenance=provenance).returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
