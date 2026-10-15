#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <cstdint>
#include <vector>

namespace
{

void ReverseRecords(void* records)
{
  if (!records)
    return;

  auto* listClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(records));
  if (!listClass)
    return;

  auto* getCount = il2cpp_class_get_method_from_name(listClass, "get_Count", 0);
  auto* getItem  = il2cpp_class_get_method_from_name(listClass, "get_Item", 1);
  auto* setItem  = il2cpp_class_get_method_from_name(listClass, "set_Item", 2);
  if (!getCount || !getItem || !setItem) {
    spdlog::warn("[HavenHistory] Record list is missing expected IList<T> methods");
    return;
  }

  Il2CppException* exception = nullptr;
  auto*            countObj  = il2cpp_runtime_invoke(getCount, records, nullptr, &exception);
  if (exception || !countObj)
    return;

  const auto* countType = il2cpp_class_get_type(countObj->klass);
  auto* countData = static_cast<int32_t*>(il2cpp_object_unbox(countObj));
  if (!countType || countType->byref || countType->type != IL2CPP_TYPE_I4 || !countData)
    return;
  const auto count = *countData;
  if (count <= 1)
    return;
  if (count > 2000) {
    spdlog::warn("[HavenHistory] Record list reported an implausible count ({}); skipping", count);
    return;
  }

  struct RootedItems {
    std::vector<Il2CppGCHandle> handles;
    ~RootedItems() { for (auto handle : handles) if (handle) il2cpp_gchandle_free(handle); }
  } items;
  items.handles.reserve(count);
  for (int32_t index = 0; index < count; ++index) {
    void* getArgs[1] = {&index};
    exception        = nullptr;
    auto* item       = il2cpp_runtime_invoke(getItem, records, getArgs, &exception);
    if (exception)
      return;
    // A setter can release the original list's last reference to a record.
    // Keep every record rooted until reversal has finished.
    auto handle = item ? il2cpp_gchandle_new(item, false) : nullptr;
    if (item && !handle)
      return;
    items.handles.push_back(handle);
  }

  for (int32_t index = 0; index < count; ++index) {
    auto handle = items.handles[count - index - 1];
    void* setArgs[2] = {&index, handle ? il2cpp_gchandle_get_target(handle) : nullptr};
    exception        = nullptr;
    il2cpp_runtime_invoke(setItem, records, setArgs, &exception);
    if (exception)
      return;
  }
}

void PlanetaryHelpHistoryContext_ApplyRecords_Hook(auto original, void* _this, void* records)
{
  if (Config::Get().reverse_haven_history)
    ReverseRecords(records);
  original(_this, records);
}

} // namespace

void InstallHavenHistoryHooks()
{
  auto context = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlanetaryBase", "PlanetaryHelpHistoryContext");
  auto applyRecords = context.GetMethod("ApplyRecords", 1);
  if (!applyRecords) {
    ErrorMsg::MissingMethod("PlanetaryHelpHistoryContext", "ApplyRecords");
    return;
  }

  SPUD_STATIC_DETOUR(applyRecords, PlanetaryHelpHistoryContext_ApplyRecords_Hook);
  spdlog::info("Haven history: hooks installed");
}
