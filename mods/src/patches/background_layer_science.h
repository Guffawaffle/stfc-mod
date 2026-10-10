#pragma once

#include <il2cpp/il2cpp_helper.h>
#include <string>

namespace background_layer_science
{
// Keep ambient source discovery stable when a science comparison hides a renderer.
void Install();
bool Hide(Il2CppObject *flat); // Handles visibility and bypasses fr_scale for science comparisons.
bool AmbientEnabled();
void Next();
void ToggleAmbient();
void UpdatePanel(const std::string &status, bool visible);
bool IsHiddenRenderer(Il2CppObject *renderer);
} // namespace background_layer_science
