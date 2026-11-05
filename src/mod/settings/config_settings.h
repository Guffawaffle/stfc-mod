#pragma once
#include "page_catalog.h"
#include <span>
#include <variant>

namespace mod_settings
{
// Exact-entry adapters use these canonical models, not the convenience slider's restricted reader.
struct ConfigNumberControl {
  std::string                                            page;
  std::variant<ValueSetting<int>*, ValueSetting<float>*> value;
  double                                                 minimum, maximum;
};

void                                 RegisterConfigSettings(PageCatalog& catalog);
std::span<const ConfigNumberControl> ConfigNumberControls();
} // namespace mod_settings
