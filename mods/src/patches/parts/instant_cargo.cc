#include "config.h"
#include "settings/preview_settings.h"
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

namespace
{
Il2CppClass* cargoProgressClass{};
bool         installed = false;

float SetupDuration_Hook(auto original, void* self, Il2CppObject* source, Il2CppObject* target)
{
  // The native zero-duration path selects the final value and completes normally.
  // Match the target, so switching from another ship's interpolated value is instant too.
  // Other progress data (health, jobs, rewards, etc.) keeps its original timing.
  if (Config::Get().instant_cargo_counter && target && il2cpp_object_get_class(target) == cargoProgressClass)
    return 0.0f;
  return original(self, source, target);
}
} // namespace

bool mod_settings::InstantCargoCounterAvailable()
{ return installed; }

void InstallInstantCargoCounterHooks()
{
  auto* domain       = il2cpp_domain_get();
  auto* assembly     = domain ? il2cpp_domain_assembly_open(domain, "Digit.Client.PrimeLib.Runtime") : nullptr;
  auto* image        = assembly ? il2cpp_assembly_get_image(assembly) : nullptr;
  cargoProgressClass = image ? il2cpp_class_from_name(image, "Digit.PrimeServer.Models", "CargoProgressData") : nullptr;
  auto* lerper = image ? il2cpp_class_from_name(image, "Digit.PrimeServer.Models", "ProgressDataLerper") : nullptr;
  const auto* method =
      method_contract::Resolve(lerper, "SetupDurationInternal", false, "System.Single",
                               {"Digit.PrimeServer.Models.IProgressData", "Digit.PrimeServer.Models.IProgressData"});
  if (!cargoProgressClass || !method) {
    spdlog::warn("[InstantCargo] cargo interpolation contract unavailable; native timing retained");
    return;
  }
  installed = SPUD_STATIC_DETOUR(method->methodPointer, SetupDuration_Hook) != nullptr;
  spdlog::info("[InstantCargo] cargo counter hook {}", installed ? "installed" : "unavailable");
}
