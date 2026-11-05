#pragma once

#include "ui.h"
#include <optional>
#include <string>

struct ToastFleetObserver : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.HUD", "ToastFleetObserver">;
};

// Owned scalars only. Missing or malformed payloads leave the native/generic notification path intact.
struct IncomingPlayerAttack {
  int64_t     fleet_id{};
  int32_t     slot{-1};
  std::string attacker, event_id;
};
std::optional<IncomingPlayerAttack> ReadIncomingPlayerAttack(Toast* toast);
// Binding readiness only; no payload read, account preference access or notification delivery.
bool IncomingPlayerAttackAvailable();
