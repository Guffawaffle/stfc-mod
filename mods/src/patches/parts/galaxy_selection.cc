#include <config.h>
#include <spdlog/spdlog.h>

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
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
bool installed = false;
#if defined(__APPLE__)
thread_local uint64_t tap_sequence = 0;
void ProcessTap(auto original, void* manager, Position position, void* empty_space, void* multiple, void* forced)
{
  ++tap_sequence;
  original(manager, position, empty_space, multiple, forced);
}
#endif

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
#if defined(__APPLE__)
  // Preserve the platform handler. Only forward if it did not dispatch a tap;
  // do not assume the Mac implementation has the Windows Far early return.
  const auto before = tap_sequence;
  original(manager, tap);
  if (tap_sequence == before) Invoke(manager, single_tap, args);
#else
  Invoke(manager, single_tap, args);
#endif
  // UI references stay alive; proxies only join the hit-test list during a tap.
  Deactivate();
}
}
#endif

void InstallGalaxySelectionHooks()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  auto cls = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationManager");
  auto* recognized = method_contract::Resolve(cls.get_cls(), "OnTapRecognised", false, "System.Void", {"TKTapRecognizer"});
  single_tap = method_contract::Resolve(cls.get_cls(), "OnSingleTap", false, "System.Void",
                                      {"UnityEngine.Vector2", "Digit.Prime.Navigation.POI"});
  auto* target = method_contract::Pointer(recognized);
  if (!target || !single_tap) {
    spdlog::error("[GalaxySelection] Required named methods were not found");
    return;
  }
#if defined(__APPLE__)
  // The only manually represented managed value is GalaxyNode. Validate its
  // layout on the loaded Mac client before passing it by reference.
  auto node = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "GalaxyNode");
  auto* node_cls = node.get_cls();
  auto* galaxy_field = node_cls ? il2cpp_class_get_field_from_name(node_cls, "_galaxy") : nullptr;
  auto* index_field = node_cls ? il2cpp_class_get_field_from_name(node_cls, "_index") : nullptr;
  uint32_t alignment = 0;
  if (!node_cls || !galaxy_field || !index_field
      || il2cpp_class_value_size(node_cls, &alignment) != sizeof(galaxy_selection::Node)
      || (galaxy_field->offset != 0 && galaxy_field->offset != sizeof(Il2CppObject))
      || index_field->offset != galaxy_field->offset + sizeof(void*)
      || !method_contract::Type(galaxy_field->type, "Digit.PrimeServer.Models.OptimisedGalaxy")
      || !method_contract::Type(index_field->type, "System.Int32")) {
    spdlog::warn("[GalaxySelection] Mac GalaxyNode layout validation failed; using native selection");
    return;
  }
  auto* process = method_contract::Resolve(cls.get_cls(), "ProcessInputTap", false, "System.Void",
      {"UnityEngine.Vector2", "System.Action<UnityEngine.Vector3>",
       "System.Action<System.Collections.Generic.List<Digit.Prime.Navigation.POI>>", "Digit.Prime.Navigation.POI"});
  auto* process_target = method_contract::Pointer(process);
  if (!process_target) {
    spdlog::warn("[GalaxySelection] Mac method resolution failed; using native selection");
    return;
  }
  if (!SPUD_STATIC_DETOUR(process_target, ProcessTap)) return;
#endif
  installed = SPUD_STATIC_DETOUR(target, RecognizedTap);
  spdlog::info("[GalaxySelection] hook installed={}", installed);
#else
  if (Config::Get().galaxy_extended_selection)
    spdlog::warn("[GalaxySelection] Unsupported platform; using native selection");
#endif
}

namespace mod_settings {
bool GalaxyExtendedSelectionAvailable()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  return installed;
#else
  return false;
#endif
}
}
