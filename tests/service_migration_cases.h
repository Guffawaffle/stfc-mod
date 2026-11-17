#pragma once

// Real production consumers; config lifetime and native bodies are controlled.
struct ServiceFixtureConfig {
  using Patches = Config::Patches;
  decltype(Config::Get().game)  game{};
  decltype(Config::Get().buffs) buffs{};
  static ServiceFixtureConfig&  Get();
};
ServiceFixtureConfig  service_first, service_second;
ServiceFixtureConfig* service_config = &service_first;
ServiceFixtureConfig& ServiceFixtureConfig::Get()
{ return *service_config; }
#define Config ServiceFixtureConfig
#include "../src/mod/features/buffs.cc"
#include "../src/mod/features/game_config.cc"
#undef Config

namespace
{
struct ServiceModel {
  Il2CppObject header{};
  AppConfig*   config = nullptr;
};
std::vector<std::string> service_writes;
std::function<void()>    service_on_settings;
void                     ServiceSettings(AppConfig*, Il2CppString* value, const MethodInfo*)
{
  service_writes.push_back("settings:" + il2cpp::ToUtf8(value));
  if (service_on_settings)
    service_on_settings();
}
void ServiceAssets(AppConfig*, Il2CppString* value, const MethodInfo*)
{ service_writes.push_back("assets:" + il2cpp::ToUtf8(value)); }
void ServiceEndpointCases()
{
  auto& model               = Define("Digit.Client.Core", "Model", IL2CPP_TYPE_CLASS, &object_class);
  auto& config              = Define("Digit.Client.Core", "AppConfig", IL2CPP_TYPE_CLASS, &object_class);
  model.klass.instance_size = sizeof(ServiceModel);
  AddField(model, "_appConfig", config, offsetof(ServiceModel, config));
  const auto& settings = AddMethod(config, "set_PlatformSettingsUrl", reinterpret_cast<void*>(&ServiceSettings),
                                   T(void_class), {T(string_class)});
  const auto& assets = AddMethod(config, "set_AssetUrlOverride", reinterpret_cast<void*>(&ServiceAssets), T(void_class),
                                 {T(string_class)});
  AddProperty(config, "PlatformSettingsUrl", nullptr, &settings);
  AddProperty(config, "AssetUrlOverride", nullptr, &assets);
  Il2CppObject app{&config.klass};
  ServiceModel source{{&model.klass}};
  auto*        receiver  = reinterpret_cast<Model*>(&source);
  int          originals = 0;
  auto         original  = [&](Model*) {
    ++originals;
    source.config = reinterpret_cast<AppConfig*>(&app);
  };
  Model_LoadConfigs_Hook(original, receiver);
  Require(originals == 1 && service_writes.empty(), "empty overrides keep native config; original runs first once");
  service_first.game.settings_url         = "https://fixture.invalid/settings";
  service_first.game.assets_url_override  = "https://fixture.invalid/old-assets";
  service_second.game.assets_url_override = "https://fixture.invalid/reloaded-assets";
  service_on_settings                     = [] { service_config = &service_second; };
  Model_LoadConfigs_Hook(original, receiver);
  Require(service_writes
              == std::vector<std::string>{"settings:https://fixture.invalid/settings",
                                          "assets:https://fixture.invalid/reloaded-assets"},
          "native setter reentry reacquires current settings instead of keeping the replaced Config reference");
  service_on_settings = {};
  service_writes.clear();
  service_first.game.settings_url.clear();
  service_config = &service_first;
  Model_LoadConfigs_Hook(original, receiver);
  Require(service_writes == std::vector<std::string>{"assets:https://fixture.invalid/old-assets"},
          "assets-only override leaves native settings URL untouched");
  service_writes.clear();
  service_first.game.settings_url = "https://fixture.invalid/settings";
  service_first.game.assets_url_override.clear();
  Model_LoadConfigs_Hook(original, receiver);
  Require(service_writes == std::vector<std::string>{"settings:https://fixture.invalid/settings"},
          "settings-only override leaves native assets URL untouched");
  service_writes.clear();
  Il2CppObject replacement{&config.klass};
  service_first.game.assets_url_override = "https://fixture.invalid/old-assets";
  service_on_settings                    = [&] { source.config = reinterpret_cast<AppConfig*>(&replacement); };
  Model_LoadConfigs_Hook(original, receiver);
  Require(service_writes == std::vector<std::string>{"settings:https://fixture.invalid/settings"},
          "app-config replacement cannot receive a stale second override");
  service_writes.clear();
  service_on_settings = {};
  Model_LoadConfigs_Hook(
      [&](Model*) {
        ++originals;
        source.config = nullptr;
      },
      receiver);
  Require(service_writes.empty() && originals == 6, "unavailable native app config sends no overrides");
  Require(warp_roots.empty(), "endpoint consumer releases temporary managed custody");
  std::cout << "Production endpoint original-order, config reload and unavailable-config cases passed\n";
}
struct ServiceList {
  Il2CppObject               header{};
  std::vector<Il2CppObject*> items;
  bool                       fail_count = false, fail_item = false;
};
int32_t ServiceCount(ServiceList* list, const MethodInfo*)
{
  if (list->fail_count)
    throw std::runtime_error("controlled count unavailable");
  return static_cast<int32_t>(list->items.size());
}
Il2CppObject* ServiceItem(ServiceList* list, int32_t index, const MethodInfo*)
{
  if (list->fail_item)
    throw std::runtime_error("controlled item unavailable");
  return list->items.at(index);
}
void ServiceBuffCases()
{
  auto& klass = Define("Fixture", "BuffList", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(klass, "get_Count", reinterpret_cast<void*>(&ServiceCount), T(int_class), {});
  AddMethod(klass, "get_Item", reinterpret_cast<void*>(&ServiceItem), T(object_class), {T(int_class)});
  Il2CppObject a{&object_class.klass}, b{&object_class.klass};
  ServiceList  source{{&klass.klass}, {&a, &b}};
  auto*        list     = reinterpret_cast<il2cpp::List<il2cpp::Object*>*>(&source);
  int          calls    = 0;
  auto         original = [&](ClientModifierType modifier, auto* value) {
    ++calls;
    if (value)
      Require(std::ranges::any_of(warp_roots, [&](const auto& root) { return root.second == value; }),
              "list custody extends through the native original call");
    Require(static_cast<int>(modifier) == 987, "modifier identity passes unchanged");
    return value;
  };
  const auto modifier = static_cast<ClientModifierType>(987);
  Require(BuffService_ExtractBuffsOfType_Hook(original, modifier, nullptr) == nullptr && calls == 1,
          "null input keeps native behavior exactly once");
  Require(BuffService_ExtractBuffsOfType_Hook(original, modifier, list) == list && calls == 2,
          "complete list passes unchanged");
  for (size_t i = 0; i < source.items.size(); ++i) {
    auto* value     = source.items[i];
    source.items[i] = nullptr;
    Require(BuffService_ExtractBuffsOfType_Hook(original, modifier, list) == nullptr && calls == 2,
            "any observed null prevents native crash path");
    source.items[i] = value;
  }
  source.items.clear();
  Require(BuffService_ExtractBuffsOfType_Hook(original, modifier, list) == list && calls == 3,
          "observed empty list passes unchanged");
  source.fail_count = true;
  Require(BuffService_ExtractBuffsOfType_Hook(original, modifier, list) == nullptr && calls == 3,
          "unavailable count is not an observed empty list and cannot enter native crash path");
  source.fail_count = false;
  source.items      = {&a};
  source.fail_item  = true;
  Require(BuffService_ExtractBuffsOfType_Hook(original, modifier, list) == nullptr && calls == 3,
          "unavailable item remains unqualified");
  int  condition_calls    = 0;
  auto condition_original = [&](auto*, auto, auto*, auto*, bool exclude, bool loyalty) {
    ++condition_calls;
    Require(exclude && !loyalty, "condition flags unchanged");
    return true;
  };
  service_first.buffs.use_out_of_dock_power = true;
  service_config                            = &service_first;
  Require(!BuffService_IsBuffConditionMet_Hook(condition_original, nullptr, BuffCondition::CondSelfAtStation, nullptr,
                                               nullptr, true, false)
              && condition_calls == 0,
          "enabled station-only condition is excluded");
  service_first.buffs.use_out_of_dock_power = false;
  Require(BuffService_IsBuffConditionMet_Hook(condition_original, nullptr, BuffCondition::CondSelfAtStation, nullptr,
                                              nullptr, true, false)
              && condition_calls == 1,
          "live disable preserves native condition result");
  service_first.buffs.use_out_of_dock_power = true;
  Require(BuffService_IsBuffConditionMet_Hook(condition_original, nullptr, static_cast<BuffCondition>(999), nullptr,
                                              nullptr, true, false)
              && condition_calls == 2,
          "unrecognized conditions delegate without classification");
  Require(warp_roots.empty(), "buff consumer releases list custody");
  std::cout << "Production buff known/unknown collection and live condition cases passed\n";
}
} // namespace
