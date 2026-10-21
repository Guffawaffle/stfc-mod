#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace game_strings
{
enum class NameKind { Hull, Officer, ForbiddenTech, OfficerAbility, ShipAbility, TechnologyEffect };
enum class NameStatus {
  Resolved,
  InvalidRequest,
  SpecLookupFailed,
  SpecMissing,
  LocalizationIdMissing,
  TranslationMissing
};

struct NameLookup {
  std::string  text;
  std::int64_t loca_id = 0;
  NameStatus   status  = NameStatus::InvalidRequest;
};

inline const char* StatusName(NameStatus status)
{
  switch (status) {
    case NameStatus::Resolved:
      return "resolved";
    case NameStatus::InvalidRequest:
      return "invalid-request";
    case NameStatus::SpecLookupFailed:
      return "spec-lookup-failed";
    case NameStatus::SpecMissing:
      return "spec-missing";
    case NameStatus::LocalizationIdMissing:
      return "localization-id-missing";
    case NameStatus::TranslationMissing:
      return "translation-missing";
  }
  return "unknown-status";
}

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
    case NameKind::TechnologyEffect:
      return {"GetBuffSpec", 2, "forbidden_tech_short_desc_{0}", "forbidden_tech"};
  }
  return {};
}

// Some native OfficerEvents contain hull abilities. Only replace an unavailable
// officer title when the owner's hull and its ability agree on localization ID.
inline bool CanUseShipAbility(const NameLookup& officer, const NameLookup& hull, const NameLookup& ship)
{
  return officer.status == NameStatus::TranslationMissing && officer.loca_id > 0 && hull.status == NameStatus::Resolved
         && ship.status == NameStatus::Resolved && officer.loca_id == hull.loca_id && hull.loca_id == ship.loca_id;
}

// Strip recognized native formatting from localized descriptions. Unknown tags
// and comparisons remain content for the caller's normal UI escaping. Never
// apply this to player-authored names.
inline std::string PlainDescription(std::string_view text)
{
  std::string result;
  for (std::size_t i = 0; i < text.size();) {
    if (text[i] == '<') {
      const auto end = text.find('>', i + 1);
      if (end != text.npos) {
        const auto tag = text.substr(i + 1, end - i - 1);
        if (tag == "/color" || (tag.starts_with("color=") && tag.size() > 6 && tag.find('<') == tag.npos) || tag == "b"
            || tag == "/b" || tag == "i" || tag == "/i") {
          i = end + 1;
          continue;
        }
      }
    }
    result += text[i++];
  }
  return result;
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
