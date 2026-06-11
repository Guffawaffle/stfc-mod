#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
  std::filesystem::path find_repo_file(const std::string_view relative_path)
  {
    auto current = std::filesystem::current_path();
    while (!current.empty()) {
      const auto candidate = current / std::filesystem::path(relative_path);
      if (std::filesystem::exists(candidate)) {
        return candidate;
      }

      if (!current.has_parent_path()) {
        break;
      }

      const auto parent = current.parent_path();
      if (parent == current) {
        break;
      }

      current = parent;
    }

    return {};
  }

  std::string read_text_file(const std::string_view relative_path)
  {
    const auto path = find_repo_file(relative_path);
    REQUIRE_MESSAGE(!path.empty(), "Failed to find " << std::string(relative_path));

    std::ifstream input(path, std::ios::binary);
    REQUIRE_MESSAGE(input.good(), "Failed to open " << path.string());

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
  }

  bool contains(const std::string& text, const std::string_view needle)
  { return text.find(needle) != std::string::npos; }
}

TEST_CASE("generated native shortcut pointer callback guard family stays quarantined")
{
  const auto hotkeys_source = read_text_file("mods/src/patches/parts/hotkeys.cc");

  CHECK_FALSE(contains(hotkeys_source, "SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARDS"));
  CHECK_FALSE(contains(hotkeys_source, "OnShipLocateAction"));

  const auto has_generated_callback_guard_install =
      contains(hotkeys_source, "INSTALL_SHORTCUTS_MANAGER_POINTER_CALLBACK_GUARD")
      || contains(hotkeys_source, "HandleNativeShortcutPointerCallback")
      || contains(hotkeys_source, "should_install_native_shortcut_pointer_callback_guards")
      || contains(hotkeys_source, "native_shortcut_pointer_callback_guard_reason")
      || contains(hotkeys_source, "hotkey_router_should_suppress_native_shortcut_callback");

    CHECK_FALSE(has_generated_callback_guard_install);
}

TEST_CASE("fleet runtime sync requests require gameplay dispatch provenance")
{
  const auto sync_header = read_text_file("mods/src/patches/fleet_runtime_sync.h");
  CHECK_FALSE(contains(sync_header, "fleet_runtime_sync_trigger(std::string_view"));
  CHECK_FALSE(contains(sync_header, "fleet_runtime_sync_capture(std::string_view"));

  const auto diagnostics_header = read_text_file("mods/src/patches/fleet_runtime_diagnostics.h");
  CHECK_FALSE(contains(diagnostics_header, "fleet_runtime_diagnostics_trigger(std::string_view"));
  CHECK_FALSE(contains(diagnostics_header, "fleet_runtime_diagnostics_capture_attempt(std::string_view"));
  CHECK_FALSE(contains(diagnostics_header, "fleet_runtime_diagnostics_suppressed_unchanged(std::string_view"));
  CHECK_FALSE(contains(diagnostics_header, "fleet_runtime_diagnostics_suppressed_non_meaningful(std::string_view"));
  CHECK_FALSE(contains(diagnostics_header, "fleet_runtime_diagnostics_make_trace(\n    std::string_view source"));

  const auto live_debug_source = read_text_file("mods/src/patches/parts/live_debug.cc");
  CHECK_FALSE(contains(live_debug_source, "fleet_runtime_sync_trigger(\""));
}
