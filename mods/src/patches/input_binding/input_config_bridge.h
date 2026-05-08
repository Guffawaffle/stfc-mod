#pragma once

#include "patches/input_binding/input_binding.h"

#include <toml++/toml.h>

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

[[nodiscard]] ConfigBridgeResult ResolveInputBindingConfig(const toml::table& config);
[[nodiscard]] toml::table BuildInputBindingRuntimeConfig(const ConfigBridgeResult& bridge, const CompileResult& compile);

} // namespace input_binding