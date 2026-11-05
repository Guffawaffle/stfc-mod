#include "sync_diagnostics.h"
#include "sync_fixture_state.h"

#include <iostream>
#include <limits>

using namespace data_sync;
using sync_fixture::Check;
using sync_fixture::state;

int main()
{
  try {
    sync_fixture::Fresh();
    auto             first  = std::make_shared<Destination>();
    auto             second = std::make_shared<Destination>();
    SyncTargetConfig options;
    options.resources = true;
    first->delivery.Configure(options);
    second->delivery.Configure(options);
    state.targets.emplace("synthetic-a", first);
    state.targets.emplace("synthetic-b", second);
    const json event{{"seq", std::numeric_limits<uint64_t>::max()}, {"session", "synthetic"}, {"phase", "sample"}};
    PublishDiagnosticEvent("fixture", event);
    Check(first->diagnostics.Stats().count == 0 && second->diagnostics.Stats().count == 0,
          "diagnostics are disabled by default for each destination");

    options.diagnostics = true;
    first->delivery.Configure(options);
    state.diagnostics_ready = false;
    PublishDiagnosticEvent("fixture", event);
    Check(first->diagnostics.Stats().count == 0, "unready transport retains no observations");
    state.diagnostics_ready = true;
    PublishDiagnosticEvent("fixture", event);
    Check(first->diagnostics.Stats().count == 1 && second->diagnostics.Stats().count == 0,
          "only explicitly enabled destinations receive observations");
    Check(state.diagnostic_budget.Count() == 0 && state.diagnostic_budget.Bytes() == 0,
          "dispatch charge ends after fan-out while destination charge remains");
    auto observation = first->diagnostics.TryPop();
    Check(observation.has_value(), "actual publisher queued an observation");
    Check(first->diagnostics.Stats().count == 1 && first->diagnostics.Stats().bytes > 0,
          "in-flight destination lease keeps its own charge");
    const auto body = json::parse(observation->value.payload->body).at(0);
    Check(body.at("sequence") == "18446744073709551615" && body.at("payload") == event,
          "actual publisher preserves exact sequence and source payload");
    const auto generation = state.generation.load();
    Check(DiagnosticCurrent(observation->value, generation, *first, state.diagnostics_ready.load()),
          "current observation is eligible");
    state.diagnostics_ready = false;
    Check(!DiagnosticCurrent(observation->value, generation, *first, state.diagnostics_ready.load()),
          "unavailable native context invalidates even a queued observation");
    state.diagnostics_ready = true;
    SealOrdinarySession(generation);
    Check(!OrdinarySessionOpen(generation)
              && DiagnosticCurrent(observation->value, generation, *first, state.diagnostics_ready.load()),
          "ordinary evidence loss does not disable independently requested diagnostics");
    options.token = "synthetic-new-token";
    first->delivery.Configure(options);
    Check(!DiagnosticCurrent(observation->value, generation, *first, state.diagnostics_ready.load()),
          "configuration revision rejects stale observation");
    observation.reset();
    Check(first->diagnostics.Stats().count == 0 && first->diagnostics.Stats().bytes == 0,
          "destination charge ends after payload ownership is released");

    PublishDiagnosticEvent("fixture", event);
    {
      auto stale = first->diagnostics.TryPop();
      Check(stale.has_value(), "new revision queues new observations");
      first->delivery.Retire();
      Check(!DiagnosticCurrent(stale->value, generation, *first, state.diagnostics_ready.load()),
            "retirement invalidates queued observations");
    }
    first->delivery.Configure(options);
    PublishDiagnosticEvent("fixture", event);
    {
      auto stale = first->diagnostics.TryPop();
      Check(stale.has_value(), "observation exists before replacement session");
      state.records.Reset();
      state.generation = state.records.Generation();
      Check(!DiagnosticCurrent(stale->value, state.generation.load(), *first, state.diagnostics_ready.load()),
            "new session rejects older observations");
    }
    Check(OrdinarySessionOpen(state.generation.load()), "complete fresh generation recovers ordinary admission");

    auto disabled =
        std::make_shared<Config::Sync>(*std::atomic_load_explicit(&sync_fixture::settings, std::memory_order_acquire));
    disabled->options.diagnostics = false;
    std::atomic_store_explicit(&sync_fixture::settings, std::shared_ptr<const Config::Sync>{disabled},
                               std::memory_order_release);
    PublishDiagnosticEvent("fixture", event);
    Check(first->diagnostics.Stats().count == 0, "global opt-out prevents even an enabled destination retaining data");
    auto enabled                 = std::make_shared<Config::Sync>(*disabled);
    enabled->options.diagnostics = true;
    std::atomic_store_explicit(&sync_fixture::settings, std::shared_ptr<const Config::Sync>{enabled},
                               std::memory_order_release);
    first->unavailable = true;
    PublishDiagnosticEvent("fixture", event);
    Check(first->diagnostics.Stats().count == 0, "failed destination remains a tombstone");
    first->unavailable = false;

    for (uint64_t i = 0; i < kDiagnosticCount; ++i)
      PublishDiagnosticEvent("fixture", json{{"seq", i}, {"value", "synthetic"}});
    const auto full = first->diagnostics.Stats();
    Check(full.count == kDiagnosticCount && full.bytes <= kDiagnosticBytes, "destination count and byte bounds hold");
    PublishDiagnosticEvent("fixture", event);
    Check(first->diagnostics.Stats().count == full.count && first->diagnostics.Stats().bytes == full.bytes,
          "overflow rejects new observations without evicting the retained prefix");
    {
      auto prefix = first->diagnostics.TryPop();
      Check(prefix && json::parse(prefix->value.payload->body).at(0).at("sequence") == "0",
            "overflow retains oldest event");
    }
    while (first->diagnostics.TryPop()) {}
    Check(first->diagnostics.Stats().count == 0 && first->diagnostics.Stats().bytes == 0, "queue drains all charges");

    for (uint64_t i = 0; i < 32; ++i)
      PublishDiagnosticEvent("fixture", json{{"seq", i}, {"value", std::string(200 * 1024, 'x')}});
    const auto bytes_full = first->diagnostics.Stats();
    Check(bytes_full.count > 0 && bytes_full.count < 32 && bytes_full.bytes <= kDiagnosticBytes,
          "byte limit rejects large observations before count capacity");
    while (first->diagnostics.TryPop()) {}
    const auto before_invalid = state.diagnostic_drops.load();
    PublishDiagnosticEvent("fixture", json{{"seq", uint64_t{1}}, {"value", std::string(1024 * 1024, 'x')}});
    Check(first->diagnostics.Stats().count == 0 && state.diagnostic_drops.load() > before_invalid,
          "oversized serialization is counted and never reaches destination ownership");
    Check(state.diagnostic_budget.Reserve(kDiagnosticBytes), "synthetic concurrent dispatch occupies budget");
    {
      DiagnosticCharge held{state.diagnostic_budget, kDiagnosticBytes};
      PublishDiagnosticEvent("fixture", event);
      Check(first->diagnostics.Stats().count == 0, "dispatch cap rejects rather than evicting destination data");
    }
    Check(state.diagnostic_budget.Count() == 0 && state.diagnostic_budget.Bytes() == 0,
          "all dispatch reservations release");
    state.targets.clear();
    std::cout << "Diagnostic publisher, opt-in, epochs and ownership tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
