#pragma once

// The production FC preference adapter over controlled metadata, native bodies and
// existing handle APIs. No real preference store, account or game action is used.
#include "features/features.h"
#include "game/settings.h"
#include "runtime_config.h"
#include "settings/confirmation_settings.h"

namespace fc_confirmation_fixture_runtime
{
int  saves = 0, registrations = 0;
bool RegisterSetting(const char*, const char*) noexcept
{
  ++registrations;
  return true;
}
mod_settings::SaveStatus GetSaveStatus(const char*, const char*, const config_edit::Value&) noexcept
{ return {}; }
void SaveSetting(const char*, const char*, config_edit::Value, std::chrono::milliseconds = {}) noexcept
{ ++saves; }
} // namespace fc_confirmation_fixture_runtime

std::span<const FeatureInstallation> FcConfirmationFixtureFeatures()
{ return {}; }

namespace fc_confirmation_backend
{
using namespace mod_settings;
}

// Existing Settings fixtures supply their own public exports. Keep these actual
// definitions private, while preserving the production model and typed bindings.
#define mod_settings fc_confirmation_backend
#define runtime_config fc_confirmation_fixture_runtime
#define FeatureInstallations FcConfirmationFixtureFeatures
#include "../src/mod/settings/confirmation_settings.cc"
#undef FeatureInstallations
#undef runtime_config
#undef mod_settings

namespace fc_confirmation_fixture
{
struct Prefs {
  Il2CppObject  header{};
  bool          loaded = true, loading = false;
  Il2CppObject* saved = nullptr;
};
Prefs                 prefs, replacement;
Il2CppObject          saved, replacement_saved, manager;
Il2CppObject*         current_prefs     = nullptr;
Il2CppObject*         current_manager   = nullptr;
bool                  prefs_initialized = true, manager_initialized = true, hidden = false;
bool                  get_failure = false, set_failure = false;
int                   gets = 0, sets = 0, factories = 0;
std::function<void()> on_get;
FakeClass *           prefs_base = nullptr, *manager_base = nullptr;

bool StaticRead(FieldInfo* field, void* result)
{
  const bool preference = field->parent == &prefs_base->klass;
  if (!preference && field->parent != &manager_base->klass)
    return false;
  if (std::string_view(field->name) == "s_initialized") {
    const bool value = preference ? prefs_initialized : manager_initialized;
    std::memcpy(result, &value, sizeof(value));
    return true;
  }
  if (std::string_view(field->name) == "s_instance") {
    auto* value = preference ? current_prefs : current_manager;
    std::memcpy(result, &value, sizeof(value));
    return true;
  }
  return false;
}
Il2CppObject* Factory(const MethodInfo*)
{
  ++factories;
  return nullptr;
}
bool GetBool(PersistentPrefsManager* self, Il2CppString* key, bool default_value, bool add_key, const MethodInfo*)
{
  Require(reinterpret_cast<Il2CppObject*>(self) == current_prefs, "FC reads the existing preference owner");
  Require(il2cpp::ToUtf8(key) == "options/hide_fcaa_use_confirmation" && !default_value && !add_key,
          "FC display uses the exact native key and default without inserting a storage key");
  ++gets;
  if (on_get)
    std::exchange(on_get, {})();
  if (get_failure)
    throw Il2CppExceptionWrapper(&thrown);
  return hidden;
}
void SetHidden(FleetCommanderManager* self, bool value, const MethodInfo*)
{
  Require(reinterpret_cast<Il2CppObject*>(self) == current_manager, "FC writes through the existing manager");
  ++sets;
  if (set_failure)
    throw Il2CppExceptionWrapper(&thrown);
  hidden = value;
}

void Build(std::string_view mode)
{
  prefs_base          = &Define("FCFixture.Prefs", "MonoSingleton`1", IL2CPP_TYPE_CLASS, &object_class);
  manager_base        = &Define("FCFixture.Manager", "MonoSingleton`1", IL2CPP_TYPE_CLASS, &object_class);
  auto& prefs_class   = Define("Digit.Prime.PersistentPrefs", "PersistentPrefsManager", IL2CPP_TYPE_CLASS, prefs_base);
  auto& manager_class = Define("Digit.Prime.FleetCommander", "FleetCommanderManager", IL2CPP_TYPE_CLASS, manager_base);
  if (mode != "fc-confirm-missing-singleton")
    AddField(*prefs_base, "s_initialized", bool_class, 0, true);
  AddField(*prefs_base, "s_instance", prefs_class, 0, true);
  AddField(*manager_base, "s_initialized", bool_class, 0, true);
  AddField(*manager_base, "s_instance", manager_class, 0, true);
  AddMethod(*prefs_base, "get_Instance", reinterpret_cast<void*>(&Factory), T(prefs_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(*manager_base, "get_Instance", reinterpret_cast<void*>(&Factory), T(manager_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  if (mode != "fc-confirm-missing-loaded")
    AddField(prefs_class, "_isCloudFileLoaded", bool_class, offsetof(Prefs, loaded));
  if (mode != "fc-confirm-missing-loading")
    AddField(prefs_class, "_isLoading", bool_class, offsetof(Prefs, loading));
  if (mode != "fc-confirm-missing-saved")
    AddField(prefs_class, "_savedData", object_class, offsetof(Prefs, saved));
  if (mode != "fc-confirm-missing-getter") {
    auto& get = AddMethod(prefs_class, "GetBool", reinterpret_cast<void*>(&GetBool), T(bool_class),
                          {T(string_class), T(bool_class), T(bool_class)});
    if (mode == "fc-confirm-wrong-getter")
      const_cast<const Il2CppType**>(get.parameters)[2] = T(intptr_class);
  }
  if (mode != "fc-confirm-missing-setter")
    AddMethod(manager_class, "set_HideFleetCommanderAbilityUseConfirmationHidden", reinterpret_cast<void*>(&SetHidden),
              T(void_class), {T(bool_class)});
  prefs.header.klass = replacement.header.klass = &prefs_class.klass;
  manager.klass                                 = &manager_class.klass;
  saved.klass = replacement_saved.klass = &object_class.klass;
  prefs.saved                           = &saved;
  replacement.saved                     = &replacement_saved;
  current_prefs                         = &prefs.header;
  current_manager                       = &manager;
  tow_static_field_read_override        = StaticRead;
}

void Run(std::string_view mode)
{
  using namespace mod_settings;
  const auto roots_before    = warp_roots.size();
  const auto initializations = initialized.size();
  Build(mode);
  const auto unavailable = [] {
    const auto value = fc_confirmation_backend::ReadFc();
    Require(value.availability == Availability::Unavailable && !value.known() && !value.value,
            "unready preferences remain unavailable instead of a fabricated confirmation value");
  };
  const auto read = [] {
    const auto value = fc_confirmation_backend::ReadFc();
    Require(value.known() && *value.value == !hidden, "FC display inverts the native hidden-confirmation value");
    return value;
  };
  if (mode == "fc-confirm-missing-singleton") {
    unavailable();
    Require(fc_confirmation_backend::WriteFc(false, 1) == ApplyResult::Rejected && gets == 0 && sets == 0,
            "missing singleton metadata cannot authorize a preference write");
  } else if (mode.starts_with("fc-confirm-missing-") || mode == "fc-confirm-wrong-getter") {
    const auto value = fc_confirmation_backend::ReadFc();
    Require(value.availability == Availability::Unsupported && !value.known() && !value.value,
            "incompatible required FC metadata is unsupported before invocation");
    Require(fc_confirmation_backend::WriteFc(false, 1) == ApplyResult::Rejected && gets == 0 && sets == 0,
            "unsupported FC metadata cannot authorize a native call");
  } else if (mode == "fc-confirm-unready") {
    prefs_initialized = false;
    unavailable();
    prefs_initialized = true;
    current_prefs     = nullptr;
    unavailable();
    current_prefs = &prefs.header;
    prefs.loaded  = false;
    unavailable();
    prefs.loaded  = true;
    prefs.loading = true;
    unavailable();
    prefs.loading = false;
    prefs.saved   = nullptr;
    unavailable();
    Require(gets == 0 && sets == 0, "unready preference states never read or write the preference storage");
    prefs.saved = &saved;
    read();
  } else if (mode == "fc-confirm-generation") {
    const auto before = read();
    prefs.saved       = &replacement_saved;
    Require(fc_confirmation_backend::WriteFc(false, before.generation) == ApplyResult::Rejected && sets == 0,
            "replacement saved data invalidates the observed FC generation before mutation");
    const auto after_data = read();
    Require(after_data.generation != before.generation, "saved-data replacement establishes a fresh generation");
    current_prefs = &replacement.header;
    Require(fc_confirmation_backend::WriteFc(false, after_data.generation) == ApplyResult::Rejected && sets == 0,
            "replacement preference owner rejects the former generation");
    Require(read().generation != after_data.generation, "owner replacement establishes another generation");
  } else if (mode == "fc-confirm-read-reentry") {
    on_get = [] { prefs.saved = &replacement_saved; };
    unavailable();
    Require(gets == 1 && sets == 0, "native read reentry cannot acknowledge the former preference storage");
    read();
  } else if (mode == "fc-confirm-read-failure") {
    get_failure = true;
    unavailable();
    Require(gets == 1 && sets == 0, "failed FC native read remains unavailable without a write or retry");
    get_failure = false;
    read();
  } else if (mode == "fc-confirm-set-failure") {
    const auto before = read();
    set_failure       = true;
    Require(fc_confirmation_backend::WriteFc(false, before.generation) == ApplyResult::Unverified && sets == 1
                && !hidden,
            "failed native FC setter is unverified rather than falsely applied");
  } else if (mode == "fc-confirm-manager-missing") {
    const auto before   = read();
    manager_initialized = false;
    Require(fc_confirmation_backend::WriteFc(false, before.generation) == ApplyResult::Rejected && sets == 0,
            "uninitialized FC manager is not constructed for a settings write");
    manager_initialized = true;
    current_manager     = nullptr;
    Require(fc_confirmation_backend::WriteFc(false, before.generation) == ApplyResult::Rejected && sets == 0,
            "absent FC manager cannot receive a write");
    current_manager = &saved;
    Require(fc_confirmation_backend::WriteFc(false, before.generation) == ApplyResult::Rejected && sets == 0,
            "unrelated singleton receiver cannot impersonate the FC manager");
  } else if (mode == "fc-confirm-session") {
    auto&      setting = fc_confirmation_backend::FleetCommanderConfirmationSetting();
    const auto before  = setting.Observe();
    Require(before.state.known(), "FC public setting observes an existing ready backend");
    fc_confirmation_backend::InvalidateConfirmationSettings();
    Require(!setting.IsCurrent(before) && setting.SetFromUser(false, before).outcome == Outcome::Conflict && sets == 0,
            "public session invalidation revokes the old FC setting snapshot before application");
  } else {
    Require(mode == "fc-confirm-native", "known FC confirmation case");
    auto value = read();
    Require(gets == 1 && sets == 0 && !hidden, "display observation never mutates confirmation preferences");
    Require(fc_confirmation_backend::WriteFc(false, value.generation) == ApplyResult::Applied && sets == 1 && hidden,
            "disabling displayed confirmation passes hidden=true to the exact native manager setter");
    value = read();
    Require(fc_confirmation_backend::WriteFc(true, value.generation) == ApplyResult::Applied && sets == 2 && !hidden,
            "enabling displayed confirmation passes hidden=false to the native setter");
    read();
  }
  Require(factories == 0 && initialized.size() == initializations && fc_confirmation_fixture_runtime::saves == 0,
          "FC operations do not invoke singleton factories, initialize static classes or write mod configuration");
  fc_confirmation_backend::Backend().Invalidate();
  fc_confirmation_backend::Backend().key.Reset();
  tow_static_field_read_override = nullptr;
  Require(warp_roots.size() == roots_before,
          "FC fixture retires all retained handles while its controlled runtime lives");
  std::cout << "FC confirmation backend passed: " << mode << '\n';
}
} // namespace fc_confirmation_fixture

void FcConfirmationCases(std::string_view mode)
{ fc_confirmation_fixture::Run(mode); }
