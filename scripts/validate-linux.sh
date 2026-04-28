#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${STFC_MOD_LINUX_BUILD_DIR:-"${repo_root}/build/linux-validation"}"
command_name="${1:-pure-tests}"

usage() {
  cat <<'USAGE'
Usage: scripts/validate-linux.sh [pure-tests|decode-tool|all|build]

Commands:
  pure-tests   Configure, build, and run the pure C++ test suite.
  decode-tool  Configure and build the battle-log decode CLI helper.
  all          Run the currently available Linux-native validation set.
  build        Reserved for full mod build parity; not implemented here yet.

This wrapper is intentionally limited to code that can be validated on Linux
without PowerShell, xmake, IL2CPP runtime headers beyond the checked-in stubs,
or game/runtime libraries.
USAGE
}

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required tool: $1" >&2
    exit 127
  fi
}

configure() {
  require_tool cmake
  cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
}

build_targets() {
  cmake --build "$build_dir" --parallel --target "$@"
}

run_pure_tests() {
  configure
  build_targets stfc-mod-tests battle-log-decode
  ctest --test-dir "$build_dir" --output-on-failure
}

build_decode_tool() {
  configure
  build_targets battle-log-decode
}

case "$command_name" in
  pure-tests)
    run_pure_tests
    ;;
  decode-tool)
    build_decode_tool
    ;;
  all)
    run_pure_tests
    ;;
  build)
    echo "Full Linux mod build parity is not implemented in this wrapper." >&2
    echo "Current Linux-native coverage is: scripts/validate-linux.sh pure-tests" >&2
    exit 2
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    usage >&2
    exit 64
    ;;
esac
