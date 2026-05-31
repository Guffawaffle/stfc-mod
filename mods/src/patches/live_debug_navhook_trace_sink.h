/**
 * @file live_debug_navhook_trace_sink.h
 * @brief Bounded navhook trace-file sink used by live-debug diagnostics.
 */
#pragma once

namespace live_debug_navhook_trace_sink {

void AppendStep(const char* step,
                const char* phase,
                const void* controller = nullptr,
                const void* sender = nullptr,
                const void* callback_context = nullptr);

} // namespace live_debug_navhook_trace_sink
