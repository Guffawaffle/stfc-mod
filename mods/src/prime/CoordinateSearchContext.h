#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>

struct CoordinateSearchContext {
  static CoordinateSearchContext* Create()
  {
    auto& helper = get_class_helper();
    if (!helper.isValidHelper()) {
      spdlog::error("[CoordSearch] CoordinateSearchContext class lookup failed");
      return nullptr;
    }

    auto* context = helper.New<CoordinateSearchContext>();
    if (!context) {
      spdlog::error("[CoordSearch] CoordinateSearchContext allocation failed");
      return nullptr;
    }

    static auto ctor = helper.GetMethod<void(CoordinateSearchContext*)>(".ctor");
    if (!ctor) {
      spdlog::error("[CoordSearch] CoordinateSearchContext constructor lookup failed");
      return nullptr;
    }
    ctor(context);
    return context;
  }

  bool InitializeDefaults()
  {
    auto* zero  = il2cpp_string_new("0");
    auto* empty = il2cpp_string_new("");
    if (!zero || !empty) {
      return false;
    }

    static auto set_x =
        get_class_helper().GetMethod<void(CoordinateSearchContext*, Il2CppString*)>("set_DefaultXCoordinate");
    static auto set_y =
        get_class_helper().GetMethod<void(CoordinateSearchContext*, Il2CppString*)>("set_DefaultYCoordinate");
    static auto set_system =
        get_class_helper().GetMethod<void(CoordinateSearchContext*, Il2CppString*)>("set_DefaultSystemInput");
    if (!set_x || !set_y || !set_system) {
      spdlog::error("[CoordSearch] CoordinateSearchContext default setter lookup failed");
      return false;
    }

    set_x(this, zero);
    set_y(this, zero);
    set_system(this, empty);
    return true;
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Bookmarks", "CoordinateSearchContext");
    return class_helper;
  }
};
