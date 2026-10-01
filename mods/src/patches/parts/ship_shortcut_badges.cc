#include "patches/ship_shortcut_badges.h"

#include "config.h"
#include "il2cpp/method_contract.h"
#include "patches/mapkey.h"
#include "prime/FleetsManager.h"
#include "prime/NavigationFleetWidget.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <string>
#include <unordered_map>

namespace ship_shortcut_badges
{
namespace
{
  constexpr std::array kSelectShip{GameFunction::SelectShip1, GameFunction::SelectShip2, GameFunction::SelectShip3,
                                   GameFunction::SelectShip4, GameFunction::SelectShip5, GameFunction::SelectShip6,
                                   GameFunction::SelectShip7, GameFunction::SelectShip8};

  struct Methods {
    FieldInfo*        label          = nullptr;
    const MethodInfo* is_local       = nullptr;
    const MethodInfo* fleet_index    = nullptr;
    const MethodInfo* override_text  = nullptr;
    const MethodInfo* clear_override = nullptr;

    Methods()
    {
      auto widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationFleetWidget");
      auto fleet =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "FleetDeployedData");
      auto manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.FleetManagement", "FleetsManager");
      auto text    = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "TextLocalizer");
      label        = widget.get_cls() ? il2cpp_class_get_field_from_name(widget.get_cls(), "_fleetLevel") : nullptr;
      if (label
          && ((il2cpp_field_get_flags(label) & FIELD_ATTRIBUTE_STATIC)
              || label->offset < static_cast<int32_t>(sizeof(Il2CppObject))
              || !method_contract::Type(label->type, "Digit.Client.UI.TextLocalizer")))
        label = nullptr;
      is_local = method_contract::Resolve(fleet.get_cls(), "get_IsLocalPlayer", false, "System.Boolean", {});
      fleet_index =
          method_contract::Resolve(manager.get_cls(), "GetPlayerFleetIndex", false, "System.Int32", {"System.Int64"});
      override_text =
          method_contract::Resolve(text.get_cls(), "OverrideLocalizedText", false, "System.Void", {"System.String"});
      clear_override = method_contract::Resolve(text.get_cls(), "ClearTextOverride", false, "System.Void", {});
      if (*this)
        MapKey::CacheShortcutHints();
      else
        spdlog::warn("[ShipBadges] Native label contract unavailable; retaining dock letters");
    }

    explicit operator bool() const
    { return label && is_local && fleet_index && override_text && clear_override; }
  };

  Methods& GetMethods()
  {
    static Methods methods;
    return methods;
  }

  // The existing widget lifecycle owns these pointers. Only local-player widgets are retained.
  struct Badge {
    int         index;
    std::string hint;
  };
  std::unordered_map<NavigationFleetWidget*, Badge> badges;

  Il2CppObject* Label(NavigationFleetWidget* widget, const Methods& methods)
  {
    Il2CppObject* label = nullptr;
    if (widget && methods.label)
      il2cpp_field_get_value(reinterpret_cast<Il2CppObject*>(widget), methods.label, &label);
    return label;
  }

  void Update(NavigationFleetWidget* widget, Badge& badge)
  {
    const auto& methods = GetMethods();
    const auto& config  = Config::Get();
    const auto  hint =
        config.ship_hotkey_badges && config.installHotkeyHooks && config.hotkeys_enabled && !config.use_scopely_hotkeys
            ? MapKey::GetShortcutHint(kSelectShip[badge.index])
            : std::string{};
    if (badge.hint == hint)
      return;
    if (auto* label = Label(widget, methods)) {
      Il2CppException* exception = nullptr;
      if (hint.empty()) {
        il2cpp_runtime_invoke(methods.clear_override, label, nullptr, &exception);
      } else {
        auto* text = il2cpp_string_new(hint.c_str());
        void* args[]{text};
        il2cpp_runtime_invoke(methods.override_text, label, args, &exception);
      }
      if (!exception)
        badge.hint = hint;
    }
  }
} // namespace

bool Available()
{ return static_cast<bool>(GetMethods()); }

void Release(NavigationFleetWidget* widget)
{
  const auto found = badges.find(widget);
  if (found == badges.end())
    return;
  const bool overridden = !found->second.hint.empty();
  badges.erase(found);
  const auto& methods = GetMethods();
  if (auto* label = Label(widget, methods); overridden && label && methods.clear_override) {
    Il2CppException* exception = nullptr;
    il2cpp_runtime_invoke(methods.clear_override, label, nullptr, &exception);
  }
}

void Bind(NavigationFleetWidget* widget)
{
  if (!widget)
    return;
  Release(widget);
  const auto& methods = GetMethods();
  auto*       fleet   = widget->Context;
  if (!methods || !fleet)
    return;

  Il2CppException* exception = nullptr;
  auto*            local     = il2cpp_runtime_invoke(methods.is_local, fleet, nullptr, &exception);
  if (exception || !local || !*static_cast<bool*>(il2cpp_object_unbox(local)))
    return;
  auto*      manager = FleetsManager::Instance();
  auto       id      = fleet->ID;
  void*      index_args[]{&id};
  auto*      result = manager ? il2cpp_runtime_invoke(methods.fleet_index, manager, index_args, &exception) : nullptr;
  const auto index  = !exception && result ? *static_cast<int32_t*>(il2cpp_object_unbox(result)) : -1;
  if (index < 0 || index >= static_cast<int>(kSelectShip.size()))
    return;

  auto [entry, inserted] = badges.insert_or_assign(widget, Badge{index, {}});
  Update(widget, entry->second);
}

void Refresh()
{
  // Rebinding in settings updates visible badges without reloading the system.
  static auto next_refresh = std::chrono::steady_clock::time_point{};
  const auto  now          = std::chrono::steady_clock::now();
  if (now < next_refresh)
    return;
  next_refresh = now + std::chrono::milliseconds(250);
  for (auto& [widget, badge] : badges)
    Update(widget, badge);
}
} // namespace ship_shortcut_badges
