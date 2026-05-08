#include "patches/input_binding/input_config_bridge.h"

#include "str_utils_pure.h"

#include <optional>
#include <sstream>
#include <utility>

namespace input_binding
{
namespace {
struct BindingCandidate {
  std::string             binding;
  std::string             source_key;
  BindingConfigSourceKind source_kind = BindingConfigSourceKind::Default;
};

void add_warning(ConfigBridgeResult& result, std::string message)
{ result.compatibility_warnings.push_back(std::move(message)); }

std::optional<std::string> normalize_binding_string(const std::string_view value)
{
  const auto trimmed = StripAsciiWhitespace(value);
  if (trimmed.empty()) {
    return std::nullopt;
  }
  return AsciiStrToUpper(trimmed);
}

const toml::table* input_bindings_table(const toml::table& config)
{
  const auto* input = config["input"].as_table();
  return input ? (*input)["bindings"].as_table() : nullptr;
}

const toml::table* shortcuts_table(const toml::table& config)
{ return config["shortcuts"].as_table(); }

std::optional<std::string> read_binding_value(const toml::node_view<const toml::node> node, const std::string& source_key,
                                              ConfigBridgeResult& result)
{
  if (const auto value = node.value<std::string>()) {
    auto normalized = normalize_binding_string(*value);
    if (!normalized) {
      add_warning(result, source_key + " is empty after trimming; ignoring configured value.");
    }
    return normalized;
  }

  if (const auto* array = node.as_array()) {
    std::string joined;
    for (size_t index = 0; index < array->size(); ++index) {
      const auto item = (*array)[index].value<std::string>();
      if (!item) {
        add_warning(result, source_key + "[" + std::to_string(index) + "] must be a string; ignoring item.");
        continue;
      }

      auto normalized = normalize_binding_string(*item);
      if (!normalized) {
        add_warning(result, source_key + "[" + std::to_string(index) + "] is empty after trimming; ignoring item.");
        continue;
      }

      if (!joined.empty()) {
        joined.append("|");
      }
      joined.append(*normalized);
    }

    if (joined.empty()) {
      add_warning(result, source_key + " has no valid string items; ignoring configured value.");
      return std::nullopt;
    }

    return joined;
  }

  add_warning(result, source_key + " must be a string or array of strings; ignoring configured value.");
  return std::nullopt;
}

std::optional<BindingCandidate> read_candidate(const toml::table* table,
                                               const std::string_view key,
                                               const std::string_view source_prefix,
                                               const BindingConfigSourceKind source_kind,
                                               ConfigBridgeResult& result,
                                               const std::string_view deprecation_warning = {})
{
  if (!table || !table->contains(key)) {
    return std::nullopt;
  }

  const std::string source_key = std::string(source_prefix) + std::string(key);
  auto              binding    = read_binding_value((*table)[key], source_key, result);
  if (!binding) {
    return std::nullopt;
  }

  if (!deprecation_warning.empty()) {
    add_warning(result, std::string(deprecation_warning));
  }

  return BindingCandidate{std::move(*binding), source_key, source_kind};
}

BindingCandidate default_candidate(const InputActionSpec& spec)
{
  return BindingCandidate{std::string(spec.default_bind), "default", BindingConfigSourceKind::Default};
}

void warn_conflict(const InputActionSpec& spec,
                   const BindingCandidate& chosen,
                   const BindingCandidate& ignored,
                   ConfigBridgeResult& result)
{
  std::ostringstream message;
  message << "Conflicting bindings for [input.bindings]." << spec.canonical_key << ": using " << chosen.source_key
          << "='" << chosen.binding << "', ignoring " << ignored.source_key << "='" << ignored.binding << "'.";
  add_warning(result, message.str());
}

ResolvedBinding resolve_binding(const InputActionSpec& spec,
                                const std::vector<std::optional<BindingCandidate>>& candidates,
                                ConfigBridgeResult& result)
{
  auto chosen        = default_candidate(spec);
  bool saw_explicit  = false;

  for (const auto& candidate : candidates) {
    if (!candidate) {
      continue;
    }

    if (!saw_explicit) {
      chosen = *candidate;
      saw_explicit = true;
      continue;
    }

    if (candidate->binding != chosen.binding) {
      warn_conflict(spec, chosen, *candidate, result);
    }
  }

  return ResolvedBinding{spec.id, std::string(spec.canonical_key), std::move(chosen.binding),
                         std::move(chosen.source_key), chosen.source_kind};
}

std::optional<BindingCandidate> resolve_disable_hotkeys_shortcut(const toml::table& config, ConfigBridgeResult& result)
{
  const auto* shortcuts = shortcuts_table(config);
  auto canonical = read_candidate(shortcuts, "set_hotkeys_disable", "[shortcuts].", BindingConfigSourceKind::LegacyAlias,
                                  result);
  auto typo = read_candidate(shortcuts, "set_hotkeys_disble", "[shortcuts].",
                             BindingConfigSourceKind::DeprecatedAlias, result,
                             "[shortcuts].set_hotkeys_disble is deprecated; prefer [input.bindings].hotkeys_disable or "
                             "[shortcuts].set_hotkeys_disable.");
  auto legacy = read_candidate(shortcuts, "set_hotkeys_disabled", "[shortcuts].",
                               BindingConfigSourceKind::DeprecatedAlias, result,
                               "[shortcuts].set_hotkeys_disabled is deprecated; prefer [input.bindings].hotkeys_disable or "
                               "[shortcuts].set_hotkeys_disable.");

  const auto* spec = FindActionSpec(InputActionId::HotkeysDisable);
  if (!spec) {
    return std::nullopt;
  }

  if (canonical) {
    if (typo && typo->binding != canonical->binding) {
      warn_conflict(*spec, *canonical, *typo, result);
    }
    if (legacy && legacy->binding != canonical->binding) {
      warn_conflict(*spec, *canonical, *legacy, result);
    }
    return canonical;
  }

  if (typo) {
    if (legacy && legacy->binding != typo->binding) {
      warn_conflict(*spec, *typo, *legacy, result);
    }
    return typo;
  }

  return legacy;
}

std::optional<BindingCandidate> resolve_enable_hotkeys_shortcut(const toml::table& config, ConfigBridgeResult& result)
{
  const auto* shortcuts = shortcuts_table(config);
  auto canonical = read_candidate(shortcuts, "set_hotkeys_enable", "[shortcuts].", BindingConfigSourceKind::LegacyAlias,
                                  result);
  auto legacy = read_candidate(shortcuts, "set_hotkeys_enabled", "[shortcuts].",
                               BindingConfigSourceKind::DeprecatedAlias, result,
                               "[shortcuts].set_hotkeys_enabled is accepted for compatibility; prefer "
                               "[input.bindings].hotkeys_enable or [shortcuts].set_hotkeys_enable.");

  const auto* spec = FindActionSpec(InputActionId::HotkeysEnable);
  if (!spec) {
    return std::nullopt;
  }

  if (canonical) {
    if (legacy && legacy->binding != canonical->binding) {
      warn_conflict(*spec, *canonical, *legacy, result);
    }
    return canonical;
  }

  return legacy;
}

std::string format_binding_diagnostic(const BindingDiagnostic& diagnostic)
{
  const auto* spec = FindActionSpec(diagnostic.action);
  std::ostringstream message;
  message << (spec ? spec->canonical_key : "unknown") << ": " << diagnostic.message;
  return message.str();
}

std::string format_binding_conflict(const BindingConflict& conflict)
{
  const auto* action_a = FindActionSpec(conflict.action_a);
  const auto* action_b = FindActionSpec(conflict.action_b);

  std::ostringstream message;
  message << (action_a ? action_a->canonical_key : "unknown") << " conflicts with "
          << (action_b ? action_b->canonical_key : "unknown") << " on '" << conflict.chord.display << "'.";
  return message.str();
}
} // namespace

std::vector<BindingOverride> ConfigBridgeResult::AsOverrides() const
{
  std::vector<BindingOverride> overrides;
  overrides.reserve(bindings.size());
  for (const auto& binding : bindings) {
    overrides.push_back({binding.action, binding.binding});
  }
  return overrides;
}

ConfigBridgeResult ResolveInputBindingConfig(const toml::table& config)
{
  ConfigBridgeResult result;
  result.bindings.reserve(ActionSpecs().size());

  const auto* input_bindings = input_bindings_table(config);
  const auto* shortcuts      = shortcuts_table(config);

  for (const auto& spec : ActionSpecs()) {
    std::vector<std::optional<BindingCandidate>> candidates;
    candidates.reserve(4);
    candidates.push_back(read_candidate(input_bindings, spec.canonical_key, "[input.bindings].",
                                        BindingConfigSourceKind::Canonical, result));

    switch (spec.id) {
      case InputActionId::FleetPrimary:
        candidates.push_back(read_candidate(shortcuts, "action_primary", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        candidates.push_back(read_candidate(shortcuts, "action_queue", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        candidates.push_back(read_candidate(shortcuts, "action_recall_cancel", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetSecondary:
        candidates.push_back(read_candidate(shortcuts, "action_secondary", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetService:
        candidates.push_back(read_candidate(shortcuts, "action_recall", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        candidates.push_back(read_candidate(shortcuts, "action_repair", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetViewInfo:
        candidates.push_back(read_candidate(shortcuts, "action_view", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetQueueClear:
        candidates.push_back(read_candidate(shortcuts, "action_queue_clear", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetQueueToggle:
        candidates.push_back(read_candidate(shortcuts, "toggle_queue", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::HotkeysDisable:
        candidates.push_back(resolve_disable_hotkeys_shortcut(config, result));
        break;
      case InputActionId::HotkeysEnable:
        candidates.push_back(resolve_enable_hotkeys_shortcut(config, result));
        break;
      case InputActionId::LogDebug:
        candidates.push_back(read_candidate(shortcuts, "log_debug", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ZoomIn:
        candidates.push_back(read_candidate(shortcuts, "zoom_in", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ZoomOut:
        candidates.push_back(read_candidate(shortcuts, "zoom_out", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::Max:
        break;
    }

    result.bindings.push_back(resolve_binding(spec, candidates, result));
  }

  return result;
}

toml::table BuildInputBindingRuntimeConfig(const ConfigBridgeResult& bridge, const CompileResult& compile)
{
  toml::table input_table;
  toml::table binding_table;
  toml::table source_table;

  for (const auto& binding : bridge.bindings) {
    binding_table.insert_or_assign(binding.canonical_key, binding.binding);
    source_table.insert_or_assign(binding.canonical_key, binding.source_key);
  }

  input_table.insert_or_assign("bindings", std::move(binding_table));
  input_table.insert_or_assign("binding_sources", std::move(source_table));

  if (!bridge.compatibility_warnings.empty()) {
    toml::array warnings;
    for (const auto& warning : bridge.compatibility_warnings) {
      warnings.push_back(warning);
    }
    input_table.insert_or_assign("compatibility_warnings", std::move(warnings));
  }

  if (!compile.diagnostics.empty()) {
    toml::array diagnostics;
    for (const auto& diagnostic : compile.diagnostics) {
      diagnostics.push_back(format_binding_diagnostic(diagnostic));
    }
    input_table.insert_or_assign("binding_diagnostics", std::move(diagnostics));
  }

  if (!compile.conflicts.empty()) {
    toml::array conflicts;
    for (const auto& conflict : compile.conflicts) {
      conflicts.push_back(format_binding_conflict(conflict));
    }
    input_table.insert_or_assign("binding_conflicts", std::move(conflicts));
  }

  return input_table;
}
} // namespace input_binding