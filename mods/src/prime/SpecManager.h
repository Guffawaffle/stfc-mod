#pragma once

#include "errormsg.h"
#include "BuffSpec.h"
#include "ComponentSpec.h"
#include "ForbiddenTechSpec.h"
#include "HullSpec.h"
#include "MonoSingleton.h"
#include "OfficerSpec.h"
#include "ResourceSpec.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct SpecManager : MonoSingleton<SpecManager> {
  friend struct MonoSingleton<SpecManager>;

public:
  HullSpec* GetHull(int64_t id)
  {
    static auto method = get_class_helper().GetMethod<HullSpec*(SpecManager*, int64_t)>("GetHull");
    static auto warn   = true;

    if (method) {
      return method(this, id);
    }

    if (warn) {
      warn = false;
      ErrorMsg::MissingMethod("SpecManager", "GetHull");
    }

    return nullptr;
  }

  ResourceSpec* GetResourceSpec(int64_t id)
  {
    static auto method = get_class_helper().GetMethod<ResourceSpec*(SpecManager*, int64_t)>("GetResourceSpec");
    static auto warn   = true;

    if (method) {
      return method(this, id);
    }

    if (warn) {
      warn = false;
      ErrorMsg::MissingMethod("SpecManager", "GetResourceSpec");
    }

    return nullptr;
  }

  ComponentSpec* SearchForSpec(int64_t id)
  {
    static auto method = get_class_helper().GetMethod<ComponentSpec*(SpecManager*, int64_t)>("SearchForSpec");
    static auto warn   = true;

    if (method) {
      return method(this, id);
    }

    if (warn) {
      warn = false;
      ErrorMsg::MissingMethod("SpecManager", "SearchForSpec");
    }

    return nullptr;
  }

  OfficerSpec* GetOfficerSpec(int64_t id)
  {
    static auto method = get_class_helper().GetMethod<OfficerSpec*(SpecManager*, int64_t)>("GetOfficerSpec");
    static auto warn   = true;

    if (method) {
      return method(this, id);
    }

    if (warn) {
      warn = false;
      ErrorMsg::MissingMethod("SpecManager", "GetOfficerSpec");
    }

    return nullptr;
  }

  BuffSpec* GetBuffSpec(int64_t id, bool ignore_warning = false)
  {
    static auto method = get_class_helper().GetMethod<BuffSpec*(SpecManager*, int64_t, bool)>("GetBuffSpec");
    static auto warn   = true;

    if (method) {
      return method(this, id, ignore_warning);
    }

    if (warn) {
      warn = false;
      ErrorMsg::MissingMethod("SpecManager", "GetBuffSpec");
    }

    return nullptr;
  }

  ForbiddenTechSpec* GetForbiddenTechSpec(int64_t id)
  {
    static auto method = get_class_helper().GetMethod<ForbiddenTechSpec*(SpecManager*, int64_t)>("GetForbiddenTechSpec");
    static auto warn   = true;

    if (method) {
      return method(this, id);
    }

    if (warn) {
      warn = false;
      ErrorMsg::MissingMethod("SpecManager", "GetForbiddenTechSpec");
    }

    return nullptr;
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation.Managers", "SpecManager");
    return class_helper;
  }
};