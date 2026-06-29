#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace sidecar_local_chunking
{
inline constexpr std::string_view kTransportChunkKind            = "transport.chunk";
inline constexpr std::string_view kTransportChunkPayloadProtocol = "stfc.sidecar.ingest.chunk.v1";
inline constexpr std::string_view kTransportChunkPayloadEncoding = "base64";
// Keep local chunking aligned with the existing Majel per-event cap so large
// battle batches switch transport modes before they become oversized elsewhere.
inline constexpr size_t           kChunkingThresholdBytes        = 256 * 1024;
inline constexpr size_t           kChunkDataBytes                = 64 * 1024;

bool EnvelopeRequiresChunking(std::string_view serialized_envelope,
                              size_t           threshold_bytes = kChunkingThresholdBytes);

std::vector<nlohmann::json> BuildTransportChunkEnvelopes(const nlohmann::json& original_envelope,
                                                         std::string_view       serialized_envelope,
                                                         size_t                 chunk_data_bytes = kChunkDataBytes);
} // namespace sidecar_local_chunking
