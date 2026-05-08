#pragma once

#include <string>

namespace config_redaction
{
[[nodiscard]] std::string mask_token_for_log(const std::string& token);
[[nodiscard]] std::string redact_secret_for_runtime_snapshot(const std::string& secret);
[[nodiscard]] std::string mask_proxy_userinfo(const std::string& proxy);
} // namespace config_redaction
