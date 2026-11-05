#pragma once
#include "page_catalog.h"

namespace mod_settings
{
// Process-lifetime models. Registration performs no native operations or live setting mutations.
PageCatalog& BuildModPages();
} // namespace mod_settings
