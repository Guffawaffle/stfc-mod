#include "patches/hook_install_audit.h"

#include <mutex>
#include <unordered_map>

namespace
{
struct HookAuditEntry {
  HookAuditStatus status = HookAuditStatus::Skipped;
};

using HookEntries = std::unordered_map<std::string, HookAuditEntry>;

std::mutex& audit_mutex()
{
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<std::string, HookEntries>& audit_modules()
{
  static std::unordered_map<std::string, HookEntries> modules;
  return modules;
}

HookAuditModuleSnapshot build_snapshot(const std::string_view module, const HookEntries* entries)
{
  HookAuditModuleSnapshot snapshot;
  snapshot.module = module;
  if (!entries) {
    return snapshot;
  }

  snapshot.total = entries->size();
  for (const auto& [_, entry] : *entries) {
    switch (entry.status) {
      case HookAuditStatus::Installed:
        ++snapshot.installed;
        break;
      case HookAuditStatus::Failed:
      case HookAuditStatus::Missing:
        ++snapshot.failed;
        break;
      case HookAuditStatus::Skipped:
        ++snapshot.skipped;
        break;
      case HookAuditStatus::Attempted:
        ++snapshot.attempted;
        break;
    }
  }
  return snapshot;
}
} // namespace

void hook_install_audit_record(const std::string_view module, const std::string_view hook, const HookAuditStatus status)
{
  std::lock_guard lock(audit_mutex());
  audit_modules()[std::string(module)][std::string(hook)].status = status;
}

HookAuditModuleSnapshot hook_install_audit_snapshot(const std::string_view module)
{
  std::lock_guard lock(audit_mutex());
  const auto&     modules = audit_modules();
  const auto      found   = modules.find(std::string(module));
  return build_snapshot(module, found == modules.end() ? nullptr : &found->second);
}

std::vector<HookAuditModuleSnapshot> hook_install_audit_snapshots()
{
  std::lock_guard                      lock(audit_mutex());
  std::vector<HookAuditModuleSnapshot> snapshots;
  snapshots.reserve(audit_modules().size());
  for (const auto& [module, entries] : audit_modules()) {
    snapshots.push_back(build_snapshot(module, &entries));
  }
  return snapshots;
}

void hook_install_audit_reset_for_testing()
{
  std::lock_guard lock(audit_mutex());
  audit_modules().clear();
}
