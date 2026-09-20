#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/settings-test
for fixture in boolean_settings boolean_view native_boolean_callback page_catalog page_sections choice_setting slider_setting shortcut_editor galaxy_selection_pins; do
  clang++ -std=c++23 -pthread -I mods/src -I third_party/libil2cpp \
    "tests/${fixture}_test.cc" -o "build/settings-test/$fixture"
  "build/settings-test/$fixture"
done

# The Mac candidate carries the fleet-arrival fixes; keep their regression
# fixture in the native-platform test run, with assertions enabled.
clang++ -std=c++23 -I mods/src tests/fleet_arrival_tracker.cc -o build/settings-test/fleet_arrival_tracker
build/settings-test/fleet_arrival_tracker
