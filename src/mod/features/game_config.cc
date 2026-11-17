// [config] settings_url and assets_url_override: replace the server addresses in the game's own app config as it
// loads.

#include "config.h"
#include "features/features.h"

#include "game/app.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>

namespace
{
void Model_LoadConfigs_Hook(auto original, Model* _this)
{
  auto model = il2cpp::GcHandle::Strong(_this);
  original(_this);
  auto config = model ? il2cpp::GcHandle::Strong(model.Get<Model>()->appConfig()) : il2cpp::GcHandle{};
  if (!config)
    return;
  const auto settings_url = Config::Get().game.settings_url;
  if (!settings_url.empty()) {
    auto text = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString(settings_url)));
    if (text)
      config.Get<AppConfig>()->SetPlatformSettingsUrl(reinterpret_cast<Il2CppString*>(text.Get()));
  }
  // A native setter may replace Config or rebind the model. Preserve the source's
  // fresh settings read and never send the second write into a replaced app config.
  if (model.Get<Model>()->appConfig() != config.Get())
    return;
  const auto assets_url = Config::Get().game.assets_url_override;
  if (!assets_url.empty()) {
    auto text = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString(assets_url)));
    if (text)
      config.Get<AppConfig>()->SetAssetUrlOverride(reinterpret_cast<Il2CppString*>(text.Get()));
  }
}

bool InstallGameConfigOverrides()
{
  il2cpp::HookSet hooks("GameConfigOverrides");
  IL2CPP_STAGE(hooks, Model::Class::Method<void()>("LoadConfigs"), Model_LoadConfigs_Hook);
  return hooks.Commit();
}
} // namespace

const Feature kGameConfigOverrides{"GameConfigOverrides", &Config::Patches::testpatches, InstallGameConfigOverrides};
