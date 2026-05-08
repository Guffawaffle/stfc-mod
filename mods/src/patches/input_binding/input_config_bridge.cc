#include "patches/input_binding/input_config_bridge.h"

#include "str_utils_pure.h"

#include <optional>
#include <sstream>
#include <utility>

namespace input_binding
{
namespace
{
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

  std::optional<std::string> read_binding_value(const toml::node_view<const toml::node> node,
                                                const std::string& source_key, ConfigBridgeResult& result)
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

  std::optional<BindingCandidate> read_candidate(const toml::table* table, const std::string_view key,
                                                 const std::string_view        source_prefix,
                                                 const BindingConfigSourceKind source_kind, ConfigBridgeResult& result,
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
  { return BindingCandidate{std::string(spec.default_bind), "default", BindingConfigSourceKind::Default}; }

  void warn_conflict(const InputActionSpec& spec, const BindingCandidate& chosen, const BindingCandidate& ignored,
                     ConfigBridgeResult& result)
  {
    std::ostringstream message;
    message << "Conflicting bindings for [input.bindings]." << spec.canonical_key << ": using " << chosen.source_key
            << "='" << chosen.binding << "', ignoring " << ignored.source_key << "='" << ignored.binding << "'.";
    add_warning(result, message.str());
  }

  ResolvedBinding resolve_binding(const InputActionSpec&                              spec,
                                  const std::vector<std::optional<BindingCandidate>>& candidates,
                                  ConfigBridgeResult&                                 result)
  {
    auto chosen       = default_candidate(spec);
    bool saw_explicit = false;

    for (const auto& candidate : candidates) {
      if (!candidate) {
        continue;
      }

      if (!saw_explicit) {
        chosen       = *candidate;
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

  std::optional<BindingCandidate> resolve_disable_hotkeys_shortcut(const toml::table&  config,
                                                                   ConfigBridgeResult& result)
  {
    const auto* shortcuts = shortcuts_table(config);
    auto        canonical =
        read_candidate(shortcuts, "set_hotkeys_disable", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result);
    auto typo = read_candidate(
        shortcuts, "set_hotkeys_disble", "[shortcuts].", BindingConfigSourceKind::DeprecatedAlias, result,
        "[shortcuts].set_hotkeys_disble is deprecated; prefer [input.bindings].hotkeys_disable or "
        "[shortcuts].set_hotkeys_disable.");
    auto legacy = read_candidate(
        shortcuts, "set_hotkeys_disabled", "[shortcuts].", BindingConfigSourceKind::DeprecatedAlias, result,
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
    auto        canonical =
        read_candidate(shortcuts, "set_hotkeys_enable", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result);
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
    const auto*        spec = FindActionSpec(diagnostic.action);
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
        candidates.push_back(
            read_candidate(shortcuts, "action_primary", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        candidates.push_back(
            read_candidate(shortcuts, "action_queue", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        candidates.push_back(read_candidate(shortcuts, "action_recall_cancel", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetSecondary:
        candidates.push_back(read_candidate(shortcuts, "action_secondary", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetService:
        candidates.push_back(
            read_candidate(shortcuts, "action_recall", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        candidates.push_back(
            read_candidate(shortcuts, "action_repair", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetViewInfo:
        candidates.push_back(
            read_candidate(shortcuts, "action_view", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetQueueClear:
        candidates.push_back(read_candidate(shortcuts, "action_queue_clear", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::FleetQueueToggle:
        candidates.push_back(
            read_candidate(shortcuts, "toggle_queue", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectShip1:
        candidates.push_back(
            read_candidate(shortcuts, "select_ship1", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectShip2:
        candidates.push_back(
            read_candidate(shortcuts, "select_ship2", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectShip3:
        candidates.push_back(
            read_candidate(shortcuts, "select_ship3", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectShip4:
        candidates.push_back(
            read_candidate(shortcuts, "select_ship4", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectShip5:
        candidates.push_back(
            read_candidate(shortcuts, "select_ship5", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectShip6:
        candidates.push_back(
            read_candidate(shortcuts, "select_ship6", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectShip7:
        candidates.push_back(
            read_candidate(shortcuts, "select_ship7", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectShip8:
        candidates.push_back(
            read_candidate(shortcuts, "select_ship8", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectCurrent:
        candidates.push_back(
            read_candidate(shortcuts, "select_current", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowChat:
        candidates.push_back(
            read_candidate(shortcuts, "show_chat", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowChatSide1:
        candidates.push_back(
            read_candidate(shortcuts, "show_chatside1", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowChatSide2:
        candidates.push_back(
            read_candidate(shortcuts, "show_chatside2", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectChatGlobal:
        candidates.push_back(read_candidate(shortcuts, "select_chatglobal", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectChatAlliance:
        candidates.push_back(read_candidate(shortcuts, "select_chatalliance", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::SelectChatPrivate:
        candidates.push_back(read_candidate(shortcuts, "select_chatprivate", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::MoveLeft:
        candidates.push_back(
            read_candidate(shortcuts, "move_left", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::MoveRight:
        candidates.push_back(
            read_candidate(shortcuts, "move_right", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::HotkeysDisable:
        candidates.push_back(resolve_disable_hotkeys_shortcut(config, result));
        break;
      case InputActionId::HotkeysEnable:
        candidates.push_back(resolve_enable_hotkeys_shortcut(config, result));
        break;
      case InputActionId::Quit:
        candidates.push_back(
            read_candidate(shortcuts, "quit", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::UiScaleUp:
        candidates.push_back(
            read_candidate(shortcuts, "ui_scaleup", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::UiScaleDown:
        candidates.push_back(
            read_candidate(shortcuts, "ui_scaledown", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::UiViewerScaleUp:
        candidates.push_back(read_candidate(shortcuts, "ui_scaleviewerup", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::UiViewerScaleDown:
        candidates.push_back(read_candidate(shortcuts, "ui_scaleviewerdown", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::LogOff:
        candidates.push_back(
            read_candidate(shortcuts, "log_off", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::LogError:
        candidates.push_back(
            read_candidate(shortcuts, "log_error", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::LogWarn:
        candidates.push_back(
            read_candidate(shortcuts, "log_warn", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::LogInfo:
        candidates.push_back(
            read_candidate(shortcuts, "log_info", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::LogDebug:
        candidates.push_back(
            read_candidate(shortcuts, "log_debug", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::LogTrace:
        candidates.push_back(
            read_candidate(shortcuts, "log_trace", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowQTrials:
        candidates.push_back(
            read_candidate(shortcuts, "show_qtrials", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowBookmarks:
        candidates.push_back(
            read_candidate(shortcuts, "show_bookmarks", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowLookup:
        candidates.push_back(
            read_candidate(shortcuts, "show_lookup", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowRefinery:
        candidates.push_back(
            read_candidate(shortcuts, "show_refinery", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowFactions:
        candidates.push_back(
            read_candidate(shortcuts, "show_factions", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowStationExterior:
        candidates.push_back(read_candidate(shortcuts, "show_stationexterior", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowGalaxy:
        candidates.push_back(
            read_candidate(shortcuts, "show_galaxy", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowStationInterior:
        candidates.push_back(read_candidate(shortcuts, "show_stationinterior", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowSystem:
        candidates.push_back(
            read_candidate(shortcuts, "show_system", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowArtifacts:
        candidates.push_back(
            read_candidate(shortcuts, "show_artifacts", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowInventory:
        candidates.push_back(
            read_candidate(shortcuts, "show_inventory", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowMissions:
        candidates.push_back(
            read_candidate(shortcuts, "show_missions", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowResearch:
        candidates.push_back(
            read_candidate(shortcuts, "show_research", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowScrapYard:
        candidates.push_back(
            read_candidate(shortcuts, "show_scrapyard", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowOfficers:
        candidates.push_back(
            read_candidate(shortcuts, "show_officers", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowCommander:
        candidates.push_back(
            read_candidate(shortcuts, "show_commander", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowAwayTeam:
        candidates.push_back(
            read_candidate(shortcuts, "show_awayteam", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowEvents:
        candidates.push_back(
            read_candidate(shortcuts, "show_events", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowExoComp:
        candidates.push_back(
            read_candidate(shortcuts, "show_exocomp", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowDaily:
        candidates.push_back(
            read_candidate(shortcuts, "show_daily", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowGifts:
        candidates.push_back(
            read_candidate(shortcuts, "show_gifts", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowAlliance:
        candidates.push_back(
            read_candidate(shortcuts, "show_alliance", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowAllianceHelp:
        candidates.push_back(read_candidate(shortcuts, "show_alliance_help", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowAllianceArmada:
        candidates.push_back(read_candidate(shortcuts, "show_alliance_armada", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowSettings:
        candidates.push_back(
            read_candidate(shortcuts, "show_settings", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::TogglePreviewLocate:
        candidates.push_back(read_candidate(shortcuts, "toggle_preview_locate", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::TogglePreviewRecall:
        candidates.push_back(read_candidate(shortcuts, "toggle_preview_recall", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ToggleCargoDefault:
        candidates.push_back(read_candidate(shortcuts, "toggle_cargo_default", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ToggleCargoPlayer:
        candidates.push_back(read_candidate(shortcuts, "toggle_cargo_player", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ToggleCargoStation:
        candidates.push_back(read_candidate(shortcuts, "toggle_cargo_station", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ToggleCargoHostile:
        candidates.push_back(read_candidate(shortcuts, "toggle_cargo_hostile", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ToggleCargoArmada:
        candidates.push_back(read_candidate(shortcuts, "toggle_cargo_armada", "[shortcuts].",
                                            BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ShowShips:
        candidates.push_back(
            read_candidate(shortcuts, "show_ships", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ZoomIn:
        candidates.push_back(
            read_candidate(shortcuts, "zoom_in", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
        break;
      case InputActionId::ZoomOut:
        candidates.push_back(
            read_candidate(shortcuts, "zoom_out", "[shortcuts].", BindingConfigSourceKind::LegacyAlias, result));
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