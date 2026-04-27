#pragma once

#include "MonoSingleton.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

struct GameWorldManager : MonoSingleton<GameWorldManager> {
  friend struct MonoSingleton<GameWorldManager>;

public:
  bool TryGetGalaxyLocaId(int64_t node_id, int64_t* galaxy_loca_id)
  {
    static auto method = get_class_helper().GetMethod<bool(GameWorldManager*, int64_t, int64_t*)>("TryGetGalaxyLocaId");
    if (!method || !galaxy_loca_id) {
      return false;
    }

    return method(this, node_id, galaxy_loca_id);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "GameWorldManager");
    return class_helper;
  }
};