/**
 * @file testing_config_override.cc
 * @brief Dev-only config URL override hook extracted from testing.cc.
 *
 * This source owns the nonpublic config/asset URL injection behavior so the
 * public release profile can compile-exclude it without affecting mature
 * public testing hooks such as cursor behavior or queue unlock gating.
 */
#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

namespace
{
class AppConfig
{
public:
  __declspec(property(get = __get_PlatformSettingsUrl, put = __set_PlatformSettingsUrl))
  Il2CppString*                                                                                  PlatformSettingsUrl;
  __declspec(property(get = __get_PlatformApiKey, put = __set_PlatformApiKey)) Il2CppString*     PlatformApiKey;
  __declspec(property(get = __get_AssetUrlOverride, put = __set_AssetUrlOverride)) Il2CppString* AssetUrlOverride;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "AppConfig");
    return class_helper;
  }

public:
  Il2CppString* __get_PlatformSettingsUrl()
  {
    static auto prop = get_class_helper().GetProperty("PlatformSettingsUrl");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_PlatformSettingsUrl(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("PlatformSettingsUrl");
    return prop.SetRaw((void*)this, *v);
  }

  Il2CppString* __get_PlatformApiKey()
  {
    static auto prop = get_class_helper().GetProperty("PlatformApiKey");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_PlatformApiKey(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("PlatformApiKey");
    return prop.SetRaw((void*)this, *v);
  }

  Il2CppString* __get_AssetUrlOverride()
  {
    static auto prop = get_class_helper().GetProperty("AssetUrlOverride");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_AssetUrlOverride(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("AssetUrlOverride");
    return prop.SetRaw((void*)this, *v);
  }
};

class Model
{
public:
  __declspec(property(get = __get_AppConfig)) AppConfig* AppConfig_;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Model");
    return class_helper;
  }

public:
  AppConfig* __get_AppConfig()
  {
    static auto field = get_class_helper().GetField("_appConfig");
    return *(AppConfig**)((ptrdiff_t)this + field.offset());
  }
};

AppConfig* Model_LoadConfigs(auto original, Model* _this)
{
  original(_this);
  auto config = _this->AppConfig_;

  if (!Config::Get().config_settings_url.empty()) {
    auto new_settings_url       = il2cpp_string_new(Config::Get().config_settings_url.c_str());
    config->PlatformSettingsUrl = new_settings_url;
  }

  if (!Config::Get().config_assets_url_override.empty()) {
    auto new_url             = il2cpp_string_new(Config::Get().config_assets_url_override.c_str());
    config->AssetUrlOverride = new_url;
  }

  return config;
}
} // namespace

void InstallTestingConfigOverrideHooks()
{
  auto model = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Model");
  if (!model.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "Model");
  } else {
    auto load_configs_ptr = model.GetMethod("LoadConfigs");
    if (load_configs_ptr == nullptr) {
      ErrorMsg::MissingMethod("Model", "LoadConfigs");
    } else {
      SPUD_STATIC_DETOUR(load_configs_ptr, Model_LoadConfigs);
    }
  }
}
