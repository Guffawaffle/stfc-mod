#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace game_localization
{
void Initialize();

[[nodiscard]] std::string LocalizeContext(void* locale_text_context, bool localize_text_parameters = true);
[[nodiscard]] std::string LocalizeIdentifier(std::string_view identifier, std::string_view category, int64_t parameter);
[[nodiscard]] std::string LocalizeIdentifier(std::string_view identifier, std::string_view category,
                                             std::string_view parameter);
} // namespace game_localization
