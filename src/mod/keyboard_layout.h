#pragma once

#include "keyboard_layout_diagnostics.h"
#include "keyboard_layout_mapping.h"
#include <string_view>

namespace keyboard_layout
{
// Accepted configuration only. No Unity access or native subscription occurs here.
void Configure(std::string_view mode);
void RegisterShortcut(KeyCode key) noexcept;
// Game-thread snapshots only; these never refresh mappings or consume pending diagnostics.
// Include generation zero at startup, and acknowledge later generations only after a successful vars write.
DiagnosticSnapshot ReadDiagnostics();
bool               DiagnosticsChanged(uint64_t accepted_generation) noexcept;
ResolvedChord      ResolveChord(KeyCode configured);
ResolvedChord      DescribeChord(KeyCode configured);
KeyCode            CaptureIdentity(KeyCode physical, bool shift);
} // namespace keyboard_layout
