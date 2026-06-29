#include "patches/section_change_router.h"

#include "patches/hook_registry.h"

#include <exception>
#include <vector>

#include <spdlog/spdlog.h>

namespace
{
constexpr HookDescriptor kSectionManagerTriggerSectionChangeHook{
    "SectionManager.TriggerSectionChange",
    "Fan out section-change evidence to seam-owned observers.",
    {"Assembly-CSharp", "Digit.Client.Sections", "SectionManager", "TriggerSectionChange"},
    "Section-change observers will not receive runtime transition evidence."};

std::vector<SectionChangeObserver>& observers()
{
  static std::vector<SectionChangeObserver> value;
  return value;
}

void notify_observer(const SectionChangeObserver& observer,
                     const SectionChangeContext&  context,
                     void (*callback)(const SectionChangeContext&))
{
  if (callback == nullptr) {
    return;
  }

  try {
    callback(context);
  } catch (const std::exception& ex) {
    spdlog::error("[SectionChangeRouter] observer={} status=failed error='{}'", observer.name, ex.what());
  } catch (...) {
    spdlog::error("[SectionChangeRouter] observer={} status=failed error='unknown exception'", observer.name);
  }
}

void SectionManager_TriggerSectionChange_Router_Hook(auto            original,
                                                     SectionManager* self,
                                                     SectionID       nextSectionID,
                                                     void*           args,
                                                     bool            forcedSectionChange,
                                                     bool            isGoBackStep,
                                                     bool            allowSameSection)
{
  const SectionChangeContext context{
      .manager               = self,
      .next_section          = nextSectionID,
      .args                  = args,
      .forced_section_change = forcedSectionChange,
      .is_go_back_step       = isGoBackStep,
      .allow_same_section    = allowSameSection,
  };

  for (const auto& observer : observers()) {
    notify_observer(observer, context, observer.before_original);
  }

  original(self, nextSectionID, args, forcedSectionChange, isGoBackStep, allowSameSection);

  for (const auto& observer : observers()) {
    notify_observer(observer, context, observer.after_original);
  }
}
} // namespace

void RegisterSectionChangeObserver(SectionChangeObserver observer)
{
  if (observer.name.empty() || (observer.before_original == nullptr && observer.after_original == nullptr)) {
    spdlog::warn("[SectionChangeRouter] ignored invalid observer registration name='{}'", observer.name);
    return;
  }

  for (const auto& existing : observers()) {
    if (existing.name == observer.name) {
      spdlog::warn("[SectionChangeRouter] ignored duplicate observer registration name='{}'", observer.name);
      return;
    }
  }

  observers().push_back(observer);
  spdlog::info("[SectionChangeRouter] registered observer={}", observer.name);
}

bool SectionChangeRouterHasSubscribers()
{ return !observers().empty(); }

void InstallSectionChangeRouterHooks()
{
  HookModuleHealth hooks("SectionChangeRouterHooks");

  if (!SectionChangeRouterHasSubscribers()) {
    hooks.record_skipped(kSectionManagerTriggerSectionChangeHook, "no registered section-change observers");
    hooks.log_summary();
    return;
  }

  auto section_manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sections", "SectionManager");
  if (!section_manager.isValidHelper()) {
    hooks.record_missing_helper(kSectionManagerTriggerSectionChangeHook);
    hooks.log_summary();
    return;
  }

  auto trigger_section_change = section_manager.GetMethod("TriggerSectionChange");
  if (trigger_section_change == nullptr) {
    hooks.record_missing_method(kSectionManagerTriggerSectionChangeHook);
    hooks.log_summary();
    return;
  }

  HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kSectionManagerTriggerSectionChangeHook, trigger_section_change,
                                   SectionManager_TriggerSectionChange_Router_Hook);
  hooks.log_summary();
}
