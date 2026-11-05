#include "features/fleet_opc_sample.h"
#include "game/fleet_opc.h"

#include <chrono>

namespace
{
fleet_opc::SampleCache opc_sample_cache;
}

namespace fleet_opc
{
void Invalidate(int32_t slot)
{ opc_sample_cache.Invalidate(slot); }

Cargo Read(FleetPlayerData* fleet, int32_t slot, int64_t fleet_id, int32_t state)
{
  if (!fleet)
    return {};
  const auto now =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count();
  return opc_sample_cache.Read(slot, fleet_id, reinterpret_cast<uintptr_t>(fleet), state, now, [fleet] {
    Cargo      result;
    const auto observed = fleet_opc_binding::Cargo(fleet);
    auto       hold     = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
    const auto progress =
        hold ? hold.Get<fleet_opc_binding::Hold>()->Unprotected() : il2cpp::CallResult<fleet_opc_binding::Progress*>{};
    auto root = il2cpp::GcHandle::Strong(progress ? progress.value : nullptr);
    if (!root)
      return result;
    const auto current         = root.Get<fleet_opc_binding::Progress>()->Current();
    const auto protected_limit = root.Get<fleet_opc_binding::Progress>()->Protected();
    if (!current || !protected_limit || !std::isfinite(current.value) || !std::isfinite(protected_limit.value))
      return result;
    result.known           = true;
    result.current         = current.value;
    result.protected_limit = protected_limit.value;
    result.opc             = result.current > result.protected_limit;
    return result;
  });
}

Status Observe(FleetPlayerData* fleet, int32_t slot, int64_t fleet_id, int32_t state)
{
  const bool mining = state == static_cast<int32_t>(FleetState::Mining);
  if (!mining || !fleet)
    return {};
  const auto cargo    = Read(fleet, slot, fleet_id, state);
  const auto observed = fleet_opc_binding::MiningData(fleet);
  auto       root     = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  auto*      model    = root.Get<fleet_opc_binding::Mining>();
  const auto rate     = model ? model->Speed() : il2cpp::CallResult<float>{};
  const auto mined    = model ? model->Mined() : il2cpp::CallResult<double>{};
  const auto amount   = model ? model->Amount() : il2cpp::CallResult<int64_t>{};
  return Evaluate(true, cargo, static_cast<bool>(rate), rate.value, mined && amount, mined.value,
                  static_cast<double>(amount.value));
}
} // namespace fleet_opc
