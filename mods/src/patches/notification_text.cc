/**
 * @file notification_text.cc
 * @brief Pure notification text cleanup helpers.
 */
#include "patches/notification_text.h"

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
      auto end = text.find('>', i);
      if (end != std::string_view::npos) {
        i = end + 1;
        continue;
      }
    }

    result += text[i++];
  }

  return result;
}

std::string notification_choose_body(std::string_view parsed_body,
                                     std::string_view formatted_localized_body,
                                     std::string_view raw_localized_body,
                                     std::string_view fallback)
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
