#include "patches/hook_registry.h"

#include <algorithm>
#include <sstream>

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
  log_record(record);
}

void HookModuleHealth::record_missing_helper(const HookDescriptor& descriptor)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::MissingHelper;
  record.detail = "class/helper lookup failed";
  log_record(record);
}

void HookModuleHealth::record_missing_method(const HookDescriptor& descriptor)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::MissingMethod;
  record.detail = "method lookup failed";
  log_record(record);
}

void HookModuleHealth::record_detour_attempted(const HookDescriptor& descriptor)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::DetourAttempted;
  record.method_found = true;
  record.detour_attempted = true;
}

void HookModuleHealth::record_detour_installed(const HookDescriptor& descriptor)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::DetourInstalled;
  record.method_found = true;
  record.detour_attempted = true;
  record.detail.clear();
  log_record(record);
}

void HookModuleHealth::record_detour_failed(const HookDescriptor& descriptor, std::string_view error)
{
  auto& record = upsert(descriptor);
  record.status = HookInstallStatus::DetourFailed;
  record.method_found = true;
  record.detour_attempted = true;
  record.detail = error;
  log_record(record);
}

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

  if (record.status == HookInstallStatus::DetourInstalled || record.status == HookInstallStatus::Skipped) {
    spdlog::info("[HookRegistry] module={} hook={} status={} target={} method_found={} detour_attempted={} purpose='{}' detail='{}'",
                 module_,
                 record.descriptor.name,
                 status,
                 target,
                 record.method_found,
                 record.detour_attempted,
                 record.descriptor.purpose,
                 record.detail);
    return;
  }

  spdlog::error("[HookRegistry] module={} hook={} status={} target={} method_found={} detour_attempted={} purpose='{}' symptom='{}' detail='{}'",
                module_,
                record.descriptor.name,
                status,
                target,
                record.method_found,
                record.detour_attempted,
                record.descriptor.purpose,
                record.descriptor.likely_symptom,
                record.detail);
}
