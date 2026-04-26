/**
 * @file FleetBarViewController.h
 * @brief HUD fleet-selection bar UI controller.
 *
 * Mirrors Digit.Prime.HUD.FleetBarViewController and FleetBarContext.
 * The fleet bar is the row of fleet slot buttons at the bottom of the
 * navigation screen. This header exposes slot selection, index checking,
 * and access to the underlying fleet panel controller.
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "errormsg.h"
#include "FleetLocalViewController.h"

/** @brief Context object for the fleet bar, providing the currently selected fleet. */
struct FleetBarContext {
public:
  __declspec(property(get = __get_CurrentFleet)) void* CurrentFleet;

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetBarContext");
    return class_helper;
  }

public:
  void* __get_CurrentFleet()
  {
    static auto field = get_class_helper().GetProperty("CurrentFleet");
    return field.Get<void>(this);
  }
};

/**
 * @brief UI controller for the fleet selection bar in the HUD.
 *
 * Drives fleet slot selection, checks which index is active, and provides
 * access to the canvas context and underlying fleet panel controller.
 * Tracked by ObjectFinder for global lookup.
 */
struct FleetBarViewController {
public:
  __declspec(property(get = __get__fleetPanelController)) FleetLocalViewController* _fleetPanelController;

  void RequestSelect(int32_t index, bool simulated = false)
  {
    static auto RequestSelectWarn   = true;
    static auto RequestSelectMethod = get_class_helper().GetMethodSpecial<void(FleetBarViewController*, int32_t, bool)>(
        "RequestSelect", [](auto count, auto params) {
          if (count != 2) {
            return false;
          }
          auto p1 = params[0]->type;
          if (p1 == IL2CPP_TYPE_I4) {
            return true;
          }
          return false;
        });
    if (RequestSelectMethod) {
      RequestSelectMethod(this, index, simulated);
    } else if (RequestSelectWarn) {
      RequestSelectWarn = false;
      ErrorMsg::MissingMethod("FleetBarViewController", "RequestSelect");
    }
  }

  bool IsIndexSelected(int32_t index)
  {
    static auto IsIndexSelectedWarn = true;
    static auto IsIndexSelectedMethod =
        get_class_helper().GetMethod<bool(FleetBarViewController*, int32_t)>("IsIndexSelected");

    if (IsIndexSelectedMethod) {
      return IsIndexSelectedMethod(this, index);
    } else if (IsIndexSelectedWarn) {
      IsIndexSelectedWarn = false;
      ErrorMsg::MissingMethod("FleetBarViewController", "IsIndexSelected");
    }

    return false;
  }

  void ElementAction(int32_t index)
  {
    static auto ElementActionWarn   = true;
    static auto ElementActionMethod =
        get_class_helper().GetMethod<void(FleetBarViewController*, int32_t)>("ElementAction");
    if (ElementActionMethod) {
      ElementActionMethod(this, index);
    } else if (ElementActionWarn) {
      ElementActionWarn = false;
      ErrorMsg::MissingMethod("FleetBarViewController", "ElementAction");
    }
  }

  void TogglePanel()
  {
    static auto TogglePanelWarn   = true;
    static auto TogglePanelMethod =
        get_class_helper().GetMethod<void(FleetBarViewController*)>("TogglePanel");
    if (TogglePanelMethod) {
      TogglePanelMethod(this);
    } else if (TogglePanelWarn) {
      TogglePanelWarn = false;
      ErrorMsg::MissingMethod("FleetBarViewController", "TogglePanel");
    }
  }

  FleetBarContext* CanvasContext()
  {
    static auto n = get_class_helper().GetProperty("CanvasContext");
    return n.GetRaw<FleetBarContext>(this);
  }

private:
  friend class ObjectFinder<FleetBarViewController>;

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController");
    return class_helper;
  }

public:
  FleetLocalViewController* __get__fleetPanelController()
  {
    static auto field = get_class_helper().GetField("_fleetPanelController").offset();
    return *(FleetLocalViewController**)((uintptr_t)this + field);
  }
};
