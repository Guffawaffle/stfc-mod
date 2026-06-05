/**
 * @file battle_ref_resolver.cc
 * @brief Candidate-only resolver probe for battle analytics catalog refs.
 */
#include "patches/battle_ref_resolver.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace battle_ref_resolver
{
namespace
{
  using battle_log_decoder::CatalogResolver;

  struct RefObservation {
    std::string              field;
    std::string              refKind;
    std::string              sourcePath;
    std::vector<std::string> domains;
  };

  struct RefProbe {
    std::string                 ref;
    std::vector<RefObservation> observations;
  };

  struct DomainResolver {
    std::string                            domain;
    std::string                            resolver_name;
    std::function<std::string(int64_t)>    name;
    std::function<nlohmann::json(int64_t)> metadata;
  };

  [[nodiscard]] std::optional<int64_t> parse_i64_exact(const std::string& text)
  {
    if (text.empty()) {
      return std::nullopt;
    }

    try {
      size_t     consumed = 0;
      const auto value    = std::stoll(text, &consumed, 10);
      if (consumed == text.size()) {
        return value;
      }
    } catch (...) {
    }

    return std::nullopt;
  }

  [[nodiscard]] bool is_i64_string(const std::string& text)
  { return parse_i64_exact(text).has_value(); }

  [[nodiscard]] std::optional<std::string> json_ref_string(const nlohmann::json& value)
  {
    if (value.is_string()) {
      const auto text = value.get<std::string>();
      if (is_i64_string(text)) {
        return text;
      }
      return std::nullopt;
    }
    if (value.is_number_integer()) {
      return std::to_string(value.get<int64_t>());
    }
    if (value.is_number_unsigned()) {
      const auto unsigned_value = value.get<uint64_t>();
      if (unsigned_value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return std::to_string(unsigned_value);
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] nlohmann::json observation_json(const RefObservation& observation)
  {
    return nlohmann::json{{"field", observation.field},
                          {"refKind", observation.refKind},
                          {"sourceFile", "battle.analytics"},
                          {"sourcePath", observation.sourcePath},
                          {"domainHints", observation.domains}};
  }

  void add_observation(std::map<std::string, RefProbe>& refs, const nlohmann::json& value, std::string field,
                       std::string ref_kind, std::string source_path, std::vector<std::string> domains)
  {
    const auto ref = json_ref_string(value);
    if (!ref) {
      return;
    }

    auto& probe = refs[*ref];
    probe.ref   = *ref;
    probe.observations.push_back(
        RefObservation{std::move(field), std::move(ref_kind), std::move(source_path), std::move(domains)});
  }

  void add_array_observations(std::map<std::string, RefProbe>& refs, const nlohmann::json& object,
                              std::string_view field, std::string ref_kind, std::string source_path,
                              std::vector<std::string> domains)
  {
    if (!object.is_object() || !object.contains(std::string(field)) || !object[std::string(field)].is_array()) {
      return;
    }

    const auto& values = object[std::string(field)];
    for (size_t index = 0; index < values.size(); ++index) {
      add_observation(refs, values[index], std::string(field), ref_kind,
                      source_path + "." + std::string(field) + "[" + std::to_string(index) + "]", domains);
    }
  }

  void collect_ship_refs(std::map<std::string, RefProbe>& refs, const nlohmann::json& ship, const std::string& path)
  {
    add_array_observations(refs, ship, "hullIdsExact", "hull", path, {"hull"});
    add_array_observations(refs, ship, "componentIdsExact", "ship_component", path, {"ship_component"});
    add_array_observations(refs, ship, "hull_ids_exact", "hull", path, {"hull"});
    add_array_observations(refs, ship, "component_ids_exact", "ship_component", path, {"ship_component"});
  }

  void collect_runtime_candidate_refs(std::map<std::string, RefProbe>& refs, const nlohmann::json& candidates,
                                      const std::string& path_prefix)
  {
    if (!candidates.is_array()) {
      return;
    }

    for (size_t index = 0; index < candidates.size(); ++index) {
      if (!candidates[index].is_object()) {
        continue;
      }
      const auto path = path_prefix + "[" + std::to_string(index) + "]";
      add_observation(refs, candidates[index].contains("sourceRef") ? candidates[index]["sourceRef"] : nlohmann::json(),
                      "sourceRef", "runtime_candidate_source", path + ".sourceRef",
                      {"officer", "ability", "forbidden_tech", "buff", "ship_component", "hull", "resource"});
      add_observation(refs, candidates[index].contains("effectRef") ? candidates[index]["effectRef"] : nlohmann::json(),
                      "effectRef", "runtime_candidate_effect", path + ".effectRef",
                      {"ability", "buff", "forbidden_tech", "officer", "ship_component", "hull", "resource"});
    }
  }

  [[nodiscard]] std::map<std::string, RefProbe> collect_refs(const nlohmann::json& decoded)
  {
    auto refs = std::map<std::string, RefProbe>{};

    if (!decoded.is_object()) {
      return refs;
    }

    if (decoded.contains("runtime_ability_row_candidates")) {
      collect_runtime_candidate_refs(refs, decoded["runtime_ability_row_candidates"],
                                     "decoded.runtime_ability_row_candidates");
    }

    if (decoded.contains("participants") && decoded["participants"].is_array()) {
      const auto& participants = decoded["participants"];
      for (size_t index = 0; index < participants.size(); ++index) {
        collect_ship_refs(refs, participants[index], "decoded.participants[" + std::to_string(index) + "]");
      }
    }

    if (!decoded.contains("attack_rows") || !decoded["attack_rows"].is_array()) {
      return refs;
    }

    const auto& attacks = decoded["attack_rows"];
    for (size_t attack_index = 0; attack_index < attacks.size(); ++attack_index) {
      if (!attacks[attack_index].is_object()) {
        continue;
      }

      const auto  attack_path = "decoded.attack_rows[" + std::to_string(attack_index) + "]";
      const auto& attack      = attacks[attack_index];
      add_observation(refs, attack.contains("componentIdExact") ? attack["componentIdExact"] : nlohmann::json(),
                      "componentIdExact", "ship_component", attack_path + ".componentIdExact", {"ship_component"});

      if (attack.contains("attacker")) {
        collect_ship_refs(refs, attack["attacker"], attack_path + ".attacker");
      }
      if (attack.contains("target")) {
        collect_ship_refs(refs, attack["target"], attack_path + ".target");
      }

      if (attack.contains("runtimeAbilityRowCandidates")) {
        collect_runtime_candidate_refs(refs, attack["runtimeAbilityRowCandidates"],
                                       attack_path + ".runtimeAbilityRowCandidates");
      }

      if (attack.contains("triggeredEffects") && attack["triggeredEffects"].is_array()) {
        const auto& effects = attack["triggeredEffects"];
        for (size_t effect_index = 0; effect_index < effects.size(); ++effect_index) {
          if (!effects[effect_index].is_object()) {
            continue;
          }
          const auto effect_path = attack_path + ".triggeredEffects[" + std::to_string(effect_index) + "]";
          add_observation(
              refs, effects[effect_index].contains("refAExact") ? effects[effect_index]["refAExact"] : nlohmann::json(),
              "refAExact", "triggered_effect_ref", effect_path + ".refAExact",
              {"officer", "ability", "forbidden_tech", "buff", "ship_component", "hull", "resource"});
          add_observation(
              refs, effects[effect_index].contains("refBExact") ? effects[effect_index]["refBExact"] : nlohmann::json(),
              "refBExact", "triggered_effect_ref", effect_path + ".refBExact",
              {"officer", "ability", "forbidden_tech", "buff", "ship_component", "hull", "resource"});
        }
      }
    }

    return refs;
  }

  [[nodiscard]] std::string resolve_name(const std::function<std::string(int64_t)>& resolver, int64_t id)
  {
    if (!resolver) {
      return {};
    }
    try {
      return resolver(id);
    } catch (...) {
      return {};
    }
  }

  [[nodiscard]] nlohmann::json resolve_metadata(const std::function<nlohmann::json(int64_t)>& resolver, int64_t id)
  {
    if (!resolver) {
      return nlohmann::json::object();
    }
    try {
      auto metadata = resolver(id);
      return metadata.is_object() ? metadata : nlohmann::json::object();
    } catch (...) {
      return nlohmann::json::object();
    }
  }

  [[nodiscard]] std::vector<DomainResolver> build_domain_resolvers(const CatalogResolver& resolver)
  {
    return {{"officer", "officer", resolver.officer_name, resolver.officer_metadata},
            {"ability", "ability", resolver.ability_name, resolver.ability_metadata},
            {"forbidden_tech", "forbidden_tech", resolver.forbidden_tech_name, resolver.forbidden_tech_metadata},
            {"buff", "buff", resolver.buff_name, resolver.buff_metadata},
            {"buff", "debuff", resolver.debuff_name, resolver.debuff_metadata},
            {"ship_component", "component", resolver.component_name, resolver.component_metadata},
            {"hull", "hull", resolver.hull_name, resolver.hull_metadata},
            {"resource", "resource", resolver.resource_name, resolver.resource_metadata}};
  }

  [[nodiscard]] bool domain_requested(const RefProbe& probe, const std::string& domain)
  {
    for (const auto& observation : probe.observations) {
      if (std::ranges::find(observation.domains, domain) != observation.domains.end()) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] nlohmann::json resolve_candidates(const RefProbe& probe, const CatalogResolver& resolver,
                                                  std::set<std::string>& domains_searched)
  {
    auto       candidates = nlohmann::json::array();
    const auto parsed_id  = parse_i64_exact(probe.ref);
    if (!parsed_id) {
      return candidates;
    }

    for (const auto& domain_resolver : build_domain_resolvers(resolver)) {
      if (!domain_requested(probe, domain_resolver.domain)) {
        continue;
      }

      domains_searched.insert(domain_resolver.domain);
      const auto name     = resolve_name(domain_resolver.name, *parsed_id);
      const auto metadata = resolve_metadata(domain_resolver.metadata, *parsed_id);
      if (name.empty() && metadata.empty()) {
        continue;
      }

      auto candidate =
          nlohmann::json{{"domain", domain_resolver.domain},
                         {"catalog", "sidecar"},
                         {"matchType", "id_exact"},
                         {"confidence", name.empty() ? "medium" : "high"},
                         {"provenance",
                          {{"sourceFile", "CatalogResolver"},
                           {"sourcePath", "battle_log_decoder::CatalogResolver." + domain_resolver.resolver_name}}}};
      if (!name.empty()) {
        candidate["name"] = name;
      }
      if (!metadata.empty()) {
        candidate["metadata"] = metadata;
      }
      candidates.push_back(std::move(candidate));
    }

    return candidates;
  }

  [[nodiscard]] std::unordered_map<std::string, nlohmann::json> build_ref_index(const nlohmann::json& resolver_probe)
  {
    auto result = std::unordered_map<std::string, nlohmann::json>{};
    if (!resolver_probe.is_object() || !resolver_probe.contains("refs") || !resolver_probe["refs"].is_array()) {
      return result;
    }

    for (const auto& ref_entry : resolver_probe["refs"]) {
      if (!ref_entry.is_object() || !ref_entry.contains("ref") || !ref_entry["ref"].is_string()) {
        continue;
      }
      result.emplace(ref_entry["ref"].get<std::string>(), ref_entry);
    }
    return result;
  }

  void append_candidates_for_ref(nlohmann::json&                                        candidates,
                                 const std::unordered_map<std::string, nlohmann::json>& ref_index,
                                 const nlohmann::json&                                  ref_value)
  {
    const auto ref = json_ref_string(ref_value);
    if (!ref) {
      return;
    }

    const auto found = ref_index.find(*ref);
    if (found == ref_index.end() || !found->second.contains("resolverCandidates")
        || !found->second["resolverCandidates"].is_array()) {
      return;
    }

    for (const auto& candidate : found->second["resolverCandidates"]) {
      candidates.push_back(candidate);
    }
  }
} // namespace

nlohmann::json BuildResolverProbe(const nlohmann::json& decoded, const CatalogResolver& resolver)
{
  const auto collected                   = collect_refs(decoded);
  auto       refs                        = nlohmann::json::array();
  auto       domains_searched            = std::set<std::string>{};
  auto       unresolved_refs             = nlohmann::json::array();
  size_t     refs_with_candidate_matches = 0;
  size_t     candidate_match_count       = 0;

  for (const auto& [ref, probe] : collected) {
    auto observed_in = nlohmann::json::array();
    for (const auto& observation : probe.observations) {
      observed_in.push_back(observation_json(observation));
    }

    auto candidates = resolver.allow_ref_probe_callbacks ? resolve_candidates(probe, resolver, domains_searched)
                              : nlohmann::json::array();
    if (candidates.empty()) {
      unresolved_refs.push_back(ref);
    } else {
      ++refs_with_candidate_matches;
      candidate_match_count += candidates.size();
    }

    refs.push_back(nlohmann::json{{"ref", ref},
                                  {"resolutionStatus", candidates.empty() ? "unresolved" : "candidate_matches"},
                                  {"observedIn", observed_in},
                                  {"resolverCandidates", std::move(candidates)}});
  }

  return nlohmann::json{
      {"schema", "stfc.battle.ref_resolver_probe.v0"},
      {"status", resolver.allow_ref_probe_callbacks ? "candidate_probe" : "scan_only"},
      {"coverage",
       {{"refsScanned", collected.size()},
        {"refsWithCandidateMatches", refs_with_candidate_matches},
        {"candidateMatchCount", candidate_match_count},
        {"domainsSearched", std::vector<std::string>(domains_searched.begin(), domains_searched.end())},
        {"unresolvedRefs", unresolved_refs},
        {"promotedToCsvRows", false}}},
      {"refs", refs},
      {"nonClaims",
       nlohmann::json::array(
           {"Resolver candidates are exact catalog probes, not finalized ability activations.",
            "A matching sourceRef/effectRef candidate does not prove an ability proc, owner, stack, rate, refresh, or "
            "expiry.",
            "Runtime candidates remain experimental and are not promoted into CSV parity ability rows."})}};
}

void AttachResolverCandidates(nlohmann::json& analytics, const nlohmann::json& resolver_probe)
{
  if (!analytics.is_object()) {
    return;
  }

  analytics["experimental"]["resolver"] = resolver_probe;

  if (!analytics["experimental"].contains("runtimeAbilityRowCandidates")
      || !analytics["experimental"]["runtimeAbilityRowCandidates"].is_array()) {
    return;
  }

  const auto ref_index  = build_ref_index(resolver_probe);
  auto&      candidates = analytics["experimental"]["runtimeAbilityRowCandidates"];
  for (auto& candidate : candidates) {
    if (!candidate.is_object()) {
      continue;
    }

    auto resolver_candidates = nlohmann::json::array();
    append_candidates_for_ref(resolver_candidates, ref_index,
                              candidate.contains("sourceRef") ? candidate["sourceRef"] : nlohmann::json());
    append_candidates_for_ref(resolver_candidates, ref_index,
                              candidate.contains("effectRef") ? candidate["effectRef"] : nlohmann::json());

    if (resolver_candidates.empty()) {
      candidate["resolverStatus"] = "unresolved";
      continue;
    }

    candidate["resolverStatus"]     = "candidate_matches";
    candidate["resolutionStatus"]   = "candidate_matches";
    candidate["resolverCandidates"] = std::move(resolver_candidates);
  }
}

nlohmann::json BuildValueStatementBridge(const nlohmann::json& resolver_probe)
{
  const auto& coverage =
      resolver_probe.is_object() && resolver_probe.contains("coverage") && resolver_probe["coverage"].is_object()
          ? resolver_probe["coverage"]
          : nlohmann::json::object();

  return nlohmann::json{
      {"schema", "stfc.battle.resolver_bridge.v0"},
      {"status", resolver_probe.value("status", std::string{"scan_only"})},
      {"refsScanned", coverage.value("refsScanned", size_t{0})},
      {"refsWithCandidateMatches", coverage.value("refsWithCandidateMatches", size_t{0})},
      {"candidateMatchCount", coverage.value("candidateMatchCount", size_t{0})},
      {"domainsSearched", coverage.contains("domainsSearched") ? coverage["domainsSearched"] : nlohmann::json::array()},
      {"unresolvedRefs", coverage.contains("unresolvedRefs") ? coverage["unresolvedRefs"] : nlohmann::json::array()},
      {"promotedToCsvRows", false},
      {"nonClaims",
       nlohmann::json::array({"Resolver bridge output contains candidates only, not confirmed ability activations.",
                              "Catalog names are emitted only when CatalogResolver returns an exact-id match.",
                              "Resolver candidates are not promoted into CSV parity ability rows."})}};
}
} // namespace battle_ref_resolver