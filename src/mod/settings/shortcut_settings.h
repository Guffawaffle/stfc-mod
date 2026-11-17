#pragma once

#include "shortcut_draft.h"
#include "shortcuts.h"

namespace mod_settings
{
class PageCatalog;
void RegisterShortcutPages(PageCatalog& catalog);
// Reuses the live dispatcher and its global replacement generation; no matching engine lives in Settings.
ValueSetting<ShortcutList>* ShortcutEditorSetting(Action action);
std::string                 ShortcutUnavailableReason(Action action);
void                        CancelShortcutEditing();
void                        SetShortcutPresentationObserver(void (*observer)());
} // namespace mod_settings
