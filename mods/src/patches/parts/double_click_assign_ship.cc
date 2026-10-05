#include "config.h"
#include "errormsg.h"
#include "patches/key.h"
#include "patches/swap_ship_pin_input.h"
#include "settings/upstream_features.h"

#include "prime/AssignShipsWidget.h"
#include "prime/CanvasController.h"
#include "prime/KeyCode.h"
#include "prime/ShipTileWidget.h"

#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <chrono>

#include "double_click_assign_ship.h"

namespace
{

bool double_click_installed = false;

constexpr auto kDoubleClickWindow = std::chrono::milliseconds(400);

FleetPlayerData*                      g_last_clicked_ship = nullptr;
std::chrono::steady_clock::time_point g_last_click_time{};

void PressAssignButton()
{
  for (auto widget : ObjectFinder<AssignShipsWidget>::GetAll()) {
    if (!widget)
      continue;

    auto canvas = GetCanvasControllerFromComponent(widget);
    if (!canvas || !canvas->Visible() || !widget->isActiveAndEnabled)
      continue;

    auto* buttonWrapper = widget->_assignButton;
    auto* buttonWidget  = buttonWrapper ? buttonWrapper->Widget : nullptr;
    auto* listener      = buttonWidget ? buttonWidget->SemaphoreListener : nullptr;
    auto* button        = listener ? listener->TheButton : nullptr;
    if (button) {
      button->Press();
    }
    return;
  }
}

void ShipTileWidget_HandleOnClick_Hook(auto original, ShipTileWidget* _this)
{
  if (_this && swap_ship_pin_input::HandleTileClick(_this)) {
    g_last_clicked_ship = nullptr;
    return;
  }
  original(_this);

  if (!Config::Get().installDoubleClickAssignShipHooks || !Config::Get().double_click_to_assign_ship)
    return;

  auto* ship = _this ? _this->Context : nullptr;
  if (!ship)
    return;

  const auto now             = std::chrono::steady_clock::now();
  const bool is_double_click = ship == g_last_clicked_ship && (now - g_last_click_time) <= kDoubleClickWindow;

  g_last_clicked_ship = ship;
  g_last_click_time   = now;

  if (!is_double_click)
    return;

  g_last_clicked_ship = nullptr; // consume, so a triple/quadruple click doesn't re-trigger immediately
  PressAssignButton();
}

} // namespace

void AssignShipEnterKeyUpdate()
{
  if (!Config::Get().installDoubleClickAssignShipHooks || !Config::Get().double_click_to_assign_ship)
    return;
  if (!Key::Pressed(KeyCode::Return) && !Key::Pressed(KeyCode::KeypadEnter))
    return;
  if (Key::IsInputFocused())
    return;

  PressAssignButton();
}

void InstallDoubleClickAssignShipHooks()
{
  auto helper = ShipTileWidget::get_class_helper();
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Ships", "ShipTileWidget");
    return;
  }

  auto method = helper.GetMethod("HandleOnClick", 0);
  if (!method) {
    ErrorMsg::MissingMethod("ShipTileWidget", "HandleOnClick");
    return;
  }

  double_click_installed = SPUD_STATIC_DETOUR(method, ShipTileWidget_HandleOnClick_Hook);
  if (!double_click_installed)
    spdlog::error("[PinnedShipSort] failed to install ShipTileWidget.HandleOnClick detour");
  else
    spdlog::info("[PinnedShipSort] installed shared ShipTileWidget.HandleOnClick detour");
}

bool mod_settings::DoubleClickAssignShipAvailable()
{ return double_click_installed && Config::Get().installDoubleClickAssignShipHooks; }
