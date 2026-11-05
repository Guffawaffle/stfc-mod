#pragma once

// The actual fleet label/badge consumers share this fixture's managed runtime.
// Only configured hint text and native method bodies are supplied here; the
// real compact-chord formatter is independently covered by mod_logic.cc.
namespace fleet_presentation_shortcuts
{
inline std::string Hint(Action action)
{
  const auto index = std::to_underlying(action) - std::to_underlying(Action::kSelectShip1);
  return index >= 0 && index < 8 ? std::to_string(index + 1) : std::string{};
}
} // namespace fleet_presentation_shortcuts
#define Shortcut fleet_presentation_shortcuts
#include "../src/mod/features/ship_shortcut_badges.cc"
#undef Shortcut
#include "../src/mod/features/fleet_labels.cc"

namespace
{
struct FpFleet {
  Il2CppObject      header;
  int64_t           id    = 9007199254740993LL;
  bool              local = true;
  DeployedFleetType kind  = DeployedFleetType::Player;
};
struct FpLabel {
  Il2CppObject header;
  std::string  text      = "H";
  int          overrides = 0, clears = 0;
};
struct FpLod {
  Il2CppObject header;
  ZoomLevels   target = ZoomLevels::Far;
  ZoomLevels   seen   = ZoomLevels::None;
  int          calls  = 0;
};
struct FpWidget {
  Il2CppObject header;
  FpFleet*     fleet = nullptr;
  FpLod*       lod   = nullptr;
  FpLabel*     label = nullptr;
};
struct FpZoom {
  Il2CppObject header;
  NodeDepth    depth      = NodeDepth::SolarSystem;
  ZoomLevels   level      = ZoomLevels::Far;
  float        normalized = .5f;
};
struct FpSlot {
  Il2CppObject header;
  int32_t      index = 0;
};
struct FpArray {
  Il2CppObject        header;
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t length = 3;
  FpSlot*             values[3]{};
};
struct FpContext {
  Il2CppObject header;
  FpArray*     slots = nullptr;
};
struct FpBar {
  Il2CppObject header;
  FpContext*   context = nullptr;
};
Il2CppObject fp_manager{};
int32_t      fp_native_index = 7;
bool         fp_index_throws = false, fp_normalized_throws = false, fp_kind_throws = false, fp_write_throws = false;
int          fp_index_reads = 0;
std::function<void()> fp_on_index, fp_on_write, fp_on_normalized, fp_on_original;

int64_t FpId(FpFleet* fleet, const MethodInfo*)
{ return fleet->id; }
bool FpLocal(FpFleet* fleet, const MethodInfo*)
{ return fleet->local; }
DeployedFleetType FpKind(FpFleet* fleet, const MethodInfo*)
{
  if (fp_kind_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return fleet->kind;
}
FpFleet* FpGetContext(FpWidget* widget, const MethodInfo*)
{ return widget->fleet; }
void FpOverride(FpLabel* label, Il2CppString* text, const MethodInfo*)
{
  if (fp_write_throws)
    throw Il2CppExceptionWrapper(&thrown);
  ++label->overrides;
  label->text = il2cpp::ToUtf8(text);
  if (fp_on_write)
    std::exchange(fp_on_write, {})();
}
void FpClear(FpLabel* label, const MethodInfo*)
{
  ++label->clears;
  label->text = "H";
}
int32_t FpIndex(FpSlot* slot, const MethodInfo*)
{
  ++fp_index_reads;
  if (fp_on_index)
    std::exchange(fp_on_index, {})();
  if (fp_index_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return slot->index;
}
FpContext* FpBarContext(FpBar* bar, const MethodInfo*)
{ return bar->context; }
FpArray* FpSlots(FpContext* context, const MethodInfo*)
{ return context->slots; }
Il2CppObject* FpManager(const MethodInfo*)
{ return &fp_manager; }
int32_t FpPlayerIndex(FleetsManager*, int64_t, const MethodInfo*)
{ return fp_native_index; }
float FpNormalized(FpZoom* zoom, const MethodInfo*)
{
  if (fp_on_normalized)
    std::exchange(fp_on_normalized, {})();
  if (fp_normalized_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return zoom->normalized;
}
void FpOriginalLod(NavigationLOD* lod, ZoomLevels level)
{
  auto* object = reinterpret_cast<FpLod*>(lod);
  ++object->calls;
  object->seen   = level;
  object->target = ZoomLevels::None; // The hook must restore its own target after native work.
  if (fp_on_original)
    std::exchange(fp_on_original, {})();
}
void FpUpdateLod(NavigationLOD* lod, ZoomLevels level, const MethodInfo*)
{ NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod, level); }

void FleetPresentationCases(std::string_view mode)
{
  auto& deployed = Define("Digit.PrimeServer.Models", "FleetDeployedData", IL2CPP_TYPE_CLASS, &object_class);
  auto& kind     = Define("Digit.PrimeServer.Models", "DeployedFleetType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& level    = Define("Digit.Prime.Navigation", "ZoomLevels", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& depth    = Define("Digit.Prime.Navigation", "NodeDepth", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  kind.is_enum = level.is_enum = depth.is_enum = true;
  auto& lod_class = Define("Digit.Prime.Navigation", "NavigationLOD", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& widget_class =
      Define("Digit.Prime.Navigation", "NavigationFleetWidget", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& label_class   = Define("Digit.Client.UI", "TextLocalizer", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& zoom_class    = Define("Digit.Prime.Navigation", "NavigationZoom", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& manager_class = Define("Digit.Prime.FleetManagement", "FleetsManager", IL2CPP_TYPE_CLASS, &object_class);
  auto& bar_class     = Define("Digit.Prime.HUD", "FleetBarViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& context_class = Define("Digit.Prime.HUD", "FleetBarContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& array         = Define("Digit.PrimeServer.Models", "FleetPlayerData[]", IL2CPP_TYPE_SZARRAY, &array_class);
  array.klass.byval_arg.data.type  = T(fleet_data_class);
  lod_class.klass.instance_size    = sizeof(FpLod);
  widget_class.klass.instance_size = sizeof(FpWidget);
  zoom_class.klass.instance_size   = sizeof(FpZoom);
  AddField(lod_class, "<TargetLevel>k__BackingField", level, offsetof(FpLod, target));
  AddField(widget_class, "_lod", lod_class, offsetof(FpWidget, lod));
  AddField(widget_class, "_fleetLevel", label_class, offsetof(FpWidget, label));
  AddField(zoom_class, "_depth", depth, offsetof(FpZoom, depth));
  AddField(zoom_class, "_zoomLevel", mode == "fleet-presentation-missing-level" ? long_class : level,
           offsetof(FpZoom, level));
  AddMethod(deployed, "get_ID", reinterpret_cast<void*>(&FpId), T(long_class), {});
  AddMethod(deployed, "get_IsLocalPlayer", reinterpret_cast<void*>(&FpLocal), T(bool_class), {});
  AddMethod(deployed, "get_FleetType", reinterpret_cast<void*>(&FpKind), T(kind), {});
  auto& context_getter =
      AddMethod(widget_class, "get_Context", reinterpret_cast<void*>(&FpGetContext), T(deployed), {});
  AddProperty(widget_class, "Context", &context_getter);
  AddMethod(label_class, "OverrideLocalizedText", reinterpret_cast<void*>(&FpOverride), T(void_class),
            {T(string_class)});
  AddMethod(label_class, "ClearTextOverride", reinterpret_cast<void*>(&FpClear), T(void_class), {});
  AddMethod(fleet_data_class, "get_Index", reinterpret_cast<void*>(&FpIndex),
            mode == "fleet-presentation-missing-index" ? T(long_class) : T(int_class), {});
  auto& bar_getter =
      AddMethod(bar_class, "get_CanvasContext", reinterpret_cast<void*>(&FpBarContext), T(context_class), {});
  AddProperty(bar_class, "CanvasContext", &bar_getter);
  auto& slots_getter = AddMethod(context_class, "get_FleetData", reinterpret_cast<void*>(&FpSlots), T(array), {});
  AddProperty(context_class, "FleetData", &slots_getter);
  auto& manager_getter = AddMethod(manager_class, "get_Instance", reinterpret_cast<void*>(&FpManager), T(manager_class),
                                   {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(manager_class, "Instance", &manager_getter);
  AddMethod(manager_class, "GetPlayerFleetIndex", reinterpret_cast<void*>(&FpPlayerIndex), T(int_class),
            {T(long_class)});
  AddMethod(lod_class, "UpdateLOD", reinterpret_cast<void*>(&FpUpdateLod), T(void_class), {T(level)});
  if (mode != "fleet-presentation-missing-zoom")
    AddMethod(zoom_class, "get_NormalizedZoom", reinterpret_cast<void*>(&FpNormalized), T(float_class), {});
  fp_manager.klass = &manager_class.klass;
  FpFleet fleet{{&deployed.klass}}, foreign{{&deployed.klass}};
  foreign.local = false;
  FpLabel  label{{&label_class.klass}}, replacement_label{{&label_class.klass}};
  FpLod    lod{{&lod_class.klass}}, foreign_lod{{&lod_class.klass}};
  FpWidget widget{{&widget_class.klass}, &fleet, &lod, &label};
  auto*    receiver = reinterpret_cast<NavigationFleetWidget*>(&widget);
  FpSlot  slot0{{&fleet_data_class.klass}, 0}, slot2{{&fleet_data_class.klass}, 2}, slot7{{&fleet_data_class.klass}, 7};
  FpArray slots{{&array.klass}, nullptr, 3, {&slot0, &slot2, &slot7}};
  FpContext context{{&context_class.klass}, &slots};
  FpBar     bar{{&bar_class.klass}, &context};
  fleet_native_widgets[&bar_class.klass] = {&bar.header};
  auto& config                           = Config::Get();
  config.control.hotkeys_enabled = config.patches.hotkeyhooks = config.graphics.ship_hotkey_badges = true;
  config.control.use_scopely_hotkeys                                                               = false;
  config.graphics.zoom_label_player     = {FleetLabelDetail::kNative, .5f};
  config.graphics.zoom_label_non_player = {FleetLabelDetail::kNative, .5f};
  ShipBadges().installed                = true;
  fleet_labels.MarkInstalled();
  ship_shortcut_badges::Bind(receiver);
  if (mode == "fleet-presentation-missing-index") {
    Require(label.overrides == 0 && fp_index_reads == 0 && label.text == "H",
            "unavailable Index metadata never impersonates legitimate dock zero or advertises a shortcut");
    ship_shortcut_badges::Release(receiver);
    return;
  }
  Require(label.text == "3", "real badge consumer maps native dock7 to third selectable position");
  if (mode == "fleet-presentation-native") {
    const auto writes = label.overrides;
    UpdateShipBadge(receiver);
    Require(label.overrides == writes, "unchanged actual badge consumer avoids redundant native writes");
    config.control.use_scopely_hotkeys = true;
    UpdateShipBadge(receiver);
    Require(label.text == "H" && label.clears == 1, "native hotkey mode returns the owned label to native text");
    config.control.use_scopely_hotkeys = false;
    UpdateShipBadge(receiver);
    fp_index_throws = true;
    UpdateShipBadge(receiver);
    fp_index_throws = false;
    Require(label.text == "H", "failed slot Index invocation clears the previous hint rather than guessing zero");
    UpdateShipBadge(receiver);
    fp_on_index = [&] {
      for (auto* object : {&slots.header, &slot0.header, &slot2.header, &slot7.header})
        Require(std::ranges::any_of(warp_roots, [object](const auto& root) { return root.second == object; }),
                "hint acquisition retains its entire acquired vector and every inspected member");
      std::swap(slots.values[0], slots.values[2]);
    };
    UpdateShipBadge(receiver);
    Require(label.text == "H", "in-place FleetData replacement cannot publish a hint from the old slot positions");
    std::swap(slots.values[0], slots.values[2]);
    UpdateShipBadge(receiver);
    fp_on_index = [&] { ship_shortcut_badges::Release(receiver); };
    UpdateShipBadge(receiver);
    Require(!ShipBadges().badges.contains(receiver) && label.text == "H",
            "release during native hint acquisition retires the old iterator/generation and cannot publish afterward");
    ship_shortcut_badges::Bind(receiver);
    int original_binds = 0;
    NavigationFleetWidget_OnDidBindContext_Hook(
        [&](auto*) {
          ++original_binds;
          Require(label.text == "H", "old badge is released before native binding replaces its label field");
          widget.label = &replacement_label;
        },
        receiver);
    Require(original_binds == 1 && label.text == "H" && replacement_label.text == "3",
            "new bind applies its own hint without clearing a replacement label as old-owner cleanup");
    ship_shortcut_badges::Release(receiver);
    fp_write_throws = true;
    ship_shortcut_badges::Bind(receiver);
    fp_write_throws = false;
    Require(ShipBadges().badges.at(receiver).applied.empty(), "failed text writes do not claim an applied hint");
    UpdateShipBadge(receiver);
    Require(replacement_label.text == "3", "failed actual native write remains retryable");
    ship_shortcut_badges::Release(receiver);
    fp_on_write = [&] { ship_shortcut_badges::Release(receiver); };
    ship_shortcut_badges::Bind(receiver);
    Require(!ShipBadges().badges.contains(receiver) && replacement_label.text == "H",
            "release from the native override callback cleans an in-flight override and retires its completion");
    widget.fleet = &foreign;
    ship_shortcut_badges::Bind(receiver);
    Require(!ShipBadges().badges.contains(receiver), "foreign-player native text remains untouched");
    widget.fleet    = &fleet;
    widget.label    = &label;
    fp_native_index = 0;
    ship_shortcut_badges::Bind(receiver);
    Require(label.text == "1", "legitimate manager and FleetData index zero remain usable");
    ship_shortcut_badges::Release(receiver);
  }
  FpZoom zoom{{&zoom_class.klass}}, other_zoom{{&zoom_class.klass}};
  auto*  camera       = reinterpret_cast<NavigationZoom*>(&zoom);
  auto*  other_camera = reinterpret_cast<NavigationZoom*>(&other_zoom);
  auto*  lod_receiver = reinterpret_cast<NavigationLOD*>(&lod);
  fleet_labels.Track(receiver);
  config.graphics.zoom_label_player = {FleetLabelDetail::kThreshold, .5f};
  FleetLabelsAfterZoom(camera);
  if (mode == "fleet-presentation-missing-zoom" || mode == "fleet-presentation-missing-level") {
    NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Far);
    Require(lod.seen == ZoomLevels::Far && lod.target == ZoomLevels::Far,
            "missing camera observations do not become zero or authorize an expanded label");
  } else {
    Require(mode == "fleet-presentation-native", "unknown fleet presentation fixture mode");
    Require(lod.seen == ZoomLevels::Middle && lod.target == ZoomLevels::Middle,
            "actual threshold consumer includes equality and restores its target after original");
    zoom.normalized = .6f;
    FleetLabelsAfterZoom(camera);
    Require(lod.seen == ZoomLevels::Far, "crossing past threshold compacts actual tracked labels");
    config.graphics.zoom_label_player.zoom_threshold = 0;
    zoom.normalized                                  = 0;
    FleetLabelsAfterZoom(camera);
    Require(lod.seen == ZoomLevels::Far, "threshold zero never expands even at real normalized zero");
    config.graphics.zoom_label_player.zoom_threshold = 1;
    FleetLabelsAfterZoom(camera);
    Require(lod.seen == ZoomLevels::Middle, "threshold one always expands");
    config.graphics.zoom_label_player.zoom_threshold = .5f;
    for (float invalid :
         {-.1f, 1.1f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
      zoom.normalized = invalid;
      FleetLabelsAfterZoom(camera);
      Require(lod.seen == ZoomLevels::Far,
              "invalid normalized observations retain native detail rather than forcing it");
    }
    zoom.normalized = 0;
    FleetLabelsAfterZoom(camera);
    Require(lod.seen == ZoomLevels::Middle, "legitimate normalized zero remains distinct from unavailable evidence");
    fp_normalized_throws = true;
    FleetLabelsAfterZoom(camera);
    fp_normalized_throws = false;
    Require(lod.seen == ZoomLevels::Far, "failed normalized invocation returns threshold labels to native detail");
    zoom.level = ZoomLevels::None;
    FleetLabelsAfterZoom(camera);
    NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Far);
    Require(lod.seen == ZoomLevels::Far, "invalid native zoom-level observation retires threshold authority");
    zoom.level        = ZoomLevels::Far;
    warp_root_failure = true;
    FleetLabelsAfterZoom(camera);
    warp_root_failure = false;
    NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Far);
    Require(lod.seen == ZoomLevels::Far, "failed camera custody cannot keep previous threshold authority");
    for (auto detail : {FleetLabelDetail::kExpanded, FleetLabelDetail::kCompact, FleetLabelDetail::kNative}) {
      config.graphics.zoom_label_player.detail = detail;
      FleetLabelsAfterZoom(camera);
      const auto expected = detail == FleetLabelDetail::kExpanded ? ZoomLevels::Middle : ZoomLevels::Far;
      Require(lod.seen == expected && lod.target == expected,
              "profile reload changes reach the real consumer, including final Native restoration");
    }
    config.graphics.zoom_label_player.detail     = FleetLabelDetail::kExpanded;
    config.graphics.zoom_label_non_player.detail = FleetLabelDetail::kCompact;
    for (auto native_kind :
         {DeployedFleetType::Nonexistent, DeployedFleetType::Marauder, DeployedFleetType::NpcInstantiated,
          DeployedFleetType::Sentinel, DeployedFleetType::Alliance, DeployedFleetType::Challenge}) {
      fleet.kind = native_kind;
      NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Near);
      Require(lod.seen == ZoomLevels::Far, "all non-player kinds, including legitimate zero, use their own profile");
    }
    fleet.kind     = DeployedFleetType::Player;
    fp_kind_throws = true;
    NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Near);
    fp_kind_throws = false;
    Require(lod.seen == ZoomLevels::Near, "unavailable fleet kind preserves native instead of guessing non-player");
    NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Near);
    Require(lod.seen == ZoomLevels::Near, "Expanded preserves native Near detail");
    fp_on_original = [&] {
      NavigationFleetWidget_OnAboutToReleaseContext_Hook([](auto*) {}, receiver);
      lod.target = ZoomLevels::Far;
    };
    NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Middle);
    Require(!fleet_labels.Revision(lod_receiver) && lod.target == ZoomLevels::Far,
            "native release retires the old LOD generation so it cannot overwrite the new owner's target");
    fleet_labels.Track(receiver);
    config.graphics.zoom_label_player = {FleetLabelDetail::kThreshold, .5f};
    zoom.normalized                   = 0;
    other_zoom.normalized             = 1;
    fp_on_normalized                  = [&] { FleetLabelsAfterZoom(other_camera); };
    FleetLabelsAfterZoom(camera);
    NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Far);
    Require(lod.seen == ZoomLevels::Far, "nested camera replacement retires the old normalized observation");
    FleetLabelsFollowDepth(other_camera, NodeDepth::Galaxy);
    NavigationLOD_UpdateLOD_Hook(FpOriginalLod, lod_receiver, ZoomLevels::Far);
    Require(lod.seen == ZoomLevels::Far, "leaving the observed system retires threshold authority");
  }
  NavigationFleetWidget_OnDisable_Hook([](auto*) {}, receiver);
  fleet_labels = FleetLabels{};
  ShipBadges().badges.clear();
  fleet_native_widgets.clear();
  Require(warp_roots.empty(), "fleet presentation retires every weak and operation-local managed handle");
  std::cout << "Fleet presentation: production badge/LOD callbacks, checked observations, native restoration and "
               "reentrant custody passed\n";
}
} // namespace
