#pragma once
#include <il2cpp/runtime.h>

namespace galaxy_housing
{
void Install(Il2CppClass* star);
bool Available();
bool Enabled();
void Hide(void* widget);
void Remove(void* widget);
void Update(void* widget, int detail);
void Refresh();
} // namespace galaxy_housing
