#pragma once

#include "MonoSingleton.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct ActivatedAbilityManager : MonoSingleton<ActivatedAbilityManager> {
  friend struct MonoSingleton<ActivatedAbilityManager>;

public:
  int64_t GetActivatedAbilityLocaId(int64_t ability_id)
  {
    static auto method = get_class_helper().GetMethod<int64_t(ActivatedAbilityManager*, int64_t)>("GetActivatedAbilityLocaId");
    if (!method) {
      return 0;
    }

    return method(this, ability_id);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.ActivatedAbilities", "ActivatedAbilityManager");
    return class_helper;
  }
};