/**
 * @file notification_text.h
 * @brief Pure notification text cleanup helpers.
 */
#pragma once

#include <string>
#include <string_view>

std::string notification_normalize_body(const char* body);
std::string notification_flatten_text(std::string_view text);
std::string notification_escape_text_for_log(std::string_view text);
std::string notification_strip_unity_rich_text(std::string_view text);
std::string notification_choose_body(std::string_view parsed_body,
                                     std::string_view formatted_localized_body,
                                     std::string_view raw_localized_body,
                                     std::string_view fallback = "(no details available)");
