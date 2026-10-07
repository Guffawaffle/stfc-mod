#pragma once

#include <il2cpp/il2cpp_helper.h>

// Raw play-dev experiment. Owns only new visual objects; never edits source artwork.
namespace navigation_environment_poc
{
void Update(Il2CppObject *camera);
void Clear();
void ApplyDrawDistance(Il2CppObject *camera);
void ValidateRuntime();
} // namespace navigation_environment_poc
