/**
 * @file notification_text.cc
 * @brief Pure notification text cleanup helpers.
 */
#include "patches/notification_text.h"

#include <vector>

namespace
{
bool ascii_iequals(std::string_view left, std::string_view right)
{
  if (left.size() != right.size()) {
    return false;
  }

  for (size_t i = 0; i < left.size(); ++i) {
    auto lhs = left[i];
    auto rhs = right[i];
    if (lhs >= 'A' && lhs <= 'Z') {
      lhs = static_cast<char>(lhs - 'A' + 'a');
    }
    if (rhs >= 'A' && rhs <= 'Z') {
      rhs = static_cast<char>(rhs - 'A' + 'a');
    }
    if (lhs != rhs) {
      return false;
    }
  }

  return true;
}

bool is_tag_name_char(char ch)
{ return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '-'; }

size_t find_rich_text_tag_end(std::string_view text, size_t start)
{
  char quote = '\0';
  for (size_t i = start + 1; i < text.size(); ++i) {
    const auto ch = text[i];
    if (quote != '\0') {
      if (ch == quote) {
        quote = '\0';
      }
      continue;
    }

    if (ch == '"' || ch == '\'') {
      quote = ch;
      continue;
    }

    if (ch == '>') {
      return i;
    }
  }

  return std::string_view::npos;
}

bool is_unity_rich_text_tag(std::string_view tag)
{
  while (!tag.empty() && tag.front() == ' ') {
    tag.remove_prefix(1);
  }
  while (!tag.empty() && tag.back() == ' ') {
    tag.remove_suffix(1);
  }
  if (tag.empty()) {
    return false;
  }

  if (tag.front() == '/') {
    tag.remove_prefix(1);
  }

  size_t name_length = 0;
  while (name_length < tag.size() && is_tag_name_char(tag[name_length])) {
    ++name_length;
  }
  if (name_length == 0) {
    return false;
  }

  const auto                 name         = tag.substr(0, name_length);
  constexpr std::string_view known_tags[] = {
      "align",    "allcaps",   "alpha",     "b",           "br",           "color",
      "cspace",   "font",      "i",         "indent",      "line-height",  "line-indent",
      "link",     "lowercase", "margin",    "margin-left", "margin-right", "mark",
      "material", "mspace",    "nobr",      "page",        "pos",          "rotate",
      "s",        "size",      "smallcaps", "space",       "sprite",       "style",
      "sub",      "sup",       "u",         "uppercase",   "voffset",      "width",
  };

  for (const auto known_tag : known_tags) {
    if (ascii_iequals(name, known_tag)) {
      return true;
    }
  }

  return false;
}

bool placeholder_suffix_starts(char ch)
{ return ch == ':' || ch == '.' || ch == ','; }

bool is_zero_padding_suffix(std::string_view suffix)
{
  if (suffix.empty()) {
    return false;
  }

  for (const auto ch : suffix) {
    if (ch != '0') {
      return false;
    }
  }

  return true;
}

bool is_integer_text(std::string_view text)
{
  if (text.empty()) {
    return false;
  }

  if (text.front() == '-') {
    text.remove_prefix(1);
  }
  if (text.empty()) {
    return false;
  }

  for (const auto ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
  }

  return true;
}

std::string format_placeholder_value(std::string_view parameter, std::string_view suffix)
{
  if (!is_zero_padding_suffix(suffix) || !is_integer_text(parameter)) {
    return std::string(parameter);
  }

  const auto negative = parameter.front() == '-';
  if (negative) {
    parameter.remove_prefix(1);
  }

  std::string result;
  if (negative) {
    result.push_back('-');
  }
  if (parameter.size() < suffix.size()) {
    result.append(suffix.size() - parameter.size(), '0');
  }
  result.append(parameter);
  return result;
}

size_t find_placeholder_end(std::string_view text, size_t start, size_t* placeholder_index = nullptr)
{
  if (start >= text.size() || text[start] != '{') {
    return std::string_view::npos;
  }

  auto       cursor = start + 1;
  size_t     index  = 0;
  const auto digits = cursor;
  while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
    index = (index * 10) + static_cast<size_t>(text[cursor] - '0');
    ++cursor;
  }

  if (cursor == digits) {
    return std::string_view::npos;
  }

  size_t end = cursor;
  if (cursor < text.size() && text[cursor] == '}') {
    end = cursor;
  } else if (cursor < text.size() && placeholder_suffix_starts(text[cursor])) {
    end = text.find('}', cursor + 1);
    if (end == std::string_view::npos) {
      return std::string_view::npos;
    }
  } else {
    return std::string_view::npos;
  }

  if (placeholder_index) {
    *placeholder_index = index;
  }
  return end;
}
} // namespace

std::string notification_normalize_body(const char* body)
{
  if (!body || !*body) {
    return {};
  }

  std::string normalized;
  normalized.reserve(std::string_view(body).size());

  for (size_t i = 0; body[i] != '\0'; ++i) {
    if (body[i] == '\r') {
      normalized += '\r';
      if (body[i + 1] == '\n') {
        normalized += '\n';
        ++i;
      }
      continue;
    }

    if (body[i] == '\n') {
      normalized += "\r\n";
      continue;
    }

    normalized += body[i];
  }

  return normalized;
}

std::string notification_flatten_text(std::string_view text)
{
  std::string flattened;
  flattened.reserve(text.size());

  bool last_was_space = false;
  for (char ch : text) {
    if (ch == '\r' || ch == '\n' || ch == '\t') {
      ch = ' ';
    }

    if (ch == ' ') {
      if (flattened.empty() || last_was_space) {
        continue;
      }

      last_was_space = true;
      flattened += ch;
      continue;
    }

    last_was_space = false;
    flattened += ch;
  }

  if (!flattened.empty() && flattened.back() == ' ') {
    flattened.pop_back();
  }

  return flattened;
}

std::string notification_escape_text_for_log(std::string_view text)
{
  std::string escaped;
  escaped.reserve(text.size());

  for (char ch : text) {
    switch (ch) {
      case '\r':
        escaped += "\\r";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }

  return escaped;
}

std::string notification_strip_unity_rich_text(std::string_view text)
{
  std::string result;
  result.reserve(text.size());

  size_t i = 0;
  while (i < text.size()) {
    if (text[i] == '<') {
      auto end = find_rich_text_tag_end(text, i);
      if (end != std::string_view::npos && is_unity_rich_text_tag(text.substr(i + 1, end - i - 1))) {
        i = end + 1;
        continue;
      }
    }

    result += text[i++];
  }

  return result;
}

bool notification_contains_placeholders(std::string_view text)
{
  size_t position = 0;
  while ((position = text.find('{', position)) != std::string_view::npos) {
    if (find_placeholder_end(text, position) != std::string_view::npos) {
      return true;
    }
    ++position;
  }

  return false;
}

std::string notification_format_placeholders(std::string_view text, std::span<const std::string> parameters)
{
  std::string result;
  result.reserve(text.size());

  size_t position = 0;
  while (position < text.size()) {
    if (text[position] != '{') {
      result.push_back(text[position++]);
      continue;
    }

    if (position + 1 < text.size() && text[position + 1] == '{') {
      result.append(text.substr(position, 2));
      position += 2;
      continue;
    }

    size_t index = 0;
    auto   end   = find_placeholder_end(text, position, &index);
    if (end == std::string_view::npos) {
      result.push_back(text[position++]);
      continue;
    }

    if (index < parameters.size()) {
      auto cursor = position + 1;
      while (cursor < end && text[cursor] >= '0' && text[cursor] <= '9') {
        ++cursor;
      }

      std::string_view suffix;
      if (cursor < end && placeholder_suffix_starts(text[cursor])) {
        suffix = text.substr(cursor + 1, end - cursor - 1);
      }

      result.append(format_placeholder_value(parameters[index], suffix));
    } else {
      result.append(text.substr(position, end - position + 1));
    }
    position = end + 1;
  }

  return result;
}

std::string notification_format_armada_created_body(std::string_view template_text, std::string_view alliance_color,
                                                    std::string_view alliance_tag, std::string_view owner_name,
                                                    std::string_view fleet_capacity,
                                                    std::string_view target_level_color, std::string_view target_level,
                                                    std::string_view target_label)
{
  const std::vector<std::string> parameters{
      std::string(alliance_color), std::string(alliance_tag),       std::string(owner_name),
      std::string(fleet_capacity), std::string(target_level_color), std::string(target_level),
      std::string(target_label),
  };

  return notification_strip_unity_rich_text(notification_format_placeholders(template_text, parameters));
}

std::string notification_choose_body(std::string_view parsed_body, std::string_view formatted_localized_body,
                                     std::string_view raw_localized_body, std::string_view fallback)
{
  if (!parsed_body.empty()) {
    return std::string(parsed_body);
  }

  if (!formatted_localized_body.empty()) {
    return std::string(formatted_localized_body);
  }

  auto stripped = notification_strip_unity_rich_text(raw_localized_body);
  if (!stripped.empty()) {
    return stripped;
  }

  return std::string(fallback);
}
