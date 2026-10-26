#pragma once

#include "page_catalog.h"
#include "settings_search.h"
#include <cstdint>
#include <string_view>

namespace mod_settings
{
// Register after all owner models and shortcut pages, before the native catalog freezes.
void               RegisterMaintenanceTools(PageCatalog& catalog);
void               AppendMaintenanceNotices(std::vector<PageCatalog::Page>& pages);
const std::string& SettingsSearchQuery();
std::uint64_t      SettingsSearchRevision();
void               SetSettingsSearchQuery(std::string text);
void               SetSettingsSearchError(std::string text);
bool               FilterMaintenanceSearch(const PageCatalog::Page& page, std::string_view item);
} // namespace mod_settings
