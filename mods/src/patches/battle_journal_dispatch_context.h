/**
 * @file battle_journal_dispatch_context.h
 * @brief Provenance metadata for battle journal evidence and derived sidecar events.
 */
#pragma once

#include "patches/gameplay_dispatch_context.h"

#include <string>
#include <string_view>

struct BattleJournalDispatchContext {
  GameplayDispatchContext dispatch;
  std::string             evidence_kind;
  std::string             classification;
  std::string             validation;
};

inline BattleJournalDispatchContext battle_journal_dispatch_context(std::string_view source,
                                                                    std::string_view owner,
                                                                    std::string_view seam,
                                                                    std::string_view reason,
                                                                    std::string_view effect,
                                                                    std::string_view evidence_kind,
                                                                    std::string_view classification,
                                                                    std::string_view validation)
{
  return BattleJournalDispatchContext{
      gameplay_dispatch_context(source, owner, seam, reason, effect),
      std::string(evidence_kind),
      std::string(classification),
      std::string(validation),
  };
}

inline BattleJournalDispatchContext battle_journal_runtime_dispatch_context()
{
  return battle_journal_dispatch_context("battle-result-headers",
                                         "SyncEntityGroupHooks",
                                         "entity-group-json.battle_result_headers",
                                         "battle-result-headers-observed",
                                         "enqueue-battle-journal-fetch",
                                         "scopely.journal.battle",
                                         "runtime-evidence",
                                         "battle journal fetch is performed by the combat log worker");
}

inline BattleJournalDispatchContext battle_journal_probe_dispatch_context(std::string_view reason)
{
  return battle_journal_dispatch_context("battle-log-decoder-probe",
                                         "BattleLogDecoder",
                                         "offline-probe-or-test-fixture",
                                         reason,
                                         "decode-or-build-sidecar-battle-event",
                                         "probe.fixture.battle_journal",
                                         "quarantined-probe-only",
                                         "allowed for AX/tools/tests; runtime callers must pass explicit provenance");
}
