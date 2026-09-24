#!/usr/bin/env bash
# Read an existing submission. No signing identity or new upload is needed.
set -euo pipefail

id="${1:?Usage: check-macos-notarization.sh SUBMISSION_ID}"
[[ "$id" =~ ^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$ ]]
profile="${NOTARY_PROFILE:-stfc-notary}"
evidence="${NOTARY_EVIDENCE_DIR:-macos-notarization-status}"
mkdir -p "$evidence"
auth=(--keychain-profile "$profile")
if [[ -n "${NOTARY_KEYCHAIN:-}" ]]; then
  auth+=(--keychain "$NOTARY_KEYCHAIN")
fi

echo "Checking Apple submission $id at $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
xcrun notarytool history "${auth[@]}" --output-format json > "$evidence/history.json"
xcrun notarytool info "$id" "${auth[@]}" --output-format json > "$evidence/info.json"
cat "$evidence/info.json"
status=$(jq -er '.status' "$evidence/info.json")
case "$status" in
  Accepted|Invalid|Rejected)
    xcrun notarytool log "$id" "${auth[@]}" "$evidence/log.json"
    cat "$evidence/log.json"
    ;;
  'In Progress')
    echo 'Apple is still processing this submission; a completed analysis log is not available yet.'
    ;;
  *)
    echo "Unexpected Apple status: $status" >&2
    exit 1
    ;;
esac
echo 'This check does not submit, sign, staple, or publish software.'
