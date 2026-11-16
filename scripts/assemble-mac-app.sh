#!/bin/bash
# Assembles and signs the launcher bundle from Zig install prefixes, one per architecture. Each prefix holds
# macOSLauncher.app, bin/stfc-community-mod-loader and lib/libstfc-community-mod.dylib. Profiles previews also
# hold bin/stfc-profiles, which must remain beside the bundled dylib for isolated browser sign-in. With several prefixes the
# binaries are merged into universal ones; the first prefix supplies the bundle's resources.
#
# Usage: scripts/assemble-mac-app.sh <output .app> <prefix>...
# CI, package-mac-app.sh and mac-build-test-debug.sh all package through this script.
set -euo pipefail

if (($# < 2)); then
  echo "usage: $0 <output .app> <prefix>..." >&2
  exit 1
fi
APP="$1"
shift
PREFIXES=("$@")
SCRIPTS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILES=0
for prefix in "${PREFIXES[@]}"; do
  if [[ -f "$prefix/bin/stfc-profiles" ]]; then PROFILES=$((PROFILES + 1)); fi
done
if ((PROFILES != 0 && PROFILES != ${#PREFIXES[@]})); then
  echo "Cannot mix ordinary and Profiles-enabled launcher builds" >&2
  exit 1
fi
if ((PROFILES)); then
  for prefix in "${PREFIXES[@]}"; do
    if [[ ! -f "$prefix/share/stfc-profiles/source.txt" || ! -f "$prefix/share/stfc-profiles/LICENSE" ]]; then
      echo "Missing Profiles source receipt or license in $prefix" >&2
      exit 1
    fi
    cmp "${PREFIXES[0]}/share/stfc-profiles/source.txt" "$prefix/share/stfc-profiles/source.txt"
    cmp "${PREFIXES[0]}/share/stfc-profiles/LICENSE" "$prefix/share/stfc-profiles/LICENSE"
  done
fi

# merge <path inside each prefix> <destination>
merge() {
  local inputs=() prefix
  for prefix in "${PREFIXES[@]}"; do
    inputs+=("$prefix/$1")
  done
  lipo -create "${inputs[@]}" -output "$2"
}

rm -rf "$APP"
ditto "${PREFIXES[0]}/macOSLauncher.app" "$APP"
merge macOSLauncher.app/Contents/MacOS/macOSLauncher "$APP/Contents/MacOS/macOSLauncher"
merge bin/stfc-community-mod-loader "$APP/Contents/stfc-community-mod-loader"
merge lib/libstfc-community-mod.dylib "$APP/Contents/libstfc-community-mod.dylib"
if ((PROFILES)); then
  merge bin/stfc-profiles "$APP/Contents/stfc-profiles"
  mkdir -p "$APP/Contents/Resources/notices"
  ditto "${PREFIXES[0]}/share/stfc-profiles" "$APP/Contents/Resources/notices/profiles"
  cp "$SCRIPTS/../LICENSE" "$APP/Contents/Resources/notices/mod-LICENSE"
  cp "$SCRIPTS/../docs/THIRD_PARTY.md" "$APP/Contents/Resources/notices/THIRD_PARTY.md"
fi

for file in Info.plist Resources/Assets.car Resources/launcher.icns MacOS/macOSLauncher \
  stfc-community-mod-loader libstfc-community-mod.dylib; do
  if [[ ! -f "$APP/Contents/$file" ]]; then
    echo "$APP is missing Contents/$file" >&2
    exit 1
  fi
done

"$SCRIPTS/check-mac-linkage.sh" "$APP/Contents/MacOS/macOSLauncher" "$APP/Contents/stfc-community-mod-loader" \
  "$APP/Contents/libstfc-community-mod.dylib"
if ((PROFILES)); then
  "$SCRIPTS/check-mac-linkage.sh" "$APP/Contents/stfc-profiles"
fi

# Ad hoc: the bundle is not notarized, and the launcher re-signs the game itself when it has to.
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
echo "Assembled $APP"
