#pragma once

// Exercise actual process-lifetime UI caches across normal C++ process exit.
// The managed runtime is controlled; this does not reproduce a native Mac crash.
#include "../src/mod/features/forbidden_tech_ship_name.cc"

namespace
{
void ManagedOwnerExitCases(std::string_view mode)
{
  const bool store = mode == "owner-exit-store";
  const bool work  = mode == "owner-exit-store-work";
  Require(store || work || mode == "owner-exit-ft", "known managed-owner exit mode");
  Require(warp_roots.empty() && alliance_store_tooltips.empty() && ship_name_rows.empty() && !alliance_store_work.source,
          "fresh process owners");
  const auto populate = [store, work] {
    auto* object = &base_object.header;
    if (store) {
      auto entry      = std::make_shared<OwnedStoreTooltip>();
      entry->widget   = il2cpp::GcHandle::Weak(object);
      entry->plain    = il2cpp::GcHandle::Weak(object);
      entry->action   = il2cpp::GcHandle::Weak(object);
      entry->store    = il2cpp::GcHandle::Weak(object);
      entry->search   = il2cpp::GcHandle::Strong(object);
      entry->locale   = il2cpp::GcHandle::Strong(object);
      entry->settings = il2cpp::GcHandle::Strong(object);
      alliance_store_tooltips.push_back(std::move(entry));
    } else if (work) {
      alliance_store_work.source = il2cpp::GcHandle::Weak(object);
    } else {
      ShipNameRow row;
      row.scroller = il2cpp::GcHandle::Weak(object);
      row.label    = il2cpp::GcHandle::Weak(object);
      ship_name_rows.push_back(std::move(row));
    }
  };
  populate();
  const auto count = store ? 7u : work ? 1u : 2u;
  Require(warp_roots.size() == count, "actual cache owns every managed handle");
  alliance_store_tooltips.clear();
  ship_name_rows.clear();
  alliance_store_work = {};
  Require(warp_roots.empty(), "explicit live cache retirement still releases its managed handles");
  populate();
  Require(warp_roots.size() == count, "cache retains live handles until normal exit");
  Require(std::atexit(+[] { gc_runtime_available = false; }) == 0, "register managed-runtime teardown boundary");
  std::cout << "Managed-owner normal-exit case armed: " << mode << std::endl;
}
} // namespace
