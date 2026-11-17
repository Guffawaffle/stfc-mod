#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

enum class FleetNotificationKind : uint8_t {
  ArrivedInSystem = 0,
  ArrivedAtDestination,
  StartedMining,
  NodeDepleted,
  Docked,
  RepairComplete,
  MinerOpc,
  Count,
};

using FleetNotificationMask = uint32_t;

struct FleetNotificationCatalogEntry {
  FleetNotificationKind kind;
  std::string_view      config_name;
  std::string_view      audio_config_name;
};

inline constexpr std::array kFleetNotificationCatalog{
    FleetNotificationCatalogEntry{FleetNotificationKind::ArrivedInSystem, "ArrivedInSystem",
                                  "alert_fleet_arrived_in_system"},
    FleetNotificationCatalogEntry{FleetNotificationKind::ArrivedAtDestination, "ArrivedAtDestination",
                                  "alert_fleet_arrived_at_destination"},
    FleetNotificationCatalogEntry{FleetNotificationKind::StartedMining, "StartedMining", "alert_fleet_started_mining"},
    FleetNotificationCatalogEntry{FleetNotificationKind::NodeDepleted, "NodeDepleted", "alert_fleet_node_depleted"},
    FleetNotificationCatalogEntry{FleetNotificationKind::Docked, "Docked", "alert_fleet_docked"},
    FleetNotificationCatalogEntry{FleetNotificationKind::RepairComplete, "RepairComplete",
                                  "alert_fleet_repair_complete"},
    FleetNotificationCatalogEntry{FleetNotificationKind::MinerOpc, "MinerOPC", "alert_fleet_miner_opc"},
};

static_assert(kFleetNotificationCatalog.size() == static_cast<std::size_t>(FleetNotificationKind::Count));

constexpr FleetNotificationMask fleet_notification_bit(FleetNotificationKind kind)
{ return FleetNotificationMask{1} << static_cast<uint8_t>(kind); }

constexpr std::string_view fleet_notification_name(FleetNotificationKind kind)
{
  for (const auto& entry : kFleetNotificationCatalog) {
    if (entry.kind == kind) {
      return entry.config_name;
    }
  }
  return "Unknown";
}

inline constexpr FleetNotificationMask kAllFleetNotifications = [] {
  FleetNotificationMask result = 0;
  for (const auto& entry : kFleetNotificationCatalog) {
    result |= fleet_notification_bit(entry.kind);
  }
  return result;
}();

inline FleetNotificationMask ParseFleetNotifications(std::string_view text)
{
  FleetNotificationMask result = 0;
  while (!text.empty()) {
    const auto comma = text.find(',');
    auto       token = text.substr(0, comma);
    const auto first = token.find_first_not_of(" \t\r\n");
    if (first != std::string_view::npos) {
      token            = token.substr(first, token.find_last_not_of(" \t\r\n") - first + 1);
      const auto equal = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size())
          return false;
        for (size_t i = 0; i < a.size(); ++i) {
          const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
          if (lower(a[i]) != lower(b[i]))
            return false;
        }
        return true;
      };
      if (equal(token, "all"))
        result |= kAllFleetNotifications;
      for (const auto& entry : kFleetNotificationCatalog)
        if (equal(token, entry.config_name))
          result |= fleet_notification_bit(entry.kind);
    }
    if (comma == std::string_view::npos)
      break;
    text.remove_prefix(comma + 1);
  }
  return result;
}

inline std::string FormatFleetNotifications(FleetNotificationMask mask)
{
  std::string result;
  for (const auto& entry : kFleetNotificationCatalog) {
    if (!(mask & fleet_notification_bit(entry.kind)))
      continue;
    if (!result.empty())
      result += ", ";
    result += entry.config_name;
  }
  return result;
}
