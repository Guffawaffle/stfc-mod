#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "NotificationIncomingFleetParams.h"
#include "ViewController.h"

struct StationWarningViewController
    : public ViewController<NotificationIncomingFleetParams, StationWarningViewController> {
private:
  friend class ObjectFinder<StationWarningViewController>;
  friend struct ViewController<NotificationIncomingFleetParams, StationWarningViewController>;

public:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "StationWarningViewController");
    return class_helper;
  }
};