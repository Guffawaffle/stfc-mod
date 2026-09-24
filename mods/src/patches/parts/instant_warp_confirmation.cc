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
#include <utility>

#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
#endif

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
    spdlog::warn("[InstantWarpConfirmation] Ctrl-click course could not be matched: {}", error.what());
    // Once the user explicitly requested a choice, a failed read must not auto-select Jump.
    return true;
  }
}

#if defined(_WIN32) && defined(_M_X64)
bool (*mouse_held)(int)     = nullptr;
bool (*mouse_released)(int) = nullptr;

// On Windows x64 the native Nullable<Vector3> value is passed indirectly.
// This is the manual move request edge, before native confirmation/callback delays.
void OnMoveFleetAction_Hook(auto original, void* self, void* position, std::int64_t node_id, std::int64_t fleet_id,
                            bool force_move)
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
      spdlog::warn("[InstantWarpConfirmation] Ctrl-click request could not be captured: {}", error.what());
    }
  }
  original(self, position, node_id, fleet_id, force_move);
}
#endif

void InstallClickOverride()
{
#if defined(_WIN32) && defined(_M_X64)
  auto  helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationManager");
  auto* method = method_contract::Resolve(
      helper.get_cls(), "OnMoveFleetAction", false, "System.Void",
      {"System.Nullable<UnityEngine.Vector3>", "System.Int64", "System.Int64", "System.Boolean"});
  // Client263: substantive 761-byte native entry, 26 complete instruction bytes
  // cover SPUD's 24-byte overwrite. Other clients retain their existing behavior.
  constexpr unsigned char window[]   = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89, 0x74,
                                        0x24, 0x18, 0x48, 0x89, 0x7c, 0x24, 0x20, 0x41, 0x56, 0x48, 0x83, 0xec, 0x50};
  const auto              base       = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
  const auto*             pointer    = method_contract::Pointer(method);
  DWORD64                 image_base = 0;
  const auto*             entry =
      pointer ? RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(pointer), &image_base, nullptr) : nullptr;
  mouse_held     = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButton(System.Int32)");
  mouse_released = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButtonUp(System.Int32)");
  if (!base || !pointer || !entry || !mouse_held || !mouse_released
      || reinterpret_cast<uintptr_t>(pointer) != base + 0x1269b30
      || image_base + entry->BeginAddress != reinterpret_cast<uintptr_t>(pointer)
      || entry->EndAddress - entry->BeginAddress != 761 || std::memcmp(pointer, window, sizeof(window)) != 0) {
    spdlog::warn("[InstantWarpConfirmation] Ctrl-click hook contract unavailable; override disabled");
    return;
  }
  click_override_available = SPUD_STATIC_DETOUR(method->methodPointer, OnMoveFleetAction_Hook) != nullptr;
  spdlog::info("[InstantWarpConfirmation] Ctrl-click choice override {}",
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
{ return click_override_available && Key::HasCtrl(); }

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
