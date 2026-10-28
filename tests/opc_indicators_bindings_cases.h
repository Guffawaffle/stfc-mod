#pragma once

// Actual OPC producer, renderers and lifecycle callbacks against the shared
// controlled runtime. Native Unity method bodies and existing scene objects are
// supplied here; no game, allocation/layout engine or player settings file runs.
#include "../src/mod/features/fleet_opc_sample.cc"
#include "../src/mod/features/opc_indicators.cc"

namespace
{
struct OpcNativeRecord {
  Il2CppObject                                          header;
  OpcNativeRecord *                                     transform = nullptr, *object = nullptr, *parent = nullptr;
  std::vector<std::pair<std::string, OpcNativeRecord*>> children;
  std::vector<OpcNativeRecord*>                         components;
  OpcNativeRecord *fleet = nullptr, *timer = nullptr, *panel = nullptr, *cargo = nullptr, *progress = nullptr,
                  *mining           = nullptr;
  bool                       active = true, has_ship = true, raycast = true, destroyed = false;
  int32_t                    slot = 0;
  int64_t                    id = 1001, amount = 2;
  FleetState                 state   = FleetState::Mining;
  double                     current = 99, protection = 100, mined = 0;
  float                      rate = 1, font_size = 0;
  unity::TMP_Text::Alignment alignment = unity::TMP_Text::Alignment::Left;
  Il2CppString*              text      = nullptr;
  unity::Vector2             position{};
  unity::Color               color{};
};
std::deque<OpcNativeRecord>      opc_native_records;
std::deque<Il2CppReflectionType> opc_native_types;
bool                             opc_native_text_failure = false, opc_native_cargo_failure = false;
bool                             opc_native_color_failure = false;
int                              opc_native_style_failure = 0, opc_native_destroys = 0;
int                              opc_native_cargo_reads = 0, opc_native_text_writes = 0, opc_native_fleet_reads = 0;

FakeClass& OpcNativeClass(const char* ns, const char* name, FakeClass* parent = &object_class)
{
  const auto found = std::ranges::find_if(classes, [=](const auto& item) {
    return std::string_view(item.klass.namespaze) == ns && std::string_view(item.klass.name) == name;
  });
  return found == classes.end() ? Define(ns, name, IL2CPP_TYPE_CLASS, parent) : *found;
}
OpcNativeRecord& OpcNativeMake(FakeClass& klass)
{
  auto& record              = opc_native_records.emplace_back();
  record.header.klass       = &klass.klass;
  klass.klass.instance_size = sizeof(OpcNativeRecord);
  return record;
}
OpcNativeRecord* OpcNative(Il2CppObject* object)
{ return reinterpret_cast<OpcNativeRecord*>(object); }
Il2CppObject* OpcNativeType(const Il2CppType* type)
{
  auto& object        = opc_native_types.emplace_back();
  object.object.klass = &type_class.klass;
  object.type         = type;
  return &object.object;
}
unity::Transform* OpcNativeTransform(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<unity::Transform*>(OpcNative(object)->transform); }
unity::GameObject* OpcNativeObject(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<unity::GameObject*>(OpcNative(object)->object); }
unity::Transform* OpcNativeParent(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<unity::Transform*>(OpcNative(object)->parent); }
unity::Transform* OpcNativeFind(Il2CppObject* object, Il2CppString* name, const MethodInfo*)
{
  for (const auto& [key, child] : OpcNative(object)->children)
    if (il2cpp::Equals(name, key))
      return reinterpret_cast<unity::Transform*>(child);
  return nullptr;
}
Il2CppObject* OpcNativeComponent(Il2CppObject* object, Il2CppReflectionType* type, const MethodInfo*)
{
  auto* klass = il2cpp_class_from_type(type->type);
  for (auto* component : OpcNative(object)->components)
    if (il2cpp_class_is_assignable_from(klass, component->header.klass))
      return &component->header;
  return nullptr;
}
void OpcNativeActive(Il2CppObject* object, bool value, const MethodInfo*)
{ OpcNative(object)->active = value; }
bool OpcNativeEnabled(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->active; }
void OpcNativePosition(Il2CppObject* object, unity::Vector2 value, const MethodInfo*)
{ OpcNative(object)->position = value; }
void OpcNativeVector(Il2CppObject*, unity::Vector2, const MethodInfo*) {}
void OpcNativeSibling(Il2CppObject*, const MethodInfo*) {}
void OpcNativeColor(Il2CppObject* object, unity::Color value, const MethodInfo*)
{
  if (opc_native_color_failure)
    throw Il2CppExceptionWrapper(&thrown);
  OpcNative(object)->color = value;
}
void OpcNativeFontSize(Il2CppObject* object, float value, const MethodInfo*)
{
  if (opc_native_style_failure == 1)
    throw Il2CppExceptionWrapper(&thrown);
  OpcNative(object)->font_size = value;
}
void OpcNativeAlignment(Il2CppObject* object, unity::TMP_Text::Alignment value, const MethodInfo*)
{
  if (opc_native_style_failure == 2)
    throw Il2CppExceptionWrapper(&thrown);
  OpcNative(object)->alignment = value;
}
void OpcNativeRaycast(Il2CppObject* object, bool value, const MethodInfo*)
{
  if (opc_native_style_failure == 3)
    throw Il2CppExceptionWrapper(&thrown);
  OpcNative(object)->raycast = value;
}
void OpcNativeDestroy(Il2CppObject* object, const MethodInfo*)
{
  ++opc_native_destroys;
  OpcNative(object)->destroyed = true;
  OpcNative(object)->active    = false;
}
Il2CppString* OpcNativeText(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->text; }
void OpcNativeSetText(Il2CppObject* object, Il2CppString* value, const MethodInfo*)
{
  ++opc_native_text_writes;
  if (opc_native_text_failure)
    throw Il2CppExceptionWrapper(&thrown);
  OpcNative(object)->text = value;
}
FleetPlayerData* OpcNativeFleet(Il2CppObject* object, const MethodInfo*)
{
  ++opc_native_fleet_reads;
  return reinterpret_cast<FleetPlayerData*>(OpcNative(object)->fleet);
}
int32_t OpcNativeSlot(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->slot; }
int64_t OpcNativeId(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->id; }
FleetState OpcNativeState(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->state; }
bool OpcNativeHasShip(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->has_ship; }
bool OpcNativeSelected(Il2CppObject*, int32_t slot, const MethodInfo*)
{ return slot == 0; }
fleet_opc_binding::Hold* OpcNativeCargo(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<fleet_opc_binding::Hold*>(OpcNative(object)->cargo); }
fleet_opc_binding::Progress* OpcNativeProgress(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<fleet_opc_binding::Progress*>(OpcNative(object)->progress); }
double OpcNativeCurrent(Il2CppObject* object, const MethodInfo*)
{
  ++opc_native_cargo_reads;
  Require(std::ranges::any_of(warp_roots, [object](const auto& root) { return root.second == object; }),
          "production cargo reader roots progress while observing its values");
  if (opc_native_cargo_failure)
    throw Il2CppExceptionWrapper(&thrown);
  return OpcNative(object)->current;
}
double OpcNativeProtection(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->protection; }
fleet_opc_binding::Mining* OpcNativeMining(Il2CppObject* object, const MethodInfo*)
{ return reinterpret_cast<fleet_opc_binding::Mining*>(OpcNative(object)->mining); }
float OpcNativeRate(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->rate; }
double OpcNativeMined(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->mined; }
int64_t OpcNativeAmount(Il2CppObject* object, const MethodInfo*)
{ return OpcNative(object)->amount; }

void OpcIndicatorsConsumerCases(std::string_view mode)
{
  type_object_override = OpcNativeType;
  Opc()                = {};
  opc_sample_cache     = {};
  auto& behaviour      = OpcNativeClass("UnityEngine", "Behaviour", &component_class);
  auto& transform      = OpcNativeClass("UnityEngine", "Transform", &component_class);
  auto& rect           = OpcNativeClass("UnityEngine", "RectTransform", &transform);
  auto& tmp            = OpcNativeClass("TMPro", "TMP_Text", &behaviour);
  auto& label          = OpcNativeClass("TMPro", "TextMeshProUGUI", &tmp);
  auto& image          = OpcNativeClass("UnityEngine.UI", "Image", &behaviour);
  auto& local          = OpcNativeClass("Digit.Prime.Ships", "FleetLocalViewController", &behaviour);
  auto& bar            = OpcNativeClass("Digit.Prime.HUD", "FleetBarViewController", &behaviour);
  auto& state_widget   = OpcNativeClass("Digit.Prime.HUD", "FleetStateWidget", &behaviour);
  auto& flag_widget    = OpcNativeClass("Digit.Prime.HUD", "FleetbarFlagWidget", &behaviour);
  auto& timer          = OpcNativeClass("Digit.Client.UI", "TimerWidget", &behaviour);
  auto& cargo          = OpcNativeClass("Digit.PrimeServer.Models", "CargoHoldData");
  auto& progress       = OpcNativeClass("Digit.PrimeServer.Models", "ProgressData");
  auto& mining         = OpcNativeClass("Digit.PrimeServer.Models", "IMiningSlot");
  auto& mining_impl    = OpcNativeClass("Digit.PrimeServer.Models", "FleetMiningSlot", &mining);
  auto& scalar         = Define("System", "Double", IL2CPP_TYPE_R8, nullptr, sizeof(double));
  auto& vector         = Define("UnityEngine", "Vector2", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Vector2));
  auto& color          = Define("UnityEngine", "Color", IL2CPP_TYPE_VALUETYPE, nullptr, sizeof(unity::Color));
  auto& fleet_state    = Define("Digit.PrimeServer.Models", "FleetState", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  fleet_state.is_enum  = true;
  const auto property  = [](FakeClass& owner, const char* name, const char* getter, void* body, FakeClass& result) {
    auto& method = AddMethod(owner, getter, body, T(result), {});
    AddProperty(owner, name, &method);
  };
  property(component_class, "transform", "get_transform", reinterpret_cast<void*>(&OpcNativeTransform), transform);
  property(game_object_class, "transform", "get_transform", reinterpret_cast<void*>(&OpcNativeTransform), transform);
  property(transform, "gameObject", "get_gameObject", reinterpret_cast<void*>(&OpcNativeObject), game_object_class);
  property(transform, "parent", "get_parent", reinterpret_cast<void*>(&OpcNativeParent), transform);
  property(behaviour, "isActiveAndEnabled", "get_isActiveAndEnabled", reinterpret_cast<void*>(&OpcNativeEnabled),
           bool_class);
  AddMethod(transform, "Find", reinterpret_cast<void*>(&OpcNativeFind), T(transform), {T(string_class)});
  AddMethod(game_object_class, "SetActive", reinterpret_cast<void*>(&OpcNativeActive), T(void_class), {T(bool_class)});
  AddMethod(unity_object_class, "Destroy", reinterpret_cast<void*>(&OpcNativeDestroy), T(void_class),
            {T(unity_object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  for (auto* method : game_object_class.methods)
    if (std::string_view(method->name) == "GetComponent") {
      auto* changed          = const_cast<MethodInfo*>(method);
      changed->methodPointer = changed->virtualMethodPointer =
          reinterpret_cast<Il2CppMethodPointer>(&OpcNativeComponent);
      changed->return_type = T(component_class);
    }
  AddMethod(rect, "set_anchorMin", reinterpret_cast<void*>(&OpcNativeVector), T(void_class), {T(vector)});
  AddMethod(rect, "set_anchorMax", reinterpret_cast<void*>(&OpcNativeVector), T(void_class), {T(vector)});
  AddMethod(rect, "set_pivot", reinterpret_cast<void*>(&OpcNativeVector), T(void_class), {T(vector)});
  AddMethod(rect, "set_sizeDelta", reinterpret_cast<void*>(&OpcNativeVector), T(void_class), {T(vector)});
  AddMethod(rect, "set_anchoredPosition", reinterpret_cast<void*>(&OpcNativePosition), T(void_class), {T(vector)});
  AddMethod(transform, "SetAsLastSibling", reinterpret_cast<void*>(&OpcNativeSibling), T(void_class), {});
  AddMethod(tmp, "set_color", reinterpret_cast<void*>(&OpcNativeColor), T(void_class), {T(color)});
  AddMethod(tmp, "set_fontSize", reinterpret_cast<void*>(&OpcNativeFontSize), T(void_class), {T(float_class)});
  AddMethod(tmp, "set_alignment", reinterpret_cast<void*>(&OpcNativeAlignment), T(void_class), {T(int_class)});
  AddMethod(tmp, "set_raycastTarget", reinterpret_cast<void*>(&OpcNativeRaycast), T(void_class), {T(bool_class)});
  AddMethod(image, "set_color", reinterpret_cast<void*>(&OpcNativeColor), T(void_class), {T(color)});
  AddMethod(image, "set_raycastTarget", reinterpret_cast<void*>(&OpcNativeRaycast), T(void_class), {T(bool_class)});
  AddMethod(tmp, "get_text", reinterpret_cast<void*>(&OpcNativeText), T(string_class), {});
  AddMethod(tmp, "set_text", reinterpret_cast<void*>(&OpcNativeSetText), T(void_class), {T(string_class)});
  AddMethod(local, "get_fleet", reinterpret_cast<void*>(&OpcNativeFleet), T(fleet_data_class), {});
  AddMethod(state_widget, "get_Context", reinterpret_cast<void*>(&OpcNativeFleet), T(fleet_data_class), {});
  AddMethod(flag_widget, "get_Context", reinterpret_cast<void*>(&OpcNativeFleet), T(fleet_data_class), {});
  AddMethod(bar, "IsIndexSelected", reinterpret_cast<void*>(&OpcNativeSelected), T(bool_class), {T(int_class)});
  AddField(local, "_timerWidget", timer, offsetof(OpcNativeRecord, timer));
  AddField(bar, "_fleetPanelController", local, offsetof(OpcNativeRecord, panel));
  AddMethod(fleet_data_class, "get_Index", reinterpret_cast<void*>(&OpcNativeSlot), T(int_class), {});
  for (auto* method : fleet_data_class.methods)
    if (std::string_view(method->name) == "get_Id") {
      auto* changed          = const_cast<MethodInfo*>(method);
      changed->methodPointer = changed->virtualMethodPointer = reinterpret_cast<Il2CppMethodPointer>(&OpcNativeId);
    }
  AddMethod(fleet_data_class, "get_CurrentState", reinterpret_cast<void*>(&OpcNativeState), T(fleet_state), {});
  AddMethod(fleet_data_class, "get_HasShip", reinterpret_cast<void*>(&OpcNativeHasShip), T(bool_class), {});
  AddMethod(fleet_data_class, "get_CargoHoldData", reinterpret_cast<void*>(&OpcNativeCargo), T(cargo), {});
  AddMethod(cargo, "get_UnprotectedCargoProgress", reinterpret_cast<void*>(&OpcNativeProgress), T(progress), {});
  AddMethod(progress, "get_CurrentValue", reinterpret_cast<void*>(&OpcNativeCurrent), T(scalar), {});
  AddMethod(progress, "get_MinValue", reinterpret_cast<void*>(&OpcNativeProtection), T(scalar), {});
  AddMethod(fleet_data_class, "get_MiningData", reinterpret_cast<void*>(&OpcNativeMining), T(mining), {});
  for (auto* klass : {&mining, &mining_impl}) {
    AddMethod(*klass, "get_MiningSpeed", reinterpret_cast<void*>(&OpcNativeRate), T(float_class), {},
              METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
    AddMethod(*klass, "get_AmountMined", reinterpret_cast<void*>(&OpcNativeMined), T(scalar), {},
              METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
    AddMethod(*klass, "get_Amount", reinterpret_cast<void*>(&OpcNativeAmount), T(long_class), {},
              METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_VIRTUAL);
  }

  const auto visual = [&](const char* name, OpcNativeRecord* parent = nullptr) -> OpcNativeRecord& {
    auto& object       = OpcNativeMake(game_object_class);
    auto& position     = OpcNativeMake(rect);
    position.object    = &object;
    object.transform   = &position;
    position.transform = &position;
    position.parent    = parent;
    if (parent)
      parent->children.emplace_back(name, &position);
    return position;
  };
  const auto component = [&](FakeClass& klass, OpcNativeRecord& position) -> OpcNativeRecord& {
    auto& object     = OpcNativeMake(klass);
    object.transform = &position;
    position.object->components.push_back(&object);
    return object;
  };
  auto& fleet           = OpcNativeMake(fleet_data_class);
  fleet.cargo           = &OpcNativeMake(cargo);
  fleet.cargo->progress = &OpcNativeMake(progress);
  fleet.mining          = &OpcNativeMake(mining_impl);
  auto* native_fleet    = reinterpret_cast<FleetPlayerData*>(&fleet);
  auto& bar_transform   = visual("bar");
  auto& native_bar      = component(bar, bar_transform);
  auto& tile_transform  = visual("tile", &bar_transform);
  auto& native_tile     = component(local, tile_transform);
  native_tile.fleet     = &fleet;
  auto& body            = visual("BodyContainer", &tile_transform);
  auto& highlight       = visual("CommunityOPCHighlight", &body);
  auto& eta_label       = visual("CommunityOpcEtaLabel", &body);
  auto& native_text     = component(label, eta_label);
  auto& backing         = visual("CommunityOpcEtaBackground", &body);
  auto& backing_image   = component(image, backing);
  auto& state_transform = visual("state", &tile_transform);
  auto& native_state    = component(state_widget, state_transform);
  native_state.fleet    = &fleet;
  auto& flag_transform  = visual("flag", &tile_transform);
  auto& native_flag     = component(flag_widget, flag_transform);
  native_flag.fleet     = &fleet;
  auto& panel_transform = visual("panel", &bar_transform);
  auto& native_panel    = component(local, panel_transform);
  native_panel.fleet    = &fleet;
  native_bar.panel      = &native_panel;
  auto& timer_transform = visual("timer", &panel_transform);
  native_panel.timer    = &component(timer, timer_transform);
  auto& card_label      = visual("CommunityOpcCardEtaLabel", &timer_transform);
  auto& card_text       = component(label, card_label);
  auto* body_pointer    = reinterpret_cast<unity::Transform*>(&body);
  auto* panel_pointer   = reinterpret_cast<FleetLocalViewController*>(&native_panel);
  auto* state_pointer   = reinterpret_cast<fleet_opc_binding::StateWidget*>(&native_state);
  auto* flag_pointer    = reinterpret_cast<fleet_opc_binding::FlagWidget*>(&native_flag);
  auto& ui              = Config::Get().ui;
  Require(!ui.highlight_opc_fleets && !ui.fleet_hud_opc_eta, "both independent OPC settings default off");
  Opc().installed = true;

  if (mode == "opc-consumer-sample") {
    const auto roots = warp_roots.size();
    Require(fleet_opc::Read(native_fleet, 0, 1001, 4).known && opc_native_cargo_reads == 1,
            "actual typed producer reads rooted native cargo progress");
    fleet.cargo->progress->current = 101;
    Require(!fleet_opc::Read(native_fleet, 0, 1001, 4).opc && opc_native_cargo_reads == 1,
            "another consumer shares the same bounded cargo observation");
    fleet_opc::Invalidate(0);
    Require(fleet_opc::Observe(native_fleet, 0, 1001, 4).cargo.opc && opc_native_cargo_reads == 2,
            "cargo-event invalidation reaches the actual producer before ETA observation");
    fleet.cargo->progress->current = 99;
    fleet_opc::Invalidate(0);
    const auto status = fleet_opc::Observe(native_fleet, 0, 1001, 4);
    Require(status.rate_known && status.node_known && status.eta_seconds == 1 && !status.safe_on_node,
            "actual IMiningSlot dispatch supplies native rate and node progress");
    opc_native_cargo_failure = true;
    fleet_opc::Invalidate(0);
    Require(!fleet_opc::Observe(native_fleet, 0, 1001, 4).cargo.known,
            "contained cargo observation failure does not invent protected or OPC state");
    opc_native_cargo_failure = false;
    Require(!fleet_opc::Read(native_fleet, -1, 1001, 4).known && warp_roots.size() == roots,
            "invalid slots refuse sampling and every temporary producer root is released");
  } else if (mode == "opc-consumer-render") {
    auto* text_pointer  = reinterpret_cast<unity::TextMeshProUGUI*>(&native_text);
    auto* image_pointer = reinterpret_cast<unity::Image*>(&backing_image);
    for (const auto failure : {1, 2, 3}) {
      opc_native_style_failure = failure;
      Require(!OpcConfigureLabel(text_pointer), "required native label style failure cannot qualify initial setup");
    }
    opc_native_style_failure = 0;
    Require(OpcConfigureLabel(text_pointer) && native_text.font_size == 18
                && native_text.alignment == unity::TMP_Text::Alignment::Center && !native_text.raycast,
            "production initial label setup establishes readable, noninteractive source styling");
    opc_native_color_failure = true;
    Require(!OpcConfigureBacking(image_pointer), "native backing color failure cannot qualify initial setup");
    opc_native_color_failure = false;
    opc_native_style_failure = 3;
    Require(!OpcConfigureBacking(image_pointer), "native backing raycast failure cannot qualify initial setup");
    opc_native_style_failure = 0;
    Require(OpcConfigureBacking(image_pointer) && !backing_image.raycast && backing_image.color.a == .86f,
            "production initial backing setup preserves the source color without intercepting input");
    const auto status = fleet_opc::Evaluate(true, {true, false, 99, 100}, true, 1, true, 0, 1);
    Require(OpcRenderEta(body_pointer, true, status) && il2cpp::Equals(native_text.text, "<b>SAFE</b>")
                && eta_label.position.y == 24 && backing.position.y == 25 && native_text.color.b == 1,
            "production selected ETA consumer renders SAFE with its source layout and safe color");
    Require(OpcRenderEta(body_pointer, false, fleet_opc::Evaluate(true, {true, true, 101, 100}, true, 1, true, 0, 1))
                && il2cpp::Equals(native_text.text, "<b>OPC</b>") && eta_label.position.y == 14,
            "production ordinary tile renders OPC and changes selected layout without rebuilding the model");
    Require(OpcRenderCard(panel_pointer, "OPC: Now") && il2cpp::Equals(card_text.text, "OPC: Now")
                && card_label.position.x == 7.6f && card_label.position.y == -39,
            "actual selected card consumer uses its native timer anchor and source placement");
    opc_native_text_failure = true;
    Require(!OpcRenderEta(body_pointer, false, status) && !eta_label.object->active && !backing.object->active,
            "contained native text failure leaves both ETA surfaces hidden and reports setup failure");
    Require(!OpcRenderCard(panel_pointer, "OPC: 1m") && !card_label.object->active,
            "failed selected card text cannot publish a successful stale card");
    opc_native_text_failure = false;
    Require(OpcRenderEta(body_pointer, false, {}) && !eta_label.object->active && !backing.object->active,
            "nonmining or unknown data hides ETA instead of rendering invented status");
    ui.highlight_opc_fleets = ui.fleet_hud_opc_eta = true;
    fleet.cargo->progress->current                 = 101;
    fleet_opc::Invalidate(0);
    OpcBind(state_pointer, OpcOwner::State);
    Require(highlight.object->active && eta_label.object->active && card_label.object->active
                && il2cpp::Equals(native_text.text, "<b>OPC</b>") && il2cpp::Equals(card_text.text, "OPC: Now"),
            "actual view refresh connects rooted producer to highlight, ETA and selected-card consumers");
    Opc().views.at(state_pointer).retry = OpcNow() + 30000;
    fleet.id                            = 1002;
    fleet.cargo->progress->current      = 99;
    OpcRefresh(state_pointer);
    Require(Opc().views.at(state_pointer).retry == 0 && !highlight.object->active
                && il2cpp::Equals(native_text.text, "<b><1m</b>"),
            "a changed fleet identity clears old setup backoff and renders its fresh native cargo");
    auto* tile_pointer = reinterpret_cast<FleetLocalViewController*>(&native_tile);
    int   originals    = 0;
    FleetLocalViewController_BindDataContext_OpcHook(
        [&](auto* receiver, auto* provider, auto* context) {
          Require(receiver == tile_pointer && !provider && !context,
                  "native bind preserves its receiver and references");
          ++originals;
        },
        tile_pointer, nullptr, nullptr);
    const auto cargo_reads = opc_native_cargo_reads;
    FleetLocalViewController_OnCurrentCargoReactiveEvent_OpcHook(
        [&](auto* receiver, int32_t flags) {
          Require(receiver == tile_pointer && flags == 23,
                  "native cargo event keeps its exact receiver and dirty flags");
          ++originals;
          fleet.cargo->progress->current = 101;
        },
        tile_pointer, 23);
    Require(originals == 2 && opc_native_cargo_reads == cargo_reads + 1 && highlight.object->active
                && il2cpp::Equals(native_text.text, "<b>OPC</b>"),
            "actual cargo callback runs native first, invalidates within the TTL, then refreshes the same consumers");
    card_text.text          = il2cpp::NewString("stale card");
    opc_native_text_failure = true;
    OpcRefresh(state_pointer);
    Require(Opc().views.at(state_pointer).retry > OpcNow() && highlight.object->active && !card_label.object->active,
            "native card setup failure schedules backoff while independent valid highlight remains visible");
    opc_native_text_failure        = false;
    fleet.cargo->progress->current = 99;
    fleet_opc::Invalidate(0);
    OpcRefresh(state_pointer);
    Require(Opc().views.at(state_pointer).retry > OpcNow() && !highlight.object->active && !eta_label.object->active
                && !backing.object->active && !card_label.object->active,
            "changed nonempty ETA during card backoff hides mismatched prior text without attempting setup");
    fleet.state = FleetState::Docked;
    OpcRefresh(state_pointer);
    Require(!highlight.object->active && !eta_label.object->active && !card_label.object->active
                && Opc().views.at(state_pointer).retry == 0,
            "docking during setup backoff still hides invalid artwork and clears the old failure episode");
    fleet.state                    = FleetState::Mining;
    fleet.cargo->progress->current = 101;
    fleet_opc::Invalidate(0);
    OpcRefresh(state_pointer);
    card_text.text          = il2cpp::NewString("stale card again");
    opc_native_text_failure = true;
    OpcRefresh(state_pointer);
    Require(Opc().views.at(state_pointer).retry > OpcNow(), "second native card failure has an active retry deadline");
    opc_native_text_failure = false;
    auto& replacement_timer = visual("replacement timer", &panel_transform);
    native_panel.timer      = &component(timer, replacement_timer);
    auto& replacement_card  = visual("CommunityOpcCardEtaLabel", &replacement_timer);
    auto& replacement_text  = component(label, replacement_card);
    OpcRefresh(state_pointer);
    Require(Opc().views.at(state_pointer).retry == 0 && replacement_card.object->active
                && il2cpp::Equals(replacement_text.text, "OPC: Now"),
            "replacement timer anchor on unchanged fleet and tile bypasses the old card setup backoff");
    const auto destroys   = opc_native_destroys;
    const auto children   = body.children.size();
    native_text.transform = nullptr;
    Require(!OpcRenderEta(body_pointer, false, status) && eta_label.object->destroyed
                && opc_native_destroys == destroys + 1 && body.children.size() == children && !eta_label.object->active
                && !backing.object->active,
            "existing label without a native transform is destroyed and refused without a duplicate named child");
    backing.object->components.clear();
    Require(!OpcEnsureBacking(body_pointer) && backing.object->destroyed && opc_native_destroys == destroys + 2
                && body.children.size() == children,
            "existing backing without Image is destroyed and refused in the same discovery episode");
  } else {
    int originals = 0;
    FleetStateWidget_SetWidgetData_OpcHook([&](auto*) { ++originals; }, state_pointer);
    FleetbarFlagWidget_SetWidgetData_OpcHook([&](auto*) { ++originals; }, flag_pointer);
    Require(originals == 2 && Opc().views.size() == 2 && opc_native_fleet_reads == 0,
            "disabled native bind callbacks retain one original and register weak views without sampling");
    FleetLocalViewController_OnCurrentCargoReactiveEvent_OpcHook(
        [&](auto*, int32_t flags) {
          Require(flags == 17, "disabled cargo callback preserves the native dirty flags");
          ++originals;
        },
        reinterpret_cast<FleetLocalViewController*>(&native_tile), 17);
    Require(originals == 3 && opc_native_fleet_reads == 0,
            "both controls disabled preserves the original cargo event without model observations");
    Opc().touched = true;
    FleetStateWidget_ClearWidgetData_OpcHook([&](auto*) { ++originals; }, state_pointer);
    Require(!eta_label.object->active && !backing.object->active && highlight.object->active,
            "clearing an ETA widget preserves the independent highlight on its shared body");
    eta_label.object->active = backing.object->active = true;
    FleetbarFlagWidget_ClearWidgetData_OpcHook([&](auto*) { ++originals; }, flag_pointer);
    Require(!highlight.object->active && eta_label.object->active && backing.object->active && originals == 5,
            "clearing a flag widget preserves ETA and invokes its native clear exactly once");
    ui.highlight_opc_fleets = ui.fleet_hud_opc_eta = true;
    native_tile.active = false; // Disabled native component still needs stale-artwork cleanup.
    auto* tile_pointer = reinterpret_cast<FleetLocalViewController*>(&native_tile);
    OpcBind(tile_pointer, OpcOwner::Local);
    OpcFrame();
    const auto first_generation        = Opc().views.at(tile_pointer).generation;
    Opc().views.at(tile_pointer).retry = OpcNow() + 30000;
    Opc().next                         = OpcNow() + 30000;
    eta_label.object->active = backing.object->active = highlight.object->active = true;
    ui.fleet_hud_opc_eta                                                         = false;
    OpcFrame();
    Require(Opc().options_active == 1 && !eta_label.object->active && !backing.object->active
                && Opc().views.at(tile_pointer).retry == 0,
            "turning ETA off while highlight stays on bypasses cadence and clears stale UI and backoff");
    ui.highlight_opc_fleets = false;
    OpcFrame();
    Require(!highlight.object->active && !eta_label.object->active && !backing.object->active,
            "turning both controls off cleans existing surfaces on the next shared frame");
    OpcBind(tile_pointer, OpcOwner::Local);
    Require(Opc().views.at(tile_pointer).generation != first_generation,
            "native view rebind starts a fresh generation even for an unchanged fleet");
    warp_root_failure = true;
    OpcRefresh(tile_pointer);
    warp_root_failure = false;
    Require(Opc().views.size() == 1, "temporary strong-root refusal does not discard a surviving weak view");
    for (auto& [_, target] : warp_roots)
      if (target == &native_tile.header)
        target = nullptr;
    OpcRefresh(tile_pointer);
    Require(Opc().views.empty(), "collected weak native view is removed without traversing destroyed UI");
  }
  Opc().views.clear();
  type_object_override = nullptr;
  std::cout << "OPC production " << mode << " checks passed; real Unity scene and hook dispatch remain separate\n";
}
} // namespace
