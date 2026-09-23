#include "config.h"
#include "settings/preview_settings.h"
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

void InstallInstantCargoTextHooks();

namespace
{
Il2CppClass *      fleetInfoClass{}, *cargoHoldClass{}, *lerperClass{};
FieldInfo *        providerField{}, *lerperField{}, *fillBarField{};
FieldInfo *        protectedBarField{}, *unprotectedBarField{}, *oversizedBarField{};
bool               installed = false;
thread_local void* instantLerper{};

FieldInfo* ReferenceField(Il2CppClass* cls, const char* name, const char* type)
{
  for (auto* parent = cls; parent; parent = il2cpp_class_get_parent(parent)) {
    auto* field = il2cpp_class_get_field_from_name(parent, name);
    if (field && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC) && field->offset >= sizeof(Il2CppObject)
        && field->offset + sizeof(void*) <= il2cpp_class_instance_size(cls) && method_contract::Type(field->type, type))
      return field;
  }
  return nullptr;
}

Il2CppObject* ReadReference(void* object, FieldInfo* field)
{
  Il2CppObject* result{};
  if (object && field)
    il2cpp_field_get_value(static_cast<Il2CppObject*>(object), field, &result);
  return result;
}

bool IsCargoBar(void* widget)
{
  auto* provider = ReadReference(widget, providerField);
  if (!provider)
    return false;
  const auto* cls = il2cpp_object_get_class(provider);
  if (cls == fleetInfoClass)
    return ReadReference(provider, fillBarField) == widget;
  if (cls == cargoHoldClass)
    return ReadReference(provider, protectedBarField) == widget
           || ReadReference(provider, unprotectedBarField) == widget
           || ReadReference(provider, oversizedBarField) == widget;
  return false;
}

struct InstantScope {
  void* previous = instantLerper;
  explicit InstantScope(void* lerper)
  { instantLerper = lerper; }
  ~InstantScope()
  { instantLerper = previous; }
};

bool StartLerp_Hook(auto original, void* self)
{
  // This bar uses ordinary ProgressData. Identify its owner and exact cargo field,
  // not the data type, so unrelated ship-health/job/reward bars keep their timing.
  if (!installed || !Config::Get().instant_cargo_counter || !self || !IsCargoBar(self))
    return original(self);
  auto* lerper = ReadReference(self, lerperField);
  if (!lerper || il2cpp_object_get_class(lerper) != lerperClass)
    return original(self);
  InstantScope scope(lerper);
  static bool  reported = false;
  if (!reported) {
    reported = true;
    spdlog::info("[InstantCargo] matched owned cargo bar; using native instant timing");
  }
  return original(self);
}

float SetupDuration_Hook(auto original, void* self, Il2CppObject* source, Il2CppObject* target)
{
  // Keep the native interpolation setup/completion sequence, using its supported
  // zero-duration path only inside this particular cargo bar's StartLerp call.
  if (instantLerper && self == instantLerper)
    return 0.0f;
  return original(self, source, target);
}
} // namespace

bool mod_settings::InstantCargoCounterAvailable()
{ return installed; }

void InstallInstantCargoCounterHooks()
{
  auto resolve = [](const char* assemblyName, const char* ns, const char* name) -> Il2CppClass* {
    auto* domain   = il2cpp_domain_get();
    auto* assembly = domain ? il2cpp_domain_assembly_open(domain, assemblyName) : nullptr;
    auto* image    = assembly ? il2cpp_assembly_get_image(assembly) : nullptr;
    return image ? il2cpp_class_from_name(image, ns, name) : nullptr;
  };
  lerperClass         = resolve("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "ProgressDataLerper");
  auto* bar           = resolve("Assembly-CSharp", "Digit.Prime.UI", "SimpleProgressBarWidget");
  fleetInfoClass      = resolve("Assembly-CSharp", "Digit.Prime.FleetManagement", "FleetInfoWidget");
  cargoHoldClass      = resolve("Assembly-CSharp", "Digit.Prime.FleetManagement", "CargoHoldWidget");
  providerField       = ReferenceField(bar, "m_provider", "Digit.Client.UI.IDataContextProvider");
  lerperField         = ReferenceField(bar, "_lerper", "Digit.PrimeServer.Models.ProgressDataLerper");
  fillBarField        = ReferenceField(fleetInfoClass, "_cargoFillBar", "Digit.Client.UI.ProgressBarWidget");
  protectedBarField   = ReferenceField(cargoHoldClass, "_protectedCargo", "Digit.Prime.UI.SimpleProgressBarWidget");
  unprotectedBarField = ReferenceField(cargoHoldClass, "_unprotectedCargo", "Digit.Prime.UI.SimpleProgressBarWidget");
  oversizedBarField   = ReferenceField(cargoHoldClass, "_oversizedCargo", "Digit.Prime.UI.SimpleProgressBarWidget");
  const auto* start   = method_contract::Resolve(bar, "StartLerp", false, "System.Boolean", {});
  const auto* duration =
      method_contract::Resolve(lerperClass, "SetupDurationInternal", false, "System.Single",
                               {"Digit.PrimeServer.Models.IProgressData", "Digit.PrimeServer.Models.IProgressData"});
  if (!start || !duration || !providerField || !lerperField || !fillBarField || !protectedBarField
      || !unprotectedBarField || !oversizedBarField) {
    spdlog::warn("[InstantCargo] cargo widget contract unavailable; native timing retained");
    return;
  }
  const bool durationInstalled = SPUD_STATIC_DETOUR(duration->methodPointer, SetupDuration_Hook) != nullptr;
  installed = durationInstalled && SPUD_STATIC_DETOUR(start->methodPointer, StartLerp_Hook) != nullptr;
  spdlog::info("[InstantCargo] cargo counter hooks {}", installed ? "installed" : "unavailable");
  if (installed)
    InstallInstantCargoTextHooks();
}
