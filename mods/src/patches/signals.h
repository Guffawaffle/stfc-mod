/**
 * @file signals.h
 * @brief Lightweight runtime signal contracts shared between hooks and feature observers.
 *
 * These structs are not an event-bus framework. They name the data crossing a
 * hook boundary so feature modules can accept a stable contract instead of a
 * loose argument list. Raw IL2CPP pointers are borrowed from the current hook
 * call and must not be stored beyond the receiving function.
 */
#pragma once

#include <cstdint>
#include <string_view>

struct FleetPlayerData;
struct ScreenManager;
struct Toast;

struct ToastEnqueuedSignal {
  Toast* raw_toast = nullptr;
  int state = -1;
  const char* title = nullptr;
  const char* source = nullptr;
};

struct ToastFleetQueueNotificationsSignal {
  const char* source = nullptr;
  uint64_t target_fleet_id = 0;
  int target_type = -1;
  int attacker_fleet_type = 0;
  std::string_view attacker_identity;
};

struct FleetStateObservedSignal {
  FleetPlayerData* raw_fleet = nullptr;
};

struct HotkeyFrameTickSignal {
  ScreenManager* raw_screen_manager = nullptr;
};

struct BattleHeaderObservedSignal {
  void* raw_header = nullptr;
  const char* source = nullptr;
};
