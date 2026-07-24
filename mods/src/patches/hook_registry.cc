#include "patches/hook_registry.h"

#include "patches/hook_install_audit.h"

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace {
const char* hook_status_name(HookInstallStatus status)
{
  switch (status) {
    case HookInstallStatus::Skipped:         return "skipped";
    case HookInstallStatus::MissingHelper:   return "missing-helper";
    case HookInstallStatus::MissingMethod:   return "missing-method";
    case HookInstallStatus::DetourAttempted: return "detour-attempted";
    case HookInstallStatus::DetourInstalled: return "installed";
    case HookInstallStatus::DetourFailed:    return "detour-failed";
  }

  return "unknown";
}

const char* hook_support_tier_name(HookSupportTier tier)
{
  switch (tier) {
    case HookSupportTier::Production: return "production";
    case HookSupportTier::Science:    return "science";
    case HookSupportTier::Dormant:    return "dormant";
    case HookSupportTier::Internal:   return "internal";
  }

  return "unknown";
}

std::string hook_target_string(const HookTarget& target)
{
  std::ostringstream out;
  if (!target.assembly.empty()) {
    out << target.assembly << ":";
  }
  if (!target.namespc.empty()) {
    out << target.namespc << ".";
  }
  out << target.class_name << "->" << target.method_name;
  return out.str();
}
}

HookModuleHealth::HookModuleHealth(std::string_view module)
  : module_(module)
{
}

void HookModuleHealth::record_skipped(const HookDescriptor& descriptor, std::string_view reason)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::Skipped;
  record.detail = reason;
  hook_install_audit_record(module_, descriptor.name, HookAuditStatus::Skipped);
  log_record(record);
}

void HookModuleHealth::record_missing_helper(const HookDescriptor& descriptor)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::MissingHelper;
  record.detail = "class/helper lookup failed";
  hook_install_audit_record(module_, descriptor.name, HookAuditStatus::Missing);
  log_record(record);
}

void HookModuleHealth::record_missing_method(const HookDescriptor& descriptor)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::MissingMethod;
  record.detail = "method lookup failed";
  hook_install_audit_record(module_, descriptor.name, HookAuditStatus::Missing);
  log_record(record);
}

void HookModuleHealth::record_detour_attempted(const HookDescriptor& descriptor)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::DetourAttempted;
  record.method_found = true;
  record.detour_attempted = true;
  hook_install_audit_record(module_, descriptor.name, HookAuditStatus::Attempted);
}

void HookModuleHealth::record_detour_installed(const HookDescriptor& descriptor)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::DetourInstalled;
  record.method_found = true;
  record.detour_attempted = true;
  record.detail.clear();
  hook_install_audit_record(module_, descriptor.name, HookAuditStatus::Installed);
  log_record(record);
}

void HookModuleHealth::record_detour_failed(const HookDescriptor& descriptor, std::string_view error)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::DetourFailed;
  record.method_found = true;
  record.detour_attempted = true;
  record.detail = error;
  hook_install_audit_record(module_, descriptor.name, HookAuditStatus::Failed);
  log_record(record);
}

std::string_view HookModuleHealth::module_name() const
{ return module_; }

void HookModuleHealth::log_summary() const
{
  auto installed = 0;
  auto failed = 0;
  auto skipped = 0;

  for (const auto& record : records_) {
    switch (record.status) {
      case HookInstallStatus::DetourInstalled:
        ++installed;
        break;
      case HookInstallStatus::Skipped:
        ++skipped;
        break;
      case HookInstallStatus::MissingHelper:
      case HookInstallStatus::MissingMethod:
      case HookInstallStatus::DetourFailed:
        ++failed;
        break;
      case HookInstallStatus::DetourAttempted:
        ++failed;
        break;
    }
  }

  spdlog::info("[HookRegistry] module={} summary installed={} failed={} skipped={} total={}",
               module_,
               installed,
               failed,
               skipped,
               records_.size());
}

HookInstallRecord& HookModuleHealth::upsert(const HookDescriptor& descriptor)
{
  const auto same_hook = [&descriptor](const HookInstallRecord& record) {
    return record.descriptor.name == descriptor.name;
  };

  if (auto it = std::ranges::find_if(records_, same_hook); it != records_.end()) {
    return *it;
  }

  records_.push_back({ descriptor });
  return records_.back();
}

void HookModuleHealth::log_record(const HookInstallRecord& record) const
{
  const auto target = hook_target_string(record.descriptor.target);
  const auto status = hook_status_name(record.status);
  const auto tier   = hook_support_tier_name(record.descriptor.support_tier);

  if (record.status == HookInstallStatus::DetourInstalled || record.status == HookInstallStatus::Skipped) {
    spdlog::info("[HookRegistry] module={} hook={} tier={} status={} target={} method_found={} detour_attempted={} purpose='{}' detail='{}'",
                 module_,
                 record.descriptor.name,
                 tier,
                 status,
                 target,
                 record.method_found,
                 record.detour_attempted,
                 record.descriptor.purpose,
                 record.detail);
    return;
  }

  spdlog::error("[HookRegistry] module={} hook={} tier={} status={} target={} method_found={} detour_attempted={} purpose='{}' symptom='{}' detail='{}'",
                module_,
                record.descriptor.name,
                tier,
                status,
                target,
                record.method_found,
                record.detour_attempted,
                record.descriptor.purpose,
                record.descriptor.likely_symptom,
                record.detail);
}

// ---------------------------------------------------------------------------
// Process-global single-owner registry (issue #97).
//
// Indexed by the resolved IL2CPP method pointer — that is the actual unit
// SPUD/MinHook would conflict on. Keying on descriptor name or class.method
// would false-positive on distinct overloads of the same method name (e.g.
// FleetBarViewController.RequestSelect(int, bool) vs RequestSelect(Component,
// bool) — both resolved separately via GetMethodSpecial).
// ---------------------------------------------------------------------------

namespace {
struct OwnerEntry {
  std::string hook_name;
  std::string module;
  std::string target;
};

std::mutex& owner_registry_mutex()
{
  static std::mutex m;
  return m;
}

std::unordered_map<HookTargetKey, OwnerEntry>& owner_registry()
{
  static std::unordered_map<HookTargetKey, OwnerEntry> entries;
  return entries;
}
} // namespace

bool hook_registry_claim_owner(const HookDescriptor& descriptor, const std::string_view module,
                               const HookTargetKey target_key)
{
  if (target_key == 0) {
    // Caller already reported the missing method. Don't add a noisy log.
    return true;
  }

  std::lock_guard<std::mutex> lock(owner_registry_mutex());
  auto&                       entries = owner_registry();

  if (const auto it = entries.find(target_key); it != entries.end()) {
    spdlog::error("[HookOwnerConflict] target={} already owned by hook='{}' (module={}); duplicate claim by hook='{}' "
                  "(module={}, target={}, tier={}). Refusing to install second detour.",
                  it->second.target, it->second.hook_name, it->second.module, descriptor.name, module,
                  hook_target_string(descriptor.target), hook_support_tier_name(descriptor.support_tier));
#if _MODDBG
    // In debug builds, fail loud so the conflict cannot be missed during development.
    std::abort();
#else
    return false;
#endif
  }

  entries.emplace(target_key, OwnerEntry{std::string(descriptor.name), std::string(module),
                                         hook_target_string(descriptor.target)});
  return true;
}

void hook_registry_reset_owners_for_testing()
{
  std::lock_guard<std::mutex> lock(owner_registry_mutex());
  owner_registry().clear();
  hook_install_audit_reset_for_testing();
}

size_t hook_registry_owner_count_for_testing()
{
  std::lock_guard<std::mutex> lock(owner_registry_mutex());
  return owner_registry().size();
}
