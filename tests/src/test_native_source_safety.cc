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
  const auto has_explicit_promotion_marker =
      contains(hotkeys_source, "NATIVE_SHORTCUT_CALLBACK_GUARD_ALLOWLIST_PROMOTED");
  const auto has_unpromoted_generated_callback_guard_install =
      has_generated_callback_guard_install && !has_explicit_promotion_marker;

  CHECK_FALSE(has_unpromoted_generated_callback_guard_install);
}