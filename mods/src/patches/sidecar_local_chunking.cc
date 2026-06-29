#include "patches/sidecar_local_chunking.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <string>

namespace
{
using json = nlohmann::json;

constexpr char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string required_string_field(const json& object, std::string_view key)
{
  if (!object.is_object()) {
    return {};
  }

  const auto iterator = object.find(key);
  if (iterator == object.end() || !iterator->is_string()) {
    return {};
  }

  return iterator->get<std::string>();
}

std::string base64_encode(std::string_view bytes)
{
  std::string encoded;
  encoded.reserve(((bytes.size() + 2) / 3) * 4);

  for (size_t index = 0; index < bytes.size(); index += 3) {
    const auto remaining = std::min<size_t>(3, bytes.size() - index);
    const auto byte0 = static_cast<unsigned char>(bytes[index]);
    const auto byte1 = remaining > 1 ? static_cast<unsigned char>(bytes[index + 1]) : 0U;
    const auto byte2 = remaining > 2 ? static_cast<unsigned char>(bytes[index + 2]) : 0U;
    const auto block = (byte0 << 16) | (byte1 << 8) | byte2;

    encoded.push_back(kBase64Alphabet[(block >> 18) & 0x3f]);
    encoded.push_back(kBase64Alphabet[(block >> 12) & 0x3f]);
    encoded.push_back(remaining > 1 ? kBase64Alphabet[(block >> 6) & 0x3f] : '=');
    encoded.push_back(remaining > 2 ? kBase64Alphabet[block & 0x3f] : '=');
  }

  return encoded;
}
} // namespace

namespace sidecar_local_chunking
{
bool EnvelopeRequiresChunking(std::string_view serialized_envelope, size_t threshold_bytes)
{
  return threshold_bytes > 0 && serialized_envelope.size() > threshold_bytes;
}

std::vector<json> BuildTransportChunkEnvelopes(const json& original_envelope,
                                               std::string_view serialized_envelope,
                                               size_t           chunk_data_bytes)
{
  if (!original_envelope.is_object() || serialized_envelope.empty() || chunk_data_bytes == 0) {
    return {};
  }

  const auto original_kind     = required_string_field(original_envelope, "kind");
  const auto original_batch_id = required_string_field(original_envelope, "batchId");
  const auto produced_at       = required_string_field(original_envelope, "producedAt");
  const auto session_id        = required_string_field(original_envelope, "sessionId");
  const auto source            = required_string_field(original_envelope, "source");
  const auto mod_version       = required_string_field(original_envelope, "modVersion");
  if (original_kind.empty() || original_batch_id.empty() || produced_at.empty() || session_id.empty() || source.empty()
      || mod_version.empty()) {
    return {};
  }

  const auto chunk_count = (serialized_envelope.size() + chunk_data_bytes - 1) / chunk_data_bytes;
  std::vector<json> chunk_envelopes;
  chunk_envelopes.reserve(chunk_count);

  for (size_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
    const auto offset = chunk_index * chunk_data_bytes;
    const auto length = std::min(chunk_data_bytes, serialized_envelope.size() - offset);
    const auto chunk  = serialized_envelope.substr(offset, length);

    chunk_envelopes.push_back(json{
        {"protocolVersion", "stfc.sidecar.ingest.v1"},
        {"kind", std::string{kTransportChunkKind}},
        {"batchId", std::format("{}:chunk:{}", original_batch_id, chunk_index + 1)},
        {"producedAt", produced_at},
        {"sessionId", session_id},
        {"source", source},
        {"modVersion", mod_version},
        {"payloadProtocol", std::string{kTransportChunkPayloadProtocol}},
        {"payload",
         {
             {"schemaVersion", std::string{kTransportChunkPayloadProtocol}},
             {"chunkGroupId", original_batch_id},
             {"chunkIndex", chunk_index},
             {"chunkCount", chunk_count},
             {"totalBytes", serialized_envelope.size()},
             {"originalKind", original_kind},
             {"originalBatchId", original_batch_id},
             {"chunkEncoding", std::string{kTransportChunkPayloadEncoding}},
             {"chunkBase64", base64_encode(chunk)},
         }},
    });
  }

  return chunk_envelopes;
}
} // namespace sidecar_local_chunking
