#pragma once

#include <il2cpp/il2cpp_helper.h>

// Owns only new visual objects; native system artwork remains under game ownership.
namespace navigation_environment
{
void Update(Il2CppObject *camera);
void Clear();
void ApplyDrawDistance(Il2CppObject *camera);
} // namespace navigation_environment
