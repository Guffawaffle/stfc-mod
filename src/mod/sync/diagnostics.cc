// Already-enabled observations use their own best-effort lane on each existing
// uploader. They never activate probes or enter the canonical entity store.

#include "sync/sync.h"
#include "sync_diagnostics.h"
#include "version.h"

#include <memory>

void PublishDiagnosticEvent(std::string_view stream, const nlohmann::json& event) noexcept
{
  using namespace data_sync;
  SharedState* observed_state = nullptr;
  try {
    if (!Config::Sync::Get()->options.diagnostics)
      return;
    auto& state    = State();
    observed_state = &state;
    if (!state.diagnostics_ready.load(std::memory_order_acquire))
      return;
    const auto               generation = state.generation.load();
    static const std::string source_id  = NewUuid();
    auto                     body       = DiagnosticEnvelope(stream, event, source_id, VER_FILE_VERSION_STR);
    if (!body) {
      state.diagnostic_drops.fetch_add(1);
      return;
    }
    const auto charge = sizeof(DiagnosticPayload) + body->capacity() + 1;
    if (!state.diagnostic_budget.Reserve(charge)) {
      if (state.diagnostic_drops.fetch_add(1) % 64 == 0)
        spdlog::warn("[Sync] diagnostic dispatch budget exhausted; dropping best-effort observations");
      return;
    }
    // Like the old dispatch lease, this ends after fan-out. Each destination
    // keeps its own charge until its observation's HTTP attempt has completed.
    DiagnosticCharge reservation{state.diagnostic_budget, charge};
    auto             payload  = std::make_shared<const DiagnosticPayload>(std::move(*body));
    bool             admitted = false;
    size_t           rejected = 0;
    {
      std::scoped_lock lock(state.mutex);
      if (generation != state.generation.load())
        return;
      for (const auto& [name, destination] : state.targets) {
        const auto& delivery = destination->delivery;
        if (destination->unavailable || !delivery.Config().diagnostics)
          continue;
        const auto revision = delivery.Revision();
        const auto result   = destination->diagnostics.TryEmplace(
            charge, [&] { return DiagnosticObservation{payload, generation, revision}; });
        if (result == sync_queue::Admission::accepted)
          admitted = true;
        else
          ++rejected;
      }
    }
    if (admitted)
      state.ready.notify_all();
    if (rejected && state.diagnostic_drops.fetch_add(rejected) % 64 == 0)
      spdlog::warn("[Sync] diagnostic destination budget exhausted; dropping best-effort observations");
  } catch (...) {
    if (observed_state)
      observed_state->diagnostic_drops.fetch_add(1);
    // No producer, allocation, formatting or delivery failure may unwind into
    // the caller. Diagnostics never close ordinary input or invent an ACK.
  }
}
