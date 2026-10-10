#pragma once

#include <il2cpp/il2cpp_helper.h>

namespace background_layer_science
{
// Let the existing ambient-sky discovery use the same artwork and clipping
// bounds while science hides its renderer. All other visibility gates remain.
bool IsHiddenRenderer(Il2CppObject *renderer);
}
