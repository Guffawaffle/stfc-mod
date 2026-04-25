#pragma once

#include <string>
#include <string_view>

/**
 * @brief Parse a raw request payload, execute the live debug command, and return the response JSON text.
 */
std::string live_debug_handle_request_text(std::string_view request_text);