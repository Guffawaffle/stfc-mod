#pragma once

#include <cstdint>
#include <il2cpp/il2cpp.h>

namespace unity
{
struct Application {
  using Class = il2cpp::Class<"UnityEngine.CoreModule", "UnityEngine", "Application">;

  static void Quit(std::int32_t code)
  { Class::Call<"Quit", void(std::int32_t)>(code); }
};
} // namespace unity
