/**
 * @file battle_ref_resolver.h
 * @brief Candidate-only resolver probe for battle analytics catalog refs.
 */
#pragma once

#include "patches/battle_log_decoder.h"

#include <nlohmann/json.hpp>

namespace battle_ref_resolver
{
[[nodiscard]] nlohmann::json BuildResolverProbe(const nlohmann::json&                      decoded,
                                                const battle_log_decoder::CatalogResolver& resolver);

void AttachResolverCandidates(nlohmann::json& analytics, const nlohmann::json& resolver_probe);

[[nodiscard]] nlohmann::json BuildValueStatementBridge(const nlohmann::json& resolver_probe);
} // namespace battle_ref_resolver