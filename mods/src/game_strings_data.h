#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace game_strings
{
enum class NameKind { Hull, Officer, ForbiddenTech, OfficerAbility, ShipAbility };

struct NameKey {
  const char* method;
  int         arguments;
  const char* identifier;
  const char* category;
};

// Spec IDs and localization IDs are distinct. These are the client's own
// title contexts, not display-name guesses derived from internal spec names.
inline NameKey Key(NameKind kind)
{
  switch (kind) {
    case NameKind::Hull:
      return {"GetHull", 1, "ship_name_{0}", "ships"};
    case NameKind::Officer:
      return {"GetOfficerSpec", 1, "officer_name_{0}", "officer_names"};
    case NameKind::ForbiddenTech:
      return {"GetForbiddenTechSpecById", 1, "forbidden_tech_name_{0}", "forbidden_tech"};
    case NameKind::OfficerAbility:
      return {"GetBuffSpec", 2, "officer_ability_name_{0}", "officer_buffs"};
    case NameKind::ShipAbility:
      return {"GetBuffSpec", 2, "ship_ability_name_{0}", "ship_buffs"};
  }
  return {};
}

inline bool IsResolvedName(std::string_view text, std::string_view identifier, std::int64_t id)
{
  if (text.empty() || text.size() > 4096 || identifier.empty() || text.find_first_not_of(" \t\r\n") == text.npos
      || text.find_first_of("{}") != text.npos)
    return false;
  std::string expanded(identifier);
  const auto  token = expanded.find("{0}");
  if (token != expanded.npos)
    expanded.replace(token, 3, std::to_string(id));
  // Missing-key responses may wrap the identifier in a diagnostic message.
  return text.find(identifier) == text.npos && text.find(expanded) == text.npos;
}
} // namespace game_strings
