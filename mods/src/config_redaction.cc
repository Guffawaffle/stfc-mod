#include "config_redaction.h"

namespace config_redaction
{
std::string mask_token_for_log(const std::string& token)
{
  if (token.empty()) {
    return "<empty>";
  }

  std::string masked{token};
  if (token.size() > 21) {
    for (size_t i = 9; i < token.size() - 12; ++i) {
      if (masked[i] != '-') {
        masked[i] = '*';
      }
    }
    return masked;
  }

  if (token.size() > 8) {
    for (size_t i = 4; i < token.size() - 4; ++i) {
      if (masked[i] != '-') {
        masked[i] = '*';
      }
    }
    return masked;
  }

  for (auto& ch : masked) {
    if (ch != '-') {
      ch = '*';
    }
  }
  return masked;
}

std::string redact_secret_for_runtime_snapshot(const std::string& secret)
{ return secret.empty() ? std::string{"<empty>"} : std::string{"<redacted>"}; }

std::string mask_proxy_userinfo(const std::string& proxy)
{
  if (proxy.empty()) {
    return {};
  }

  std::string masked{proxy};
  const auto  scheme_pos      = masked.find("://");
  const auto  authority_start = scheme_pos == std::string::npos ? 0 : scheme_pos + 3;
  const auto  authority_end   = masked.find_first_of("/?#", authority_start);
  const auto  at_pos          = masked.find('@', authority_start);

  if (at_pos == std::string::npos || (authority_end != std::string::npos && at_pos > authority_end)) {
    return masked;
  }

  for (auto index = authority_start; index < at_pos; ++index) {
    if (masked[index] != ':') {
      masked[index] = '*';
    }
  }

  return masked;
}
} // namespace config_redaction
