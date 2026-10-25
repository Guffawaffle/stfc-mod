#pragma once

#include "keyboard_layout_mapping.h"
#include <string_view>

namespace keyboard_layout
{
// Accepted configuration only. No Unity access or native subscription occurs here.
void          Configure(std::string_view mode);
void          RegisterShortcut(KeyCode key) noexcept;
ResolvedChord ResolveChord(KeyCode configured);
ResolvedChord DescribeChord(KeyCode configured);
KeyCode       CaptureIdentity(KeyCode physical, bool shift);
} // namespace keyboard_layout
