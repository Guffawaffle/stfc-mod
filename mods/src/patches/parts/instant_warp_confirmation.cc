#include "config.h"
#include "errormsg.h"
#include "patches/instant_warp_policy.h"
#include "patches/key.h"
#include "patches/warp_click_intent.h"
#include "ship_name_match.h"
#include <il2cpp/il2cpp_checked.h>
#include <il2cpp/method_contract.h>

#include <prime/CourseData.h>
#include <prime/CoursePromptPopupWidget.h>
#include <prime/FleetsManager.h>

#include <spud/detour.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace
{
using PopupAction                            = void(CoursePromptPopupWidget*);
PopupAction*    initiate_regular_warp        = nullptr;
PopupAction*    on_instant_warp_button_click = nullptr;
WarpClickIntent click_intent;
bool            click_override_available = false;
bool            unmatched_click          = false;

std::int64_t ReadId(Il2CppObject* object, const char* getter)
{
  auto* value = Il2CppChecked::Invoke(object, getter);
  if (!value || !value->klass || il2cpp_class_get_type(value->klass)->type != IL2CPP_TYPE_I8)
    throw std::runtime_error("expected Int64 identifier");
  return *static_cast<std::int64_t*>(il2cpp_object_unbox(value));
}

bool ConsumeClickIntent(CourseData* course)
{
  // Clear before managed calls, including on a mismatched popup or failed read.
  auto pending = std::exchange(click_intent, {});
  if (std::exchange(unmatched_click, false))
    return true;
  if (!pending.fleet)
    return false;
  if (!course)
    return true;
  try {
    auto* object = reinterpret_cast<Il2CppObject*>(course);
    auto* target = Il2CppChecked::Invoke(object, "get_TargetNode");
    return pending.Consume(ReadId(object, "get_FleetID"), ReadId(target, "get_ID"));
  } catch (const std::exception& error) {
    spdlog::warn("[InstantWarpConfirmation] Modified-click course could not be matched: {}", error.what());
    // Once the user explicitly requested a choice, a failed read must not auto-select Jump.
    return true;
  }
}

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
bool (*mouse_held)(int)     = nullptr;
bool (*mouse_released)(int) = nullptr;

#if defined(__APPLE__)
// Nullable<Vector3>: INTEGER/SSE on Intel, two integer registers on Apple Silicon.
// Keep the native value layout; Windows x64 instead passes this value indirectly.
struct MovePosition {
  bool  has_value;
  float x, y, z;
};
static_assert(sizeof(MovePosition) == 16 && offsetof(MovePosition, x) == 4);
#else
using MovePosition = void*;
#endif

// This is the manual move request edge, before native confirmation/callback delays.
void OnMoveFleetAction_Hook(auto original, void* self, MovePosition position, std::int64_t node_id,
                            std::int64_t fleet_id, bool force_move)
{
  click_intent    = {};
  unmatched_click = false;
  if (WarpPromptOverrideHeld() && (mouse_held(0) || mouse_released(0))) {
    unmatched_click = true;
    try {
      auto id = fleet_id;
      if (id == -1) {
        auto* manager = reinterpret_cast<Il2CppObject*>(FleetsManager::Instance());
        id            = ReadId(Il2CppChecked::Invoke(manager, "GetSelectedFleetData"), "get_Id");
      }
      click_intent.Begin(true, id, node_id);
      unmatched_click = click_intent.fleet == 0;
    } catch (const std::exception& error) {
      spdlog::warn("[InstantWarpConfirmation] Modified-click request could not be captured: {}", error.what());
    }
  }
  original(self, position, node_id, fleet_id, force_move);
}
#endif

void InstallClickOverride()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  auto  helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationManager");
  auto* method = method_contract::Resolve(
      helper.get_cls(), "OnMoveFleetAction", false, "System.Void",
      {"System.Nullable<UnityEngine.Vector3>", "System.Int64", "System.Int64", "System.Boolean"});
  mouse_held     = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButton(System.Int32)");
  mouse_released = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButtonUp(System.Int32)");
  if (!mouse_held || !mouse_released || !method_contract::Pointer(method)) {
    spdlog::warn("[InstantWarpConfirmation] Modified-click method or input functions unavailable; override disabled");
    return;
  }
  click_override_available = SPUD_STATIC_DETOUR(method->methodPointer, OnMoveFleetAction_Hook) != nullptr;
  spdlog::info("[InstantWarpConfirmation] Modified-click choice override {}",
               click_override_available ? "installed" : "unavailable");
#endif
}

void CoursePromptPopupViewController_AboutToShow_Hook(auto original, CoursePromptPopupViewController* _this)
{
  original(_this);

  const auto widget  = _this == nullptr ? nullptr : _this->PopupWidget;
  const auto context = widget == nullptr ? nullptr : widget->Context;
  const auto course  = context ? context->GetCourseData() : nullptr;
  const bool ask     = ConsumeClickIntent(course);
  if (context == nullptr || !context->HasInstantWarp) {
    return;
  }

  FleetPlayerData* fleet = nullptr;
  if (course != nullptr)
    fleet = course->PlayerFleet;
  const auto action = ask ? InstantWarpConfirmation::None : ResolveInstantWarpConfirmation(fleet);
  spdlog::debug("InstantWarpConfirmation: resolved action {}", static_cast<int>(action));
  switch (action) {
    case InstantWarpConfirmation::Warp:
      initiate_regular_warp(widget);
      break;
    case InstantWarpConfirmation::Jump:
      on_instant_warp_button_click(widget);
      break;
    case InstantWarpConfirmation::None:
      break;
  }
}
} // namespace

bool WarpPromptOverrideHeld()
{
#if defined(__APPLE__)
  return click_override_available && (Key::Pressed(KeyCode::LeftCommand) || Key::Pressed(KeyCode::RightCommand));
#else
  return click_override_available && Key::HasCtrl();
#endif
}

InstantWarpConfirmation ResolveInstantWarpConfirmation(FleetPlayerData* fleet)
{
  const auto& cfg        = Config::Get();
  const auto  candidates = ShipNameMatch::DisplayWords(fleet);
  const auto  matches    = [&candidates](const std::vector<std::string>& names, bool all) {
    return all || (!candidates.empty() && std::ranges::any_of(names, [&](const auto& configured) {
             return ShipNameMatch::MatchesDisplay(candidates, ShipNameMatch::SplitWords(configured));
           }));
  };
  if (matches(cfg.instant_warp_always_ask, cfg.instant_warp_always_ask_all))
    return InstantWarpConfirmation::None;
  if (matches(cfg.instant_warp_auto_jump, cfg.instant_warp_auto_jump_all))
    return InstantWarpConfirmation::Jump;
  if (matches(cfg.instant_warp_auto_warp, cfg.instant_warp_auto_warp_all))
    return InstantWarpConfirmation::Warp;
  return cfg.auto_confirm_instant_warp;
}

void InstallInstantWarpConfirmationHooks()
{
  auto helper = CoursePromptPopupViewController::get_class_helper();
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.ObjectViewer", "CoursePromptPopupViewController");
    return;
  }

  const auto about_to_show = helper.GetMethod("AboutToShow", 0);
  if (about_to_show == nullptr) {
    ErrorMsg::MissingMethod("CoursePromptPopupViewController", "AboutToShow");
    return;
  }

  auto widget_helper = CoursePromptPopupWidget::get_class_helper();
  if (!widget_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Navigation", "CoursePromptPopupWidget");
    return;
  }

  initiate_regular_warp = widget_helper.GetMethod<PopupAction>("InitiateRegularWarp", 0);
  if (initiate_regular_warp == nullptr) {
    ErrorMsg::MissingMethod("CoursePromptPopupWidget", "InitiateRegularWarp");
    return;
  }

  on_instant_warp_button_click = widget_helper.GetMethod<PopupAction>("OnInstantWarpButtonClick", 0);
  if (on_instant_warp_button_click == nullptr) {
    ErrorMsg::MissingMethod("CoursePromptPopupWidget", "OnInstantWarpButtonClick");
    return;
  }

  if (SPUD_STATIC_DETOUR(about_to_show, CoursePromptPopupViewController_AboutToShow_Hook)) {
    InstallClickOverride();
    InstallWarpActionLabel();
  }
}
