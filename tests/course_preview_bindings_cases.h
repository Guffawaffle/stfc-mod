#pragma once

// Extends the already-included production fleet adapter and controlled runtime.
// Native cargo rendering bodies are counted substitutes; no player data or UI is used.
namespace
{
struct PreviewCargoVisibility {
  Il2CppObject    header;
  VisibilityState state = VisibilityState::Hidden;
};
struct PreviewCargoTarget {
  Il2CppObject  header;
  Il2CppObject* deployed = nullptr;
  int64_t       id       = 100;
};
struct PreviewCargoRewards {
  Il2CppObject            header;
  PreviewCargoTarget*     context = nullptr;
  PreviewCargoVisibility* cargo   = nullptr;
};
struct PreviewCargoViewer {
  Il2CppObject            header;
  PreviewCargoVisibility* visibility = nullptr;
  PreviewCargoRewards*    rewards    = nullptr;
};
int                  preview_cargo_shows = 0, preview_cargo_hides = 0;
bool                 preview_cargo_type_throws = false, preview_cargo_cancel_on_context = false;
DeployedFleetType    preview_cargo_fleet_type    = DeployedFleetType::Player;
HullType             preview_cargo_hull_type     = HullType::Any;
Il2CppObject*        preview_cargo_hull          = nullptr;
PreviewCargoRewards* preview_cargo_rebind_owner  = nullptr;
PreviewCargoTarget*  preview_cargo_rebind_target = nullptr;

Il2CppObject* PreviewCargoContext(PreviewCargoRewards* self, const MethodInfo*)
{
  if (preview_cargo_cancel_on_context)
    CancelTargetCargoForWidget(reinterpret_cast<RewardsButtonWidget*>(self));
  return reinterpret_cast<Il2CppObject*>(self->context);
}
int64_t PreviewCargoId(PreviewCargoTarget* self, const MethodInfo*)
{ return self->id; }
DeployedFleetType PreviewCargoFleetType(Il2CppObject*, const MethodInfo*)
{
  if (preview_cargo_type_throws)
    throw Il2CppExceptionWrapper(&thrown);
  if (preview_cargo_rebind_owner)
    preview_cargo_rebind_owner->context = preview_cargo_rebind_target;
  return preview_cargo_fleet_type;
}
Il2CppObject* PreviewCargoHull(Il2CppObject*, const MethodInfo*)
{ return preview_cargo_hull; }
HullType PreviewCargoHullType(Il2CppObject*, const MethodInfo*)
{ return preview_cargo_hull_type; }
void PreviewCargoShow(PreviewCargoVisibility* self, bool instant, const MethodInfo*)
{
  Require(instant, "cargo opens through original instant Show(true)");
  ++preview_cargo_shows;
  self->state = VisibilityState::Visible;
}
void PreviewCargoHide(PreviewCargoVisibility* self, bool, const MethodInfo*)
{
  ++preview_cargo_hides;
  self->state = VisibilityState::Hidden;
}

void CargoPreviewNativeCases(std::string_view mode)
{
  auto& target      = Define("Digit.PrimeServer.Models", "BattleTargetData", IL2CPP_TYPE_CLASS, &object_class);
  auto& deployed    = Define("Digit.PrimeServer.Models", "FleetDeployedData", IL2CPP_TYPE_CLASS, &object_class);
  auto& hull        = Define("Digit.PrimeServer.Models", "HullSpec", IL2CPP_TYPE_CLASS, &object_class);
  auto& visibility  = Define("Digit.Client.UI", "VisibilityController", IL2CPP_TYPE_CLASS, &object_class);
  auto& viewer_base = Define("Digit.Prime.ObjectViewer", "ObjectViewerBaseWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& viewer      = Define("Digit.Prime.Combat", "PreScanTargetWidget", IL2CPP_TYPE_CLASS, &viewer_base);
  auto& rewards     = Define("Digit.Prime.Combat", "RewardsButtonWidget", IL2CPP_TYPE_CLASS, &component_class);
  auto& fleet_type  = Define("Digit.PrimeServer.Models", "DeployedFleetType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  auto& hull_type   = Define("Digit.PrimeServer.Models", "HullType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  fleet_type.is_enum = hull_type.is_enum = true;
  AddField(target, "TargetFleetDeployedData", mode == "preview-cargo-field" ? intptr_class : deployed,
           offsetof(PreviewCargoTarget, deployed));
  AddField(visibility, "_state", int_class, offsetof(PreviewCargoVisibility, state));
  AddField(viewer_base, "_visibilityController", visibility, offsetof(PreviewCargoViewer, visibility));
  AddField(viewer, "_rewardsButtonWidget", rewards, offsetof(PreviewCargoViewer, rewards));
  AddMethod(viewer, "get_isActiveAndEnabled", reinterpret_cast<void*>(&VisualActive), T(bool_class), {});
  AddField(rewards, "_rewardsController", visibility, offsetof(PreviewCargoRewards, cargo));
  AddMethod(rewards, "get_Context", reinterpret_cast<void*>(&PreviewCargoContext), T(target), {});
  AddMethod(target, "get_TargetID", reinterpret_cast<void*>(&PreviewCargoId), T(long_class), {});
  AddMethod(deployed, "get_FleetType", reinterpret_cast<void*>(&PreviewCargoFleetType), T(fleet_type), {});
  AddMethod(deployed, "get_Hull", reinterpret_cast<void*>(&PreviewCargoHull), T(hull), {});
  AddMethod(hull, "get_Type", reinterpret_cast<void*>(&PreviewCargoHullType), T(hull_type), {});
  AddMethod(visibility, "Show", reinterpret_cast<void*>(&PreviewCargoShow), T(void_class), {T(bool_class)});
  AddMethod(visibility, "Hide", reinterpret_cast<void*>(&PreviewCargoHide), T(void_class), {T(bool_class)});
  PreviewCargoVisibility outer{{&visibility.klass}, VisibilityState::Visible}, cargo{{&visibility.klass}};
  PreviewCargoTarget     context{{&target.klass}}, replacement{{&target.klass}};
  PreviewCargoRewards    widget{{&rewards.klass}, &context, &cargo};
  PreviewCargoViewer     card{{&viewer.klass}, &outer, &widget};
  Il2CppObject           deployed_object{&deployed.klass}, hull_object{&hull.klass};
  auto*                  native_widget      = reinterpret_cast<RewardsButtonWidget*>(&widget);
  auto*                  native_card        = reinterpret_cast<PreScanTargetWidget*>(&card);
  fleet_native_widgets[&viewer.klass]       = {&card.header};
  Config::Get().control.hotkeys_enabled     = true;
  Config::Get().control.use_scopely_hotkeys = false;
  auto& ui                                  = Config::Get().ui;
  ui.show_cargo_default = ui.show_station_cargo = ui.show_player_cargo = true;
  ui.show_hostile_cargo = ui.show_armada_cargo = true;
  input_frame                                  = 500;
  selected = nullptr; // The shared runtime starts with a focused text field; this fixture exercises gameplay dispatch.
  unity::Key::Poll();
  Require(!unity::Key::IsInputFocused(), "cargo retry fixture starts in an eligible unfocused input episode");
  const auto roots = warp_roots.size();
  if (mode == "preview-cargo-field") {
    context.deployed = &deployed_object;
    Require(!ShowsCargoByDefault(native_widget) && preview_cargo_shows == 0,
            "native-pointer target field cannot become an observed-null station");
    Require(warp_roots.size() == roots, "unavailable field releases acquired roots");
    return;
  }
  Require(ShowsCargoByDefault(native_widget), "confirmed null deployed target preserves station category");
  ui.show_station_cargo = false;
  Require(!ShowsCargoByDefault(native_widget), "station category retains its independent flag");
  context.deployed = &deployed_object;
  Require(ShowsCargoByDefault(native_widget), "player category retains its independent flag");
  ui.show_player_cargo = false;
  Require(!ShowsCargoByDefault(native_widget), "disabled player category does not show cargo");
  preview_cargo_fleet_type = DeployedFleetType::Marauder;
  Require(ShowsCargoByDefault(native_widget), "observed null hostile hull retains source hostile fallback");
  preview_cargo_hull      = &hull_object;
  preview_cargo_hull_type = HullType::ArmadaTarget;
  Require(ShowsCargoByDefault(native_widget), "armada hull selects armada flag");
  ui.show_armada_cargo = false;
  Require(!ShowsCargoByDefault(native_widget), "disabled armada category cannot use hostile flag");
  preview_cargo_hull_type = HullType::Any;
  Require(ShowsCargoByDefault(native_widget), "ordinary hostile hull selects hostile flag");
  preview_cargo_fleet_type = DeployedFleetType::Alliance;
  Require(!ShowsCargoByDefault(native_widget), "unsupported fleet category does not become player or hostile");
  preview_cargo_type_throws = true;
  Require(!ShowsCargoByDefault(native_widget), "managed category failure is unavailable, not a station");
  preview_cargo_type_throws = false;
  preview_cargo_fleet_type  = DeployedFleetType::Player;
  ui.show_player_cargo      = true;

  auto current = ObserveTargetCargo(native_card);
  Require(current.has_value(), "visible original target can be acquired with owned custody");
  ScheduleTargetCargo(std::move(*current), false, 5);
  Require(pending_target_cargo.size() == 1 && warp_roots.size() == roots + 4,
          "pending cargo owns the exact card, rewards, context and controller");
  widget.context = &replacement;
  ShowPendingTargetCargo();
  Require(preview_cargo_shows == 0 && pending_target_cargo.empty() && warp_roots.size() == roots,
          "rebound context cannot receive another target's delayed cargo request");
  widget.context = &context;
  current        = ObserveTargetCargo(native_card);
  ScheduleTargetCargo(std::move(*current), false, 5);
  ++context.id;
  ShowPendingTargetCargo();
  Require(preview_cargo_shows == 0 && pending_target_cargo.empty(),
          "same-object target ID replacement invalidates cargo request");
  current = ObserveTargetCargo(native_card);
  ScheduleTargetCargo(std::move(*current), false, 5);
  input_frame = 506;
  RetireTargetCargo();
  Require(pending_target_cargo.empty() && warp_roots.size() == roots,
          "absolute original five-pass window expires even without shortcut dispatch");
  input_frame = 507;
  current     = ObserveTargetCargo(native_card);
  ScheduleTargetCargo(std::move(*current), false, 5);
  preview_cargo_cancel_on_context = true;
  ShowPendingTargetCargo();
  preview_cargo_cancel_on_context = false;
  Require(preview_cargo_shows == 0 && pending_target_cargo.empty() && warp_roots.size() == roots,
          "same-identity cancellation during managed getters prevents stale native publication");
  ToggleTargetCargo();
  ShowPendingTargetCargo();
  Require(preview_cargo_shows == 1 && cargo.state == VisibilityState::Visible,
          "explicit view opens the same visible target's cargo");
  ToggleTargetCargo();
  Require(preview_cargo_hides == 1 && pending_target_cargo.empty(),
          "second view cancels pending reopens and hides existing cargo");
  ToggleTargetCargo();
  Config::Get().control.hotkeys_enabled = false;
  RetireTargetCargo();
  Require(pending_target_cargo.empty() && warp_roots.size() == roots,
          "disabled dispatch retires pending cargo without retaining native roots");
  Config::Get().control.hotkeys_enabled = true;
  preview_cargo_rebind_owner            = &widget;
  preview_cargo_rebind_target           = &replacement;
  ShowCargoByDefault(native_widget);
  Require(preview_cargo_shows == 1 && pending_target_cargo.empty(),
          "category reentrancy cannot apply an old policy to a new target context");
  preview_cargo_rebind_owner = nullptr;
  widget.context             = &context;
  ShowCargoByDefault(native_widget);
  Require(preview_cargo_shows == 2 && pending_target_cargo.size() == 1,
          "qualified automatic bind shows immediately and schedules its single same-source retry");
  ++input_frame;
  cargo.state = VisibilityState::Hidden; // Bind occurred after last frame's shortcut pass.
  ShowPendingTargetCargo();
  Require(preview_cargo_shows == 3 && pending_target_cargo.empty() && warp_roots.size() == roots,
          "automatic one-pass retry reaches the next frame and then releases every root");
  int originals = 0;
  RewardsButtonWidget_OnDidBindContext_Hook([&](auto*) { ++originals; }, native_widget);
  Require(originals == 1, "automatic cargo hook calls the native original exactly once");
  CancelTargetCargo();
  fleet_native_widgets.clear();
  Require(warp_roots.size() == roots, "cargo fixture cleanup releases every retained root");
  std::cout << "Production cargo category, owned retry and cancellation fixtures passed\n";
}
} // namespace
