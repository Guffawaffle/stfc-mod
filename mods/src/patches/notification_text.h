/**
 * @file notification_text.h
 * @brief Pure notification text cleanup helpers.
 */
#pragma once

#include <span>
#include <string>
#include <string_view>

std::string notification_normalize_body(const char* body);
std::string notification_flatten_text(std::string_view text);
std::string notification_escape_text_for_log(std::string_view text);
std::string notification_strip_unity_rich_text(std::string_view text);
bool        notification_contains_placeholders(std::string_view text);
std::string notification_format_placeholders(std::string_view text, std::span<const std::string> parameters);
std::string notification_format_armada_created_body(std::string_view template_text, std::string_view alliance_color,
                                                    std::string_view alliance_tag, std::string_view owner_name,
                                                    std::string_view fleet_capacity,
                                                    std::string_view target_level_color, std::string_view target_level,
                                                    std::string_view target_label);
std::string notification_choose_body(std::string_view parsed_body, std::string_view formatted_localized_body,
                                     std::string_view raw_localized_body,
                                     std::string_view fallback = "(no details available)");
