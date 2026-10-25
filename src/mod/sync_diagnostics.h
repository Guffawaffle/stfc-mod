#pragma once

#include <nlohmann/json_fwd.hpp>
#include <string_view>

// Forward an already-enabled observation. No producer/probe is activated here.
// Delivery is bounded and best effort; there is no diagnostic outbox or ACK log.
void PublishDiagnosticEvent(std::string_view stream, const nlohmann::json& event) noexcept;
