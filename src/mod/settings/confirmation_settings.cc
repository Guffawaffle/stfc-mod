#include "confirmation_settings.h"
#include "config.h"
#include "features/features.h"
#include "game/settings.h"
#include "runtime_config.h"
#include <algorithm>

namespace mod_settings
{
namespace
{
  struct ConfirmationBackend {
    il2cpp::GcHandle owner, data, key;
    uint64_t         generation = 1;
    void             Invalidate()
    {
      owner.Reset();
      data.Reset();
      ++generation;
    }
    PersistentPrefsManager* Ready()
    {
      const auto existing = settings_binding::ExistingSingleton<PersistentPrefsManager>();
      auto       prefs    = il2cpp::GcHandle::Strong(existing ? existing.value : nullptr);
      auto*      value    = prefs.Get<PersistentPrefsManager>();
      auto       saved    = il2cpp::GcHandle::Strong(value ? value->SavedData() : nullptr);
      if (!value || !saved || !value->CloudLoaded() || value->Loading()) {
        if (owner || data)
          Invalidate();
        return nullptr;
      }
      if (owner.Get() != value || data.Get() != saved.Get()) {
        Invalidate();
        owner = il2cpp::GcHandle::Weak(value);
        data  = il2cpp::GcHandle::Weak(saved.Get());
        if (!owner || !data) {
          Invalidate();
          return nullptr;
        }
      }
      return value;
    }
  };
  ConfirmationBackend& Backend()
  {
    static auto* backend = new ConfirmationBackend;
    return *backend;
  }
  bool FcSupported()
  {
    static const bool available =
        PersistentPrefsManager::ConfirmationAvailable()
        && FleetCommanderManager::Class::Method<void(bool)>("set_HideFleetCommanderAbilityUseConfirmationHidden");
    return available;
  }
  ReadResult ReadFc()
  {
    if (!FcSupported())
      return {Availability::Unsupported};
    auto& backend = Backend();
    auto  owner   = il2cpp::GcHandle::Strong(backend.Ready());
    auto* prefs   = owner.Get<PersistentPrefsManager>();
    if (!prefs)
      return {};
    if (!backend.key)
      backend.key = il2cpp::GcHandle::Strong(
          reinterpret_cast<Il2CppObject*>(il2cpp::NewString("options/hide_fcaa_use_confirmation")));
    if (!backend.key)
      return {};
    const auto generation = backend.generation;
    const auto observed   = prefs->TryCall<"GetBool", bool(Il2CppString*, bool, bool)>(
        reinterpret_cast<Il2CppString*>(backend.key.Get()), false, false);
    return observed && backend.Ready() == prefs && backend.generation == generation
               ? ReadResult::Known(!observed.value, generation)
               : ReadResult{};
  }
  ApplyResult WriteFc(bool confirm, uint64_t generation)
  {
    auto& backend = Backend();
    auto  prefs   = il2cpp::GcHandle::Strong(FcSupported() ? backend.Ready() : nullptr);
    if (!prefs || backend.generation != generation)
      return ApplyResult::Rejected;
    const auto existing = settings_binding::ExistingSingleton<FleetCommanderManager>();
    auto       root     = il2cpp::GcHandle::Strong(existing ? existing.value : nullptr);
    auto*      manager  = root.Get<FleetCommanderManager>();
    if (!manager || !backend.Ready() || backend.generation != generation)
      return ApplyResult::Rejected;
    return manager->TryCall<"set_HideFleetCommanderAbilityUseConfirmationHidden", void(bool)>(!confirm)
               ? ApplyResult::Applied
               : ApplyResult::Unverified;
  }
  bool FtAvailable()
  {
    return std::ranges::any_of(FeatureInstallations(), [](const FeatureInstallation& feature) {
      return feature.name && std::string_view(feature.name) == "ForbiddenTechConfirm"
             && feature.state == InstallState::kInstalled;
    });
  }
} // namespace

BooleanSetting& FleetCommanderConfirmationSetting()
{
  static BooleanSetting setting(
      {"community_mod.fc_ability_confirmation", "[MOD] Confirm Fleet Commander abilities", ReadFc, WriteFc});
  return setting;
}
BooleanSetting& ForbiddenTechConfirmationSetting()
{
  static BooleanSetting setting([] {
    runtime_config::RegisterSetting("ui", "auto_confirm_ft_upgrade");
    Definition definition{
        "community_mod.ft_confirmation", "Confirm Forbidden Tech upgrades",
        [] { return FtAvailable() ? ReadResult::Known(!Config::Get().ui.auto_confirm_ft_upgrade, 1) : ReadResult{}; },
        [](bool confirm, uint64_t generation) {
          if (generation != 1 || !FtAvailable())
            return ApplyResult::Rejected;
          Config::Get().ui.auto_confirm_ft_upgrade = !confirm;
          return ApplyResult::Applied;
        }};
    definition.metadata      = {"ui", "auto_confirm_ft_upgrade", "Ask before upgrading Forbidden Tech.", "",
                                ApplyTiming::Live};
    definition.default_value = true;
    definition.persist       = [](const bool& confirm) {
      runtime_config::SaveSetting("ui", "auto_confirm_ft_upgrade", !confirm);
    };
    definition.save_status = [](const bool& confirm) {
      return runtime_config::GetSaveStatus("ui", "auto_confirm_ft_upgrade", !confirm);
    };
    return definition;
  }());
  return setting;
}
void InvalidateConfirmationSettings()
{
  FleetCommanderConfirmationSetting().InvalidateSession();
  ForbiddenTechConfirmationSetting().InvalidateSession();
  Backend().Invalidate();
}
} // namespace mod_settings
