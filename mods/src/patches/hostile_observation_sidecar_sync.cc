/**
 * @file hostile_observation_sidecar_sync.cc
 * @brief Sidecar-local observed hostile event bridge.
 */
#include "patches/hostile_observation_sidecar_sync.h"

#include "patches/sidecar_local_ingest.h"
#include "version.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>

namespace
{
using json = nlohmann::json;

constexpr std::string_view kEventProtocolVersion  = "stfc.sidecar.events.v0";
constexpr std::string_view kObservedHostileSchema = "stfc.observed.hostile.v0";

int64_t current_time_millis_utc()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string current_time_iso_utc()
{
  const auto now      = std::chrono::system_clock::now();
  const auto now_time = std::chrono::system_clock::to_time_t(now);

  std::tm utc{};
#if _WIN32
  gmtime_s(&utc, &now_time);
#else
  gmtime_r(&now_time, &utc);
#endif

  char buffer[sizeof("2026-05-17T22:00:00Z")];
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
    return {};
  }

  return buffer;
}

void erase_if_empty_string(json& object, std::string_view key)
{
  auto it = object.find(std::string(key));
  if (it == object.end()) {
    return;
  }

  if (it->is_null()) {
    object.erase(it);
    return;
  }

  if (it->is_string() && it->get_ref<const std::string&>().empty()) {
    object.erase(it);
  }
}

void stringify_optional_id(json& object, std::string_view key)
{
  auto it = object.find(std::string(key));
  if (it == object.end()) {
    return;
  }

  if (it->is_null()) {
    object.erase(it);
    return;
  }

  if (it->is_string()) {
    const auto& value = it->get_ref<const std::string&>();
    if (value.empty() || value == "0") {
      object.erase(it);
    }
    return;
  }

  if (it->is_number_integer()) {
    const auto value = it->get<int64_t>();
    if (value <= 0) {
      object.erase(it);
      return;
    }

    *it = std::to_string(value);
    return;
  }

  if (it->is_number_unsigned()) {
    const auto value = it->get<uint64_t>();
    if (value == 0) {
      object.erase(it);
      return;
    }

    *it = std::to_string(value);
    return;
  }

  object.erase(it);
}

void erase_if_negative(json& object, std::string_view key)
{
  auto it = object.find(std::string(key));
  if (it == object.end()) {
    return;
  }

  if (it->is_number_integer() && it->get<int64_t>() < 0) {
    object.erase(it);
  }
}

json normalize_observation(json observation, const int64_t observed_at_unix_ms)
{
  if (!observation.is_object()) {
    return nullptr;
  }

  observation.erase("signature");
  observation["observedAtUnixMs"] = observed_at_unix_ms;

  stringify_optional_id(observation, "runtimeFleetId");
  stringify_optional_id(observation, "hullId");
  stringify_optional_id(observation, "locationTranslationId");
  stringify_optional_id(observation, "userId");

  erase_if_negative(observation, "fleetTypeValue");
  erase_if_negative(observation, "hullTypeValue");
  erase_if_negative(observation, "threatLevel");
  erase_if_negative(observation, "visibilityStateValue");

  erase_if_empty_string(observation, "sourceSurface");
  erase_if_empty_string(observation, "confidence");
  erase_if_empty_string(observation, "fleetTypeName");
  erase_if_empty_string(observation, "hullName");
  erase_if_empty_string(observation, "hullTypeName");
  erase_if_empty_string(observation, "visibilityStateName");
  erase_if_empty_string(observation, "poiPointer");
  erase_if_empty_string(observation, "widgetPointer");
  erase_if_empty_string(observation, "controllerPointer");

  if (!observation.contains("sourceSurface") || !observation["sourceSurface"].is_string()
      || !observation.contains("confidence") || !observation["confidence"].is_string()) {
    return nullptr;
  }

  return observation;
}

json build_observed_hostile_event(const json& observation)
{
  const auto observed_at_unix_ms = current_time_millis_utc();
  auto       normalized          = normalize_observation(observation, observed_at_unix_ms);
  if (normalized.is_null()) {
    return nullptr;
  }

  return json{{"protocolVersion", kEventProtocolVersion},
              {"type", "observed.hostile"},
              {"schemaVersion", kObservedHostileSchema},
              {"timestamp", current_time_iso_utc()},
              {"source", "stfc-community-mod"},
              {"modVersion", VER_FILE_VERSION_STR},
              {"observation", std::move(normalized)}};
}
} // namespace

bool hostile_observation_sidecar_delivery_enabled()
{ return sidecar_local_ingest::ObservedHostilesEnabled(); }

void hostile_observation_sidecar_emit(const nlohmann::json& observation)
{
  if (!hostile_observation_sidecar_delivery_enabled()) {
    return;
  }

  auto event = build_observed_hostile_event(observation);
  if (event.is_null()) {
    spdlog::debug("[HostileObservation] skipped sidecar delivery for invalid observation payload");
    return;
  }

  sidecar_local_ingest::EnqueueObservedHostileEvents(json::array({std::move(event)}));
}
