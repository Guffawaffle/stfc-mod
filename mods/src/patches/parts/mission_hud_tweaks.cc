#include "config.h"
#include "patches/hook_registry.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace
{
struct MissionsHudViewController {
};

enum class MissionHudButtonId : std::size_t {
  QTrials,
  FieldTraining,
  Outposts,
  Missions,
  Count,
};

constexpr auto kButtonCount = static_cast<std::size_t>(MissionHudButtonId::Count);

struct MissionHudButtonSpec {
  MissionHudButtonId id;
  const char*        config_name;
  const char*        game_field_name;
};

struct MissionHudRuntimeButton {
  MissionHudButtonId   id;
  MissionHudVisibility visibility = MissionHudVisibility::Auto;
};

struct MissionHudRuntimeConfig {
  std::vector<MissionHudRuntimeButton> buttons;
  std::string                          summary;
};

constexpr std::array<MissionHudButtonSpec, kButtonCount> kButtonSpecs{{
    {MissionHudButtonId::QTrials, "q_trials", "_challengesButton"},
    {MissionHudButtonId::FieldTraining, "field_training", "_achievementsButton"},
    {MissionHudButtonId::Outposts, "outposts", "_outpostsButton"},
    {MissionHudButtonId::Missions, "missions", "_missionsButton"},
}};

constexpr HookDescriptor kOnEnableHook{
    "MissionsHudViewController.OnEnable",
    "Apply configured mission HUD button visibility after the controller is enabled.",
    {"Assembly-CSharp", "Digit.Prime.HUD", "MissionsHudViewController", "OnEnable"},
    "Configured mission HUD buttons may remain hidden or visible."};

constexpr HookDescriptor kOutpostsAndChallengesRefreshHook{
    "MissionsHudViewController.HandleOutpostsAndChallengesHUD",
    "Reapply configured mission HUD visibility after outpost and challenge state changes.",
    {"Assembly-CSharp", "Digit.Prime.HUD", "MissionsHudViewController", "HandleOutpostsAndChallengesHUD"},
    "Configured Q Trials or Outposts visibility may revert after state changes."};

using ComponentGetGameObjectFn = void* (*)(void*);
using GameObjectSetActiveFn    = void (*)(void*, bool);

IL2CppClassHelper& MissionsHudClassHelper()
{
  static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "MissionsHudViewController");
  return class_helper;
}

const MissionHudButtonSpec& ButtonSpec(MissionHudButtonId id)
{ return kButtonSpecs[static_cast<std::size_t>(id)]; }

std::string_view VisibilityName(MissionHudVisibility visibility)
{
  switch (visibility) {
    case MissionHudVisibility::Always:
      return "always";
    case MissionHudVisibility::Never:
      return "never";
    case MissionHudVisibility::Auto:
    default:
      return "auto";
  }
}

void AppendSummary(std::string& summary, const MissionHudButtonSpec& spec, MissionHudVisibility visibility)
{
  if (!summary.empty()) {
    summary += ", ";
  }
  summary += spec.config_name;
  summary += "=";
  summary += VisibilityName(visibility);
}

MissionHudRuntimeConfig ParseRuntimeConfig()
{
  MissionHudRuntimeConfig parsed;
  for (const auto& spec : kButtonSpecs) {
    const auto visibility = MissionHudButtonVisibility(spec.config_name);
    if (visibility == MissionHudVisibility::Auto) {
      continue;
    }

    parsed.buttons.push_back({spec.id, visibility});
    AppendSummary(parsed.summary, spec, visibility);
  }

  if (parsed.summary.empty()) {
    parsed.summary = "(none)";
  }

  spdlog::info("[MissionHudTweaks] visibility={}", parsed.summary);
  return parsed;
}

const MissionHudRuntimeConfig& RuntimeConfig()
{
  static const auto config = ParseRuntimeConfig();
  return config;
}

bool NeedsOutpostsAndChallengesRefresh()
{
  return std::ranges::any_of(RuntimeConfig().buttons, [](const auto& button) {
    return button.id == MissionHudButtonId::QTrials || button.id == MissionHudButtonId::Outposts;
  });
}

const std::array<ptrdiff_t, kButtonCount>& ControllerFieldOffsets()
{
  static const auto offsets = [] {
    std::array<ptrdiff_t, kButtonCount> resolved{};
    resolved.fill(-1);

    auto& helper = MissionsHudClassHelper();
    if (!helper.isValidHelper()) {
      return resolved;
    }

    std::string map_summary;
    for (const auto& button : RuntimeConfig().buttons) {
      const auto& spec = ButtonSpec(button.id);
      if (!map_summary.empty()) {
        map_summary += "; ";
      }
      map_summary += spec.config_name;
      map_summary += "=";
      map_summary += spec.game_field_name;

      auto field = helper.GetField(spec.game_field_name);
      if (!field.isValidHelper()) {
        spdlog::warn("[MissionHudTweaks] missing MissionsHudViewController field '{}'", spec.game_field_name);
        continue;
      }
      resolved[static_cast<std::size_t>(spec.id)] = field.offset();
    }

    if (!map_summary.empty()) {
      spdlog::info("[MissionHudTweaks] buttonMap {}", map_summary);
    }
    return resolved;
  }();
  return offsets;
}

void* ControllerComponent(MissionsHudViewController* controller, MissionHudButtonId id)
{
  if (controller == nullptr) {
    return nullptr;
  }

  const auto offset = ControllerFieldOffsets()[static_cast<std::size_t>(id)];
  if (offset < 0) {
    return nullptr;
  }

  return *reinterpret_cast<void**>(reinterpret_cast<char*>(controller) + offset);
}

ComponentGetGameObjectFn ComponentGetGameObject()
{
  static auto component_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
  static auto get_game_object =
      reinterpret_cast<ComponentGetGameObjectFn>(component_helper.GetMethod("get_gameObject", 0));
  if (!component_helper.isValidHelper()) {
    return nullptr;
  }
  return get_game_object;
}

GameObjectSetActiveFn GameObjectSetActive()
{
  static auto game_object_helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
  static auto set_active = reinterpret_cast<GameObjectSetActiveFn>(game_object_helper.GetMethod("SetActive", 1));
  if (!game_object_helper.isValidHelper()) {
    return nullptr;
  }
  return set_active;
}

bool SetComponentActive(void* component, bool active)
{
  auto get_game_object = ComponentGetGameObject();
  auto set_active      = GameObjectSetActive();
  if (component == nullptr || get_game_object == nullptr || set_active == nullptr) {
    return false;
  }

  if (auto game_object = get_game_object(component)) {
    set_active(game_object, active);
    return true;
  }

  return false;
}

void ApplyConfiguredVisibility(MissionsHudViewController* controller)
{
  static std::array<bool, kButtonCount> warned_missing_component{};

  for (const auto& button : RuntimeConfig().buttons) {
    auto component = ControllerComponent(controller, button.id);
    if (component == nullptr) {
      const auto index = static_cast<std::size_t>(button.id);
      if (!warned_missing_component[index]) {
        warned_missing_component[index] = true;
        spdlog::debug("[MissionHudTweaks] visibility requested for '{}' but no controller component is available",
                      ButtonSpec(button.id).config_name);
      }
      continue;
    }

    SetComponentActive(component, button.visibility == MissionHudVisibility::Always);
  }
}

void ApplyConfiguredVisibilitySafely(MissionsHudViewController* controller)
{
  try {
    ApplyConfiguredVisibility(controller);
  } catch (const std::exception& exception) {
    spdlog::warn("[MissionHudTweaks] failed applying visibility: {}", exception.what());
  } catch (...) {
    spdlog::warn("[MissionHudTweaks] failed applying visibility");
  }
}

void MissionsHudViewController_OnEnable_Hook(auto original, MissionsHudViewController* controller)
{
  original(controller);
  ApplyConfiguredVisibilitySafely(controller);
}

void MissionsHudViewController_HandleOutpostsAndChallengesHUD_Hook(auto original, MissionsHudViewController* controller)
{
  original(controller);
  ApplyConfiguredVisibilitySafely(controller);
}
} // namespace

void InstallMissionHudTweaksHooks()
{
  HookModuleHealth hooks("MissionHudTweaks");

  if (RuntimeConfig().buttons.empty()) {
    hooks.record_skipped(kOnEnableHook, "no mission HUD button visibility overrides configured");
    hooks.record_skipped(kOutpostsAndChallengesRefreshHook, "no mission HUD button visibility overrides configured");
    hooks.log_summary();
    return;
  }

  auto& helper = MissionsHudClassHelper();
  if (!helper.isValidHelper()) {
    hooks.record_missing_helper(kOnEnableHook);
  } else if (auto on_enable = helper.GetMethod("OnEnable", 0); on_enable == nullptr) {
    hooks.record_missing_method(kOnEnableHook);
  } else {
    (void)ControllerFieldOffsets();
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kOnEnableHook, on_enable, MissionsHudViewController_OnEnable_Hook);
  }

  if (!NeedsOutpostsAndChallengesRefresh()) {
    hooks.record_skipped(kOutpostsAndChallengesRefreshHook, "Q Trials and Outposts use native visibility");
  } else if (!helper.isValidHelper()) {
    hooks.record_missing_helper(kOutpostsAndChallengesRefreshHook);
  } else if (auto refresh = helper.GetMethod("HandleOutpostsAndChallengesHUD", 0); refresh == nullptr) {
    hooks.record_missing_method(kOutpostsAndChallengesRefreshHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kOutpostsAndChallengesRefreshHook, refresh,
                                     MissionsHudViewController_HandleOutpostsAndChallengesHUD_Hook);
  }

  hooks.log_summary();
}
