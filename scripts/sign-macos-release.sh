#!/usr/bin/env bash
# Re-sign an exact, successful play CI installer. Never rebuild with credentials.
set -euo pipefail
umask 077

: "${RUNNER_TEMP:?}" "${MACOS_CERTIFICATE_P12_BASE64:?}" "${MACOS_CERTIFICATE_PASSWORD:?}"
: "${APPLE_APP_SPECIFIC_PASSWORD:?}" "${APPLE_ID:?}" "${APPLE_TEAM_ID:?}"
: "${MACOS_SIGNING_IDENTITY:?}" "${SOURCE_SHA:?}" "${BUILD_RUN_ID:?}"
[[ "$SOURCE_SHA" =~ ^[0-9a-f]{40}$ && "$BUILD_RUN_ID" =~ ^[1-9][0-9]*$ ]]
[[ "$APPLE_TEAM_ID" =~ ^[A-Z0-9]{10}$ && "$MACOS_SIGNING_IDENTITY" =~ ^[A-Fa-f0-9]{40}$ ]]
command -v zstd >/dev/null

work=$(mktemp -d "$RUNNER_TEMP/stfc-macos.XXXXXX")
keychain="$RUNNER_TEMP/stfc-signing.keychain-db"
mount="$work/mount"
mkdir -p "$mount" signed-macos macos-notarization-evidence
cleanup() {
  hdiutil detach "$mount" -quiet 2>/dev/null || true
  security delete-keychain "$keychain" 2>/dev/null || true
  # mktemp above owns this directory; it contains no user files.
  rm -rf "$work"
}
trap cleanup EXIT

printf '%s' "$MACOS_CERTIFICATE_P12_BASE64" | base64 --decode > "$work/identity.p12"
keychain_password=$(openssl rand -hex 32)
security create-keychain -p "$keychain_password" "$keychain"
security set-keychain-settings -lut 3600 "$keychain"
security unlock-keychain -p "$keychain_password" "$keychain"
security import "$work/identity.p12" -k "$keychain" -P "$MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain" >/dev/null
security find-identity -v -p codesigning "$keychain" | grep -F "$MACOS_SIGNING_IDENTITY" >/dev/null
xcrun notarytool store-credentials stfc-notary --keychain "$keychain" \
  --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD"
rm "$work/identity.p12"
unset MACOS_CERTIFICATE_P12_BASE64 MACOS_CERTIFICATE_PASSWORD APPLE_APP_SPECIFIC_PASSWORD keychain_password

input=unsigned-macos/stfc-community-mod-installer.dmg
input_hash=$(shasum -a 256 "$input" | awk '{print $1}')
hdiutil attach "$input" -readonly -nobrowse -mountpoint "$mount" -quiet
# Preserve the existing installer artwork, layout and Applications link.
ditto "$mount" "$work/dmg-root"
hdiutil detach "$mount" -quiet
app="$work/dmg-root/STFC Community Mod.app"
loader="$app/Contents/stfc-community-mod-loader"
library="$app/Contents/libstfc-community-mod.dylib"
launcher="$app/Contents/MacOS/macOSLauncher"
for binary in "$library" "$loader" "$launcher"; do
  test -f "$binary"
  lipo "$binary" -verify_arch arm64 x86_64
done
unsigned_library_hash=$(shasum -a 256 "$library" | awk '{print $1}')

# Sign nested code explicitly before sealing the outer bundle. The library runs
# inside the game's process and uses the host's entitlements, not the launcher's.
codesign --force --timestamp --options runtime --keychain "$keychain" --sign "$MACOS_SIGNING_IDENTITY" "$library"
codesign --force --timestamp --options runtime --keychain "$keychain" --sign "$MACOS_SIGNING_IDENTITY" "$loader"
codesign --force --timestamp --options runtime --keychain "$keychain" --sign "$MACOS_SIGNING_IDENTITY" \
  --entitlements macos-launcher/src/macOSLauncher.entitlements "$app"
for binary in "$library" "$loader" "$app"; do
  codesign --verify --strict --all-architectures --verbose=2 "$binary"
  for arch in arm64 x86_64; do
    details=$(codesign --display --verbose=4 --arch "$arch" "$binary" 2>&1)
    grep -Fx "TeamIdentifier=$APPLE_TEAM_ID" <<< "$details" >/dev/null
    grep -F 'Authority=Developer ID Application:' <<< "$details" >/dev/null
    grep -E '^Timestamp=.+$' <<< "$details" >/dev/null
    grep -F '(runtime)' <<< "$details" >/dev/null
  done
done
codesign --verify --deep --strict --all-architectures "$app"

notarize() {
  local file="$1" name="$2" result id status
  result="macos-notarization-evidence/$name-submission.json"
  # A timeout leaves the submission ID in evidence; it never publishes an
  # unaccepted artifact or silently submits the same payload again.
  local submit_exit=0
  xcrun notarytool submit "$file" --keychain-profile stfc-notary --keychain "$keychain" \
    --wait --timeout 20m --output-format json > "$result" || submit_exit=$?
  id=$(jq -r '.id // empty' "$result")
  if [[ -n "$id" ]]; then
    xcrun notarytool log "$id" --keychain-profile stfc-notary --keychain "$keychain" \
      "macos-notarization-evidence/$name-log.json" || true
  fi
  status=$(jq -r '.status // empty' "$result")
  if [[ "$submit_exit" != 0 || "$status" != Accepted ]]; then
    echo "::error::Notarization $name status: $status; submission: $id. See evidence artifact."
    return 1
  fi
}

ditto -c -k --keepParent "$app" "$work/app.zip"
notarize "$work/app.zip" app
xcrun stapler staple "$app"
xcrun stapler validate "$app"
spctl --assess --type execute --verbose=2 "$app"

output=signed-macos/stfc-community-mod-installer.dmg
hdiutil create -quiet -volname 'STFC Community Mod Installer' -srcfolder "$work/dmg-root" -format UDZO "$output"
codesign --timestamp --keychain "$keychain" --sign "$MACOS_SIGNING_IDENTITY" "$output"
notarize "$output" dmg
xcrun stapler staple "$output"
xcrun stapler validate "$output"
codesign --verify --strict "$output"
spctl --assess --type open --context context:primary-signature --verbose=2 "$output"

# The standalone archive contains the exact library accepted in the app
# submission. Dylibs cannot carry a stapled ticket; Gatekeeper can look it up.
archive=signed-macos/stfc-community-mod-macos-universal.tar.zst
tar -cf - -C "$app/Contents" libstfc-community-mod.dylib | zstd -15 -T0 -o "$archive"
shasum -a 256 "$archive" | awk '{print $1}' > "$archive.sha256"
signed_library_hash=$(shasum -a 256 "$library" | awk '{print $1}')
dmg_hash=$(shasum -a 256 "$output" | awk '{print $1}')
jq -n --arg source "$SOURCE_SHA" --arg build "$BUILD_RUN_ID" --arg team "$APPLE_TEAM_ID" \
  --arg signer "$MACOS_SIGNING_IDENTITY" --arg input "$input_hash" \
  --arg unsigned "$unsigned_library_hash" --arg signed "$signed_library_hash" --arg dmg "$dmg_hash" \
  --arg workflow "$GITHUB_SHA" --arg url "$GITHUB_SERVER_URL/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID" \
  --slurpfile app macos-notarization-evidence/app-submission.json \
  --slurpfile dmgTicket macos-notarization-evidence/dmg-submission.json \
  '{sourceCommit:$source,buildRunId:$build,teamId:$team,signingIdentity:$signer,
    inputDmgSha256:$input,unsignedLibrarySha256:$unsigned,signedLibrarySha256:$signed,
    dmgSha256:$dmg,signingWorkflowCommit:$workflow,signingRunUrl:$url,
    appNotarization:$app[0],dmgNotarization:$dmgTicket[0]}' > signed-macos/macos-provenance.json
