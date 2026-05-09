#pragma once

#include "patches/input_binding/input_binding.h"

#include <toml++/toml.h>

#include <span>
#include <cstdint>
#include <string>
#include <vector>

namespace input_binding
{
enum class BindingConfigSourceKind : uint8_t {
  Default = 0,
  Canonical,
  LegacyAlias,
  DeprecatedAlias,
};

struct ResolvedBinding {
  InputActionId            action = InputActionId::Max;
  std::string              canonical_key;
  std::string              binding;
  std::string              source_key;
  BindingConfigSourceKind  source_kind = BindingConfigSourceKind::Default;
};

struct ConfigBridgeResult {
  std::vector<ResolvedBinding> bindings;
  std::vector<std::string>     compatibility_warnings;

  [[nodiscard]] std::vector<BindingOverride> AsOverrides() const;
};

struct BindingConfigAlias {
  std::string_view         key;
  BindingConfigSourceKind  source_kind = BindingConfigSourceKind::LegacyAlias;
  std::string_view         deprecation_warning;
};

[[nodiscard]] ConfigBridgeResult ResolveInputBindingConfig(const toml::table& config);
[[nodiscard]] toml::table BuildInputBindingRuntimeConfig(const ConfigBridgeResult& bridge, const CompileResult& compile);
[[nodiscard]] std::span<const BindingConfigAlias> ShortcutConfigAliases(InputActionId action);

} // namespace input_binding