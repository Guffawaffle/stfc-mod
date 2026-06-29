#include "test_pure_common.h"

#include "patches/sidecar_local_chunking.h"

#include <array>
#include <string>

namespace
{
using json = nlohmann::json;

int decode_base64_char(const char value)
{
  if (value >= 'A' && value <= 'Z') {
    return value - 'A';
  }
  if (value >= 'a' && value <= 'z') {
    return value - 'a' + 26;
  }
  if (value >= '0' && value <= '9') {
    return value - '0' + 52;
  }
  if (value == '+') {
    return 62;
  }
  if (value == '/') {
    return 63;
  }

  return -1;
}

std::string decode_base64(std::string_view encoded)
{
  std::string decoded;
  decoded.reserve((encoded.size() / 4) * 3);

  for (size_t index = 0; index < encoded.size(); index += 4) {
    const auto c0 = decode_base64_char(encoded[index]);
    const auto c1 = decode_base64_char(encoded[index + 1]);
    const auto c2 = encoded[index + 2] == '=' ? 0 : decode_base64_char(encoded[index + 2]);
    const auto c3 = encoded[index + 3] == '=' ? 0 : decode_base64_char(encoded[index + 3]);
    const auto block = (c0 << 18) | (c1 << 12) | (c2 << 6) | c3;

    decoded.push_back(static_cast<char>((block >> 16) & 0xff));
    if (encoded[index + 2] != '=') {
      decoded.push_back(static_cast<char>((block >> 8) & 0xff));
    }
    if (encoded[index + 3] != '=') {
      decoded.push_back(static_cast<char>(block & 0xff));
    }
  }

  return decoded;
}
} // namespace

TEST_SUITE("sidecar_local_chunking")
{
  TEST_CASE("chunk envelopes preserve the original serialized ingest envelope losslessly")
  {
    const auto original_envelope = json{
        {"protocolVersion", "stfc.sidecar.ingest.v1"},
        {"kind", "battle.events"},
        {"batchId", "battle.events-42"},
        {"producedAt", "2026-06-29T03:25:00.000Z"},
        {"sessionId", "session-alpha"},
        {"source", "stfc-community-mod"},
        {"modVersion", "2.7.0-test"},
        {"payloadProtocol", "stfc.sidecar.events.v0"},
        {"payload",
         json::array({
             json{{"type", "battle.capture"},
                  {"schemaVersion", "stfc.battle.capture.v1"},
                  {"capture", {{"notes", "naive caf\u00e9 \u0411\u043e\u0439"}}}},
             json{{"type", "battle.report"}, {"schemaVersion", "stfc.sidecar.battle-report.v0"}},
         })},
    };

    const auto serialized = original_envelope.dump();
    REQUIRE(sidecar_local_chunking::EnvelopeRequiresChunking(serialized, 32));

    const auto chunk_envelopes = sidecar_local_chunking::BuildTransportChunkEnvelopes(original_envelope, serialized, 32);
    REQUIRE(chunk_envelopes.size() > 1);

    std::string reassembled;
    for (size_t index = 0; index < chunk_envelopes.size(); ++index) {
      const auto& chunk = chunk_envelopes[index];
      CHECK(chunk["protocolVersion"] == "stfc.sidecar.ingest.v1");
      CHECK(chunk["kind"] == "transport.chunk");
      CHECK(chunk["payloadProtocol"] == "stfc.sidecar.ingest.chunk.v1");
      CHECK(chunk["payload"]["schemaVersion"] == "stfc.sidecar.ingest.chunk.v1");
      CHECK(chunk["payload"]["chunkGroupId"] == "battle.events-42");
      CHECK(chunk["payload"]["originalKind"] == "battle.events");
      CHECK(chunk["payload"]["originalBatchId"] == "battle.events-42");
      CHECK(chunk["payload"]["chunkEncoding"] == "base64");
      CHECK(chunk["payload"]["chunkIndex"] == index);
      CHECK(chunk["payload"]["chunkCount"] == chunk_envelopes.size());
      CHECK(chunk["payload"]["totalBytes"] == serialized.size());

      reassembled += decode_base64(chunk["payload"]["chunkBase64"].get<std::string>());
    }

    CHECK(reassembled == serialized);
  }

  TEST_CASE("chunking helper ignores incomplete source envelopes")
  {
    const auto invalid_envelope = json{
        {"protocolVersion", "stfc.sidecar.ingest.v1"},
        {"kind", "battle.events"},
    };

    const auto chunk_envelopes = sidecar_local_chunking::BuildTransportChunkEnvelopes(invalid_envelope, invalid_envelope.dump(), 64);
    CHECK(chunk_envelopes.empty());
  }
}
