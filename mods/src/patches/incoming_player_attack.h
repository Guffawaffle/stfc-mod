#pragma once
#include <optional>
#include <string>

struct Toast;
struct IncomingPlayerAttack {
  std::string event_id;
  std::string attacker;
  long long   fleet_id = 0;
  int         slot     = -1;
};
bool                                IncomingPlayerAttackAvailable();
std::optional<IncomingPlayerAttack> ParseIncomingPlayerAttack(Toast* toast);
bool                                FirstIncomingPlayerAttack(const IncomingPlayerAttack& attack);
