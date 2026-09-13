#pragma once
namespace mod_settings
{
class PageCatalog;
void RegisterShortcutPages(PageCatalog& catalog);
void SetShortcutPresentationObserver(void (*observer)());
} // namespace mod_settings
