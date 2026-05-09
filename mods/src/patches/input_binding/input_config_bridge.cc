#include "patches/input_binding/input_config_bridge.h"

#include "str_utils_pure.h"

#include <optional>
#include <sstream>
#include <utility>

namespace input_binding
{
namespace
{
  constexpr BindingConfigAlias kFleetPrimaryAliases[] = {
    {"action_primary", BindingConfigSourceKind::LegacyAlias, {}},
    {"action_queue", BindingConfigSourceKind::LegacyAlias, {}},
    {"action_recall_cancel", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kFleetSecondaryAliases[] = {
    {"action_secondary", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kFleetServiceAliases[] = {
    {"action_recall", BindingConfigSourceKind::LegacyAlias, {}},
    {"action_repair", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kFleetViewInfoAliases[] = {
    {"action_view", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kFleetQueueClearAliases[] = {
    {"action_queue_clear", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kFleetQueueToggleAliases[] = {
    {"toggle_queue", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kHotkeysDisableAliases[] = {
    {"set_hotkeys_disable", BindingConfigSourceKind::LegacyAlias, {}},
    {"set_hotkeys_disble", BindingConfigSourceKind::DeprecatedAlias,
     "[shortcuts].set_hotkeys_disble is deprecated; prefer [input.bindings].hotkeys_disable or "
     "[shortcuts].set_hotkeys_disable."},
    {"set_hotkeys_disabled", BindingConfigSourceKind::DeprecatedAlias,
     "[shortcuts].set_hotkeys_disabled is deprecated; prefer [input.bindings].hotkeys_disable or "
     "[shortcuts].set_hotkeys_disable."},
  };
  constexpr BindingConfigAlias kHotkeysEnableAliases[] = {
    {"set_hotkeys_enable", BindingConfigSourceKind::LegacyAlias, {}},
    {"set_hotkeys_enabled", BindingConfigSourceKind::DeprecatedAlias,
     "[shortcuts].set_hotkeys_enabled is accepted for compatibility; prefer "
     "[input.bindings].hotkeys_enable or [shortcuts].set_hotkeys_enable."},
  };
  constexpr BindingConfigAlias kUiScaleUpAliases[] = {
    {"ui_scaleup", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kUiScaleDownAliases[] = {
    {"ui_scaledown", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kUiViewerScaleUpAliases[] = {
    {"ui_scaleviewerup", BindingConfigSourceKind::LegacyAlias, {}},
  };
  constexpr BindingConfigAlias kUiViewerScaleDownAliases[] = {
    {"ui_scaleviewerdown", BindingConfigSourceKind::LegacyAlias, {}},
  };

  constexpr BindingConfigAlias kSelectShip1Aliases[] = {{"select_ship1", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectShip2Aliases[] = {{"select_ship2", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectShip3Aliases[] = {{"select_ship3", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectShip4Aliases[] = {{"select_ship4", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectShip5Aliases[] = {{"select_ship5", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectShip6Aliases[] = {{"select_ship6", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectShip7Aliases[] = {{"select_ship7", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectShip8Aliases[] = {{"select_ship8", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectCurrentAliases[] = {{"select_current", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowChatAliases[] = {{"show_chat", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowChatSide1Aliases[] = {{"show_chatside1", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowChatSide2Aliases[] = {{"show_chatside2", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectChatGlobalAliases[] = {{"select_chatglobal", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectChatAllianceAliases[] = {{"select_chatalliance", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kSelectChatPrivateAliases[] = {{"select_chatprivate", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kMoveLeftAliases[] = {{"move_left", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kMoveRightAliases[] = {{"move_right", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kLogDebugAliases[] = {{"log_debug", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kZoomInAliases[] = {{"zoom_in", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kZoomOutAliases[] = {{"zoom_out", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kQuitAliases[] = {{"quit", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kLogOffAliases[] = {{"log_off", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kLogErrorAliases[] = {{"log_error", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kLogWarnAliases[] = {{"log_warn", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kLogInfoAliases[] = {{"log_info", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kLogTraceAliases[] = {{"log_trace", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowQTrialsAliases[] = {{"show_qtrials", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowBookmarksAliases[] = {{"show_bookmarks", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowLookupAliases[] = {{"show_lookup", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowRefineryAliases[] = {{"show_refinery", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowFactionsAliases[] = {{"show_factions", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowStationExteriorAliases[] = {{"show_stationexterior", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowGalaxyAliases[] = {{"show_galaxy", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowStationInteriorAliases[] = {{"show_stationinterior", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowSystemAliases[] = {{"show_system", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowArtifactsAliases[] = {{"show_artifacts", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowInventoryAliases[] = {{"show_inventory", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowMissionsAliases[] = {{"show_missions", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowResearchAliases[] = {{"show_research", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowScrapYardAliases[] = {{"show_scrapyard", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowOfficersAliases[] = {{"show_officers", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowCommanderAliases[] = {{"show_commander", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowAwayTeamAliases[] = {{"show_awayteam", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowEventsAliases[] = {{"show_events", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowExoCompAliases[] = {{"show_exocomp", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowDailyAliases[] = {{"show_daily", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowGiftsAliases[] = {{"show_gifts", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowAllianceAliases[] = {{"show_alliance", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowAllianceHelpAliases[] = {{"show_alliance_help", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowAllianceArmadaAliases[] = {{"show_alliance_armada", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowSettingsAliases[] = {{"show_settings", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kTogglePreviewLocateAliases[] = {{"toggle_preview_locate", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kTogglePreviewRecallAliases[] = {{"toggle_preview_recall", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kToggleCargoDefaultAliases[] = {{"toggle_cargo_default", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kToggleCargoPlayerAliases[] = {{"toggle_cargo_player", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kToggleCargoStationAliases[] = {{"toggle_cargo_station", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kToggleCargoHostileAliases[] = {{"toggle_cargo_hostile", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kToggleCargoArmadaAliases[] = {{"toggle_cargo_armada", BindingConfigSourceKind::LegacyAlias, {}}};
  constexpr BindingConfigAlias kShowShipsAliases[] = {{"show_ships", BindingConfigSourceKind::LegacyAlias, {}}};

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
    candidates.reserve(1 + ShortcutConfigAliases(spec.id).size());
    candidates.push_back(read_candidate(input_bindings, spec.canonical_key, "[input.bindings].",
                                        BindingConfigSourceKind::Canonical, result));

    for (const auto& alias : ShortcutConfigAliases(spec.id)) {
      candidates.push_back(read_candidate(shortcuts, alias.key, "[shortcuts].", alias.source_kind, result,
                                          alias.deprecation_warning));
    }

    result.bindings.push_back(resolve_binding(spec, candidates, result));
  }

  return result;
}

std::span<const BindingConfigAlias> ShortcutConfigAliases(const InputActionId action)
{
  switch (action) {
    case InputActionId::FleetPrimary:
      return kFleetPrimaryAliases;
    case InputActionId::FleetSecondary:
      return kFleetSecondaryAliases;
    case InputActionId::FleetService:
      return kFleetServiceAliases;
    case InputActionId::FleetViewInfo:
      return kFleetViewInfoAliases;
    case InputActionId::FleetQueueClear:
      return kFleetQueueClearAliases;
    case InputActionId::FleetQueueToggle:
      return kFleetQueueToggleAliases;
    case InputActionId::SelectShip1:
      return kSelectShip1Aliases;
    case InputActionId::SelectShip2:
      return kSelectShip2Aliases;
    case InputActionId::SelectShip3:
      return kSelectShip3Aliases;
    case InputActionId::SelectShip4:
      return kSelectShip4Aliases;
    case InputActionId::SelectShip5:
      return kSelectShip5Aliases;
    case InputActionId::SelectShip6:
      return kSelectShip6Aliases;
    case InputActionId::SelectShip7:
      return kSelectShip7Aliases;
    case InputActionId::SelectShip8:
      return kSelectShip8Aliases;
    case InputActionId::SelectCurrent:
      return kSelectCurrentAliases;
    case InputActionId::ShowChat:
      return kShowChatAliases;
    case InputActionId::ShowChatSide1:
      return kShowChatSide1Aliases;
    case InputActionId::ShowChatSide2:
      return kShowChatSide2Aliases;
    case InputActionId::SelectChatGlobal:
      return kSelectChatGlobalAliases;
    case InputActionId::SelectChatAlliance:
      return kSelectChatAllianceAliases;
    case InputActionId::SelectChatPrivate:
      return kSelectChatPrivateAliases;
    case InputActionId::MoveLeft:
      return kMoveLeftAliases;
    case InputActionId::MoveRight:
      return kMoveRightAliases;
    case InputActionId::HotkeysDisable:
      return kHotkeysDisableAliases;
    case InputActionId::HotkeysEnable:
      return kHotkeysEnableAliases;
    case InputActionId::LogDebug:
      return kLogDebugAliases;
    case InputActionId::ZoomIn:
      return kZoomInAliases;
    case InputActionId::ZoomOut:
      return kZoomOutAliases;
    case InputActionId::Quit:
      return kQuitAliases;
    case InputActionId::UiScaleUp:
      return kUiScaleUpAliases;
    case InputActionId::UiScaleDown:
      return kUiScaleDownAliases;
    case InputActionId::UiViewerScaleUp:
      return kUiViewerScaleUpAliases;
    case InputActionId::UiViewerScaleDown:
      return kUiViewerScaleDownAliases;
    case InputActionId::LogOff:
      return kLogOffAliases;
    case InputActionId::LogError:
      return kLogErrorAliases;
    case InputActionId::LogWarn:
      return kLogWarnAliases;
    case InputActionId::LogInfo:
      return kLogInfoAliases;
    case InputActionId::LogTrace:
      return kLogTraceAliases;
    case InputActionId::ShowQTrials:
      return kShowQTrialsAliases;
    case InputActionId::ShowBookmarks:
      return kShowBookmarksAliases;
    case InputActionId::ShowLookup:
      return kShowLookupAliases;
    case InputActionId::ShowRefinery:
      return kShowRefineryAliases;
    case InputActionId::ShowFactions:
      return kShowFactionsAliases;
    case InputActionId::ShowStationExterior:
      return kShowStationExteriorAliases;
    case InputActionId::ShowGalaxy:
      return kShowGalaxyAliases;
    case InputActionId::ShowStationInterior:
      return kShowStationInteriorAliases;
    case InputActionId::ShowSystem:
      return kShowSystemAliases;
    case InputActionId::ShowArtifacts:
      return kShowArtifactsAliases;
    case InputActionId::ShowInventory:
      return kShowInventoryAliases;
    case InputActionId::ShowMissions:
      return kShowMissionsAliases;
    case InputActionId::ShowResearch:
      return kShowResearchAliases;
    case InputActionId::ShowScrapYard:
      return kShowScrapYardAliases;
    case InputActionId::ShowOfficers:
      return kShowOfficersAliases;
    case InputActionId::ShowCommander:
      return kShowCommanderAliases;
    case InputActionId::ShowAwayTeam:
      return kShowAwayTeamAliases;
    case InputActionId::ShowEvents:
      return kShowEventsAliases;
    case InputActionId::ShowExoComp:
      return kShowExoCompAliases;
    case InputActionId::ShowDaily:
      return kShowDailyAliases;
    case InputActionId::ShowGifts:
      return kShowGiftsAliases;
    case InputActionId::ShowAlliance:
      return kShowAllianceAliases;
    case InputActionId::ShowAllianceHelp:
      return kShowAllianceHelpAliases;
    case InputActionId::ShowAllianceArmada:
      return kShowAllianceArmadaAliases;
    case InputActionId::ShowSettings:
      return kShowSettingsAliases;
    case InputActionId::TogglePreviewLocate:
      return kTogglePreviewLocateAliases;
    case InputActionId::TogglePreviewRecall:
      return kTogglePreviewRecallAliases;
    case InputActionId::ToggleCargoDefault:
      return kToggleCargoDefaultAliases;
    case InputActionId::ToggleCargoPlayer:
      return kToggleCargoPlayerAliases;
    case InputActionId::ToggleCargoStation:
      return kToggleCargoStationAliases;
    case InputActionId::ToggleCargoHostile:
      return kToggleCargoHostileAliases;
    case InputActionId::ToggleCargoArmada:
      return kToggleCargoArmadaAliases;
    case InputActionId::ShowShips:
      return kShowShipsAliases;
    case InputActionId::Max:
      return {};
  }

  return {};
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