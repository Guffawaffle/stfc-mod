/**
 * @file sync_battle_logs.h
 * @brief Battle-log queueing, enrichment, resolver cache, and sent-ID persistence.
 */
#pragma once

#include "patches/battle_journal_dispatch_context.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

void load_previously_sent_logs();
void process_battle_headers(const nlohmann::json& section, const BattleJournalDispatchContext& dispatch);
void cache_player_names(std::unique_ptr<std::string>&& bytes);
void cache_alliance_names(std::unique_ptr<std::string>&& bytes);
void StartCombatLogWorker();
void ShutdownCombatLogWorker();
