#pragma once

#include <il2cpp/il2cpp_helper.h>
#include <string>

enum class NodeDepth;

namespace background_layer_science
{
// Temporary native scenery visibility controls.
void Install();
bool Hide(Il2CppObject *flat,
          Il2CppObject *view = nullptr); // Observe the native backdrop; apply the current renderer selection.
void Next();
void ToggleBoundary();
void RefreshBoundary(Il2CppObject *camera);
void Reset();
void ObserveCamera(Il2CppObject *camera, NodeDepth depth);
void RestoreCamera(); // Release the old snapshot before native view reconfiguration.
void UpdatePanel(const std::string &status, bool visible);
bool IsHiddenRenderer(Il2CppObject *renderer);
} // namespace background_layer_science
