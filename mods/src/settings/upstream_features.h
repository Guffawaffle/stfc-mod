#pragma once
#include "page_catalog.h"

namespace mod_settings
{
bool ArtifactExchangeAvailable();
bool CargoFormatAvailable();
bool OfficerSortAvailable();
bool OfficerPresetReorderAvailable();
bool DoubleClickAssignShipAvailable();
void RegisterUpstreamFeaturePages(PageCatalog& catalog);
} // namespace mod_settings
