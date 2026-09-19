#include <config.h>
#include <spdlog/spdlog.h>

#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
#include <chrono>
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <spud/detour.h>
#include "galaxy_selection.h"

namespace
{
struct Position { float x, y; };
static_assert(sizeof(Position) == 8);
const MethodInfo* single_tap = nullptr;

void RecognizedTap(auto original, Il2CppObject* manager, Il2CppObject* tap)
{
  using namespace galaxy_selection;
  if (!Config::Get().galaxy_extended_selection) { original(manager, tap); return; }
  auto* zoom = Call(Call(manager, "get_NavigationCamera"), "get_NavZoomCamera");
  auto* field = zoom ? il2cpp_class_get_field_from_name(zoom->klass, "_depth") : nullptr;
  auto* tier = Call(zoom, "get_CurrentZoomLevel");
  int depth = 0;
  if (field && method_contract::Type(field->type, "Digit.PrimeServer.Models.NodeDepth"))
    il2cpp_field_get_value(zoom, field, &depth);
  if (depth != 1 || !tier || *static_cast<int*>(il2cpp_object_unbox(tier)) != 3) {
    original(manager, tap); return;
  }
  auto* location = Call(tap, "touchLocation");
  if (!location || !method_contract::Type(il2cpp_class_get_type(location->klass), "UnityEngine.Vector2")) {
    original(manager, tap); return;
  }
  const auto position = *static_cast<Position*>(il2cpp_object_unbox(location));
  Prepare(manager, position.x, position.y);
  // Only replace the inspected Far-tier early return. Native hit testing,
  // candidate filtering and preview dispatch still run once via OnSingleTap.
  void* args[]{const_cast<Position*>(&position), nullptr};
  Invoke(manager, single_tap, args);
  // UI references stay alive; proxies only join the hit-test list during a tap.
  Deactivate();
}
}
#endif

void InstallGalaxySelectionHooks()
{
#if defined(_WIN32) && defined(_M_X64)
  auto cls = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationManager");
  auto* recognized = method_contract::Resolve(cls.get_cls(), "OnTapRecognised", false, "System.Void", {"TKTapRecognizer"});
  single_tap = method_contract::Resolve(cls.get_cls(), "OnSingleTap", false, "System.Void",
                                      {"UnityEngine.Vector2", "Digit.Prime.Navigation.POI"});
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"GameAssembly.dll"));
  auto* target = method_contract::Pointer(recognized);
  // Client 262 Win64: inspected body 0x12718e0..0x1271968, ample detour
  // extent. Other clients/architectures require independent native validation.
  if (!base || reinterpret_cast<uintptr_t>(target) != base + 0x12718e0
      || reinterpret_cast<uintptr_t>(method_contract::Pointer(single_tap)) != base + 0x1271e80) {
    if (Config::Get().galaxy_extended_selection)
      spdlog::warn("[GalaxySelection] Unsupported client; using native selection");
    return;
  }
  const bool installed = SPUD_STATIC_DETOUR(target, RecognizedTap);
  spdlog::info("[GalaxySelection] hook installed={}", installed);
#else
  if (Config::Get().galaxy_extended_selection)
    spdlog::warn("[GalaxySelection] Unsupported platform; using native selection");
#endif
}
