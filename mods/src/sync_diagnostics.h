#pragma once
#include <string_view>
#include <nlohmann/json_fwd.hpp>
// Forward an already-enabled diagnostic observation through ordinary sync delivery.
// No probe activation, new hook, outbox, retry, or game-thread HTTP is introduced.
void PublishDiagnosticEvent(std::string_view stream, const nlohmann::json& event) noexcept;
