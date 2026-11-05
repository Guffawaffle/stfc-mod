#pragma once

// The production dock consumer over controlled runtime metadata and counted Button.Press bodies. Pin input,
// persistence and frame installation are external boundaries; no native dock, ship or store is changed.
#include "features/ship_pin_input.h"
#include "frame.h"
#include "mod_state.h"
#include <functional>

namespace dock_fixture_input
{
bool consume = false;
int  calls   = 0;
bool Install()
{ return false; }
bool Available()
{ return false; }
bool ConsumeTileClick(ShipTileWidget*)
{
  ++calls;
  return consume;
}
void RefreshVisible() {}
void Tick() {}
} // namespace dock_fixture_input

// Existing preset fixtures supply Read/PrepareAsync/Status. Dock's otherwise unused pin writer must remain inert.
namespace mod_state
{
uint64_t QueueShipPins(const std::vector<int64_t>&, const std::vector<std::string>&)
{ return 0; }
} // namespace mod_state
il2cpp::HookInstallResult DockFixtureFrameTick()
{ return {il2cpp::HookInstallState::kRejected}; }
bool DockFixtureRegisterFrame(ScreenManagerUpdateCallback, const char*) noexcept
{ return false; }
bool DockFixtureFrameAvailable() noexcept
{ return false; }

// The pin-presentation fixture already owns the public ship_pins exports. Keep this feature's actual definitions
// private; assignment still calls the real bindings and callbacks below.
#define ship_pins dock_fixture_pins
#define ship_pin_input dock_fixture_input
#define EnsureFrameTick DockFixtureFrameTick
#define RegisterFrameCallback DockFixtureRegisterFrame
#define FrameDeliveryAvailable DockFixtureFrameAvailable
#include "../src/mod/features/dock.cc"
#undef FrameDeliveryAvailable
#undef RegisterFrameCallback
#undef EnsureFrameTick
#undef ship_pin_input
#undef ship_pins

namespace dock_assignment_fixture
{
struct Node {
  Il2CppObject header{};
  Node*        next   = nullptr;
  Node*        canvas = nullptr;
  bool         alive = true, active = true, visible = true;
  bool         fail_alive = false, fail_active = false, fail_parent = false, fail_visible = false;
  int          presses = 0, active_reads = 0, visible_reads = 0;
};
std::function<void(Node*)> on_active, on_parent, on_visible;
int                        total_presses = 0;

bool Alive(Il2CppObject* value, const MethodInfo*)
{
  auto* node = reinterpret_cast<Node*>(value);
  if (node->fail_alive)
    throw Il2CppExceptionWrapper(&thrown);
  return node->alive;
}
bool Active(Node* node, const MethodInfo*)
{
  ++node->active_reads;
  if (node->fail_active)
    throw Il2CppExceptionWrapper(&thrown);
  const auto result = node->active;
  if (on_active)
    std::exchange(on_active, {})(node);
  return result;
}
Il2CppObject* Parent(Node* node, Il2CppReflectionType*, const MethodInfo*)
{
  if (node->fail_parent)
    throw Il2CppExceptionWrapper(&thrown);
  auto* result = node->canvas;
  if (on_parent)
    std::exchange(on_parent, {})(node);
  return result ? &result->header : nullptr;
}
bool Visible(Node* node, const MethodInfo*)
{
  ++node->visible_reads;
  if (node->fail_visible)
    throw Il2CppExceptionWrapper(&thrown);
  const auto result = node->visible;
  if (on_visible)
    std::exchange(on_visible, {})(node);
  return result;
}
void Press(Node* node, const MethodInfo*)
{
  ++node->presses;
  ++total_presses;
}
FleetPlayerData* Context(Node* node, const MethodInfo*)
{ return reinterpret_cast<FleetPlayerData*>(node->next); }

void EnterFrame(unity::KeyCode key, bool down)
{
  ++input_frame;
  held_keys.fill(false);
  down_keys.fill(false);
  if (key != unity::KeyCode::None) {
    held_keys[std::to_underlying(key)] = true;
    down_keys[std::to_underlying(key)] = down;
  }
  any_held = key != unity::KeyCode::None;
  any_down = down && any_held;
  unity::Key::Poll();
  AssignOnEnterFrame();
}

void Cases(std::string_view mode)
{
  auto& behaviour = Define("UnityEngine", "Behaviour", IL2CPP_TYPE_CLASS, &component_class);
  auto& canvas    = Define("Digit.Client.UI", "CanvasController", IL2CPP_TYPE_CLASS, &behaviour);
  auto& widget    = Define("Digit.Prime.Ships", "AssignShipsWidget", IL2CPP_TYPE_CLASS, &behaviour);
  auto& wrapper   = Define("Digit.Client.UI", "ButtonAndContextWrapper", IL2CPP_TYPE_CLASS, &object_class);
  auto& generic   = Define("Digit.Client.UI", "GenericButtonWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& listener  = Define("Digit.Client.UI", "SemaphoreButtonListener", IL2CPP_TYPE_CLASS, &object_class);
  auto& button    = Define("UnityEngine.UI", "Button", IL2CPP_TYPE_CLASS, &behaviour);
  auto& tile      = Define("Digit.Prime.Ships", "ShipTileWidget", IL2CPP_TYPE_CLASS, &component_class);
  for (auto* owner : {&behaviour, &canvas, &widget, &wrapper, &generic, &listener, &button, &tile})
    owner->klass.instance_size = sizeof(Node);
  const auto& active =
      AddMethod(behaviour, "get_isActiveAndEnabled", reinterpret_cast<void*>(&Active), T(bool_class), {});
  AddProperty(behaviour, "isActiveAndEnabled", &active);
  AddMethod(component_class, "GetComponentInParent", reinterpret_cast<void*>(&Parent), T(canvas), {T(type_class)});
  const auto& visible = AddMethod(canvas, "get_Visible", reinterpret_cast<void*>(&Visible), T(bool_class), {});
  AddProperty(canvas, "Visible", &visible);
  AddField(widget, "_assignButton", wrapper, offsetof(Node, next));
  AddField(wrapper, "Widget", generic, offsetof(Node, next));
  AddField(generic, "_semaphoreButtonListener", listener, offsetof(Node, next));
  AddField(listener, "_button", button, offsetof(Node, next));
  AddMethod(button, "Press", reinterpret_cast<void*>(&Press), T(void_class), {});
  const auto& get_context = AddMethod(tile, "get_Context", reinterpret_cast<void*>(&Context), T(fleet_data_class), {});
  AddProperty(tile, "Context", &get_context);
  // BuildRuntime already defines UnityEngine.Object.op_Implicit. Replace only its controlled native body.
  const auto alive = unity::UnityObject::Class::StaticMethod<bool(unity::UnityObject*)>("op_Implicit");
  Require(static_cast<bool>(alive), "dock fixture has the actual Unity liveness binding");
  const_cast<MethodInfo*>(alive.Info())->methodPointer = reinterpret_cast<Il2CppMethodPointer>(&Alive);

  Node canvas_a{{&canvas.klass}}, canvas_b{{&canvas.klass}};
  Node button_a{{&button.klass}}, button_b{{&button.klass}}, replacement_button{{&button.klass}};
  Node listener_a{{&listener.klass}, &button_a}, listener_b{{&listener.klass}, &button_b};
  Node generic_a{{&generic.klass}, &listener_a}, generic_b{{&generic.klass}, &listener_b};
  Node wrapper_a{{&wrapper.klass}, &generic_a}, wrapper_b{{&wrapper.klass}, &generic_b};
  Node dock_a{{&widget.klass}, &wrapper_a, &canvas_a}, dock_b{{&widget.klass}, &wrapper_b, &canvas_b};
  fleet_native_widgets[&widget.klass] = {&dock_a.header, &dock_b.header};
  fleet_native_capture                = false;
  selected                            = nullptr;
  has_event_system                    = true;
  auto& config                        = Config::Get();
  config.patches.hotkeyhooks = config.patches.doubleclickassignshiphooks = true;
  config.ui.double_click_to_assign_ship                                  = true;
  double_click_installed                                                 = true;
  last_clicked_ship                                                      = nullptr;
  on_active = on_parent = on_visible = {};
  dock_fixture_input::consume        = false;
  dock_fixture_input::calls          = 0;
  const auto   roots                 = warp_roots.size();
  Il2CppObject ship_a{&fleet_data_class.klass}, ship_b{&fleet_data_class.klass};
  auto*        a = reinterpret_cast<FleetPlayerData*>(&ship_a);
  auto*        b = reinterpret_cast<FleetPlayerData*>(&ship_b);

  const auto pair = [&] {
    AssignOnDoubleClick(a);
    AssignOnDoubleClick(a);
  };
  if (mode == "dock-assign-first") {
    pair();
    Require(button_a.presses == 1 && button_b.presses == 0, "only the first shown dock is assigned");
  } else if (mode == "dock-assign-inactive" || mode == "dock-assign-hidden" || mode == "dock-assign-dead") {
    dock_a.active    = mode != "dock-assign-inactive";
    canvas_a.visible = mode != "dock-assign-hidden";
    dock_a.alive     = mode != "dock-assign-dead";
    pair();
    Require(button_a.presses == 0 && button_b.presses == 1,
            "known inactive, hidden or destroyed candidates are skipped");
  } else if (mode == "dock-assign-missing-button") {
    dock_a.next = nullptr;
    pair();
    Require(total_presses == 0 && dock_b.active_reads == 0, "first shown dock with no button stops selection");
  } else if (mode == "dock-assign-rebound-before-read") {
    on_visible = [&](Node*) { listener_a.next = &replacement_button; };
    pair();
    Require(button_a.presses == 0 && replacement_button.presses == 1,
            "a chain rebound before acquisition uses its current button");
  } else if (mode == "dock-assign-chain-rebind") {
    bool rebound = false;
    on_visible   = [&](Node*) {
      on_visible = [&](Node* selected_canvas) {
        Require(selected_canvas == &canvas_a, "the selected canvas is checked again before assignment");
        for (auto* retained : {&dock_a, &canvas_a, &wrapper_a, &generic_a, &listener_a, &button_a})
          Require(std::ranges::any_of(warp_roots, [&](const auto& root) { return root.second == &retained->header; }),
                  "selected dock and complete old button chain remain rooted across the managed visibility call");
        listener_a.next = &replacement_button;
        rebound         = true;
      };
    };
    pair();
    Require(rebound && total_presses == 0 && dock_b.active_reads == 0,
            "a button chain rebound after acquisition cannot press the stale or replacement button");
  } else if (mode == "dock-assign-parent-rebind" || mode == "dock-assign-late-parent-rebind") {
    const auto rebind = [&](Node*) { dock_a.canvas = &canvas_b; };
    if (mode == "dock-assign-late-parent-rebind")
      on_visible = [&, rebind](Node*) { on_visible = rebind; };
    else
      on_visible = rebind;
    pair();
    Require(total_presses == 0 && dock_b.active_reads == 0,
            "a selected dock rebound to another visible canvas cannot assign or redirect");
  } else if (mode == "dock-assign-button-dead" || mode == "dock-assign-button-unknown") {
    button_a.alive      = mode != "dock-assign-button-dead";
    button_a.fail_alive = mode == "dock-assign-button-unknown";
    pair();
    Require(total_presses == 0 && dock_b.active_reads == 0,
            "a destroyed or unreadable button on the first shown dock cannot assign or redirect");
  } else if (mode == "dock-assign-disable-reentry" || mode == "dock-assign-capture-reentry"
             || mode == "dock-assign-hide-reentry" || mode == "dock-assign-destroy-reentry") {
    on_visible = [&](Node*) {
      if (mode == "dock-assign-disable-reentry")
        Config::Get().ui.double_click_to_assign_ship = false;
      else if (mode == "dock-assign-capture-reentry")
        fleet_native_capture = true;
      else if (mode == "dock-assign-hide-reentry")
        canvas_a.visible = false;
      else
        dock_a.alive = false;
    };
    if (mode == "dock-assign-capture-reentry")
      EnterFrame(unity::KeyCode::Return, true);
    else
      pair();
    Require(total_presses == 0, "a stale policy, visibility or Unity lifetime cannot authorize assignment");
  } else if (mode == "dock-assign-unknown-active" || mode == "dock-assign-unknown-parent"
             || mode == "dock-assign-unknown-visible" || mode == "dock-assign-unknown-alive") {
    dock_a.fail_active    = mode == "dock-assign-unknown-active";
    dock_a.fail_parent    = mode == "dock-assign-unknown-parent";
    canvas_a.fail_visible = mode == "dock-assign-unknown-visible";
    dock_a.fail_alive     = mode == "dock-assign-unknown-alive";
    pair();
    Require(total_presses == 0, "unknown first-candidate state cannot redirect assignment into another shown dock");
  } else if (mode == "dock-assign-double") {
    AssignOnDoubleClick(a);
    Require(total_presses == 0, "one click does not assign");
    AssignOnDoubleClick(a);
    Require(total_presses == 1, "the second paired click assigns once");
    AssignOnDoubleClick(a);
    Require(total_presses == 1, "the third click does not immediately assign again");
    AssignOnDoubleClick(a);
    Require(total_presses == 2, "a new complete pair may assign again");
  } else if (mode == "dock-assign-different" || mode == "dock-assign-late") {
    AssignOnDoubleClick(a);
    if (mode == "dock-assign-late") {
      last_click_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(401);
      AssignOnDoubleClick(a);
    } else {
      AssignOnDoubleClick(b);
    }
    Require(total_presses == 0, "different or late clicks restart the pair");
    AssignOnDoubleClick(mode == "dock-assign-late" ? a : b);
    Require(total_presses == 1, "a later same-identity pair assigns");
  } else if (mode == "dock-assign-enter") {
    EnterFrame(unity::KeyCode::Return, true);
    EnterFrame(unity::KeyCode::Return, false);
    Require(total_presses == 1, "held Enter has only one assignment edge");
    EnterFrame(unity::KeyCode::KeypadEnter, true);
    Require(total_presses == 2, "keypad Enter has the same assignment route");
    fleet_native_capture = true;
    EnterFrame(unity::KeyCode::Return, true);
    Require(total_presses == 2, "capture owns raw Enter before the feature callback");
  } else if (mode == "dock-assign-disabled") {
    config.ui.double_click_to_assign_ship = false;
    pair();
    EnterFrame(unity::KeyCode::Return, true);
    Require(total_presses == 0, "disabled live setting suppresses both entry routes");
    config.ui.double_click_to_assign_ship     = true;
    config.patches.doubleclickassignshiphooks = false;
    EnterFrame(unity::KeyCode::Return, true);
    Require(total_presses == 0, "disabled assignment patch suppresses Enter");
  } else if (mode == "dock-assign-pin-consume") {
    last_clicked_ship           = a;
    dock_fixture_input::consume = true;
    Node tile_node{{&tile.klass}, reinterpret_cast<Node*>(&ship_a)};
    int  originals = 0;
    ShipTileWidget_HandleOnClick_Hook([&](auto*) { ++originals; }, reinterpret_cast<ShipTileWidget*>(&tile_node));
    Require(originals == 0 && last_clicked_ship == nullptr && dock_fixture_input::calls == 1 && total_presses == 0,
            "consumed pin gesture clears the pending pair without ordinary click handling");
  } else {
    Require(mode == "dock-assign-native-click" || mode == "dock-assign-physical-capture",
            "known dock assignment fixture mode");
    fleet_native_capture = mode == "dock-assign-physical-capture";
    Node  tile_node{{&tile.klass}, reinterpret_cast<Node*>(&ship_a)};
    int   originals     = 0;
    auto  original      = [&](auto*) { ++originals; };
    auto* tile_receiver = reinterpret_cast<ShipTileWidget*>(&tile_node);
    ShipTileWidget_HandleOnClick_Hook(original, tile_receiver);
    ShipTileWidget_HandleOnClick_Hook(original, tile_receiver);
    Require(originals == 2 && button_a.presses == 1, "ordinary tile clicks retain native-first double-click flow");
  }
  Require(warp_roots.size() == roots, "dock assignment releases operation-local managed custody");
  std::cout << "Production dock assignment fixture passed: " << mode << '\n';
}
} // namespace dock_assignment_fixture

void DockAssignmentCases(std::string_view mode)
{ dock_assignment_fixture::Cases(mode); }
