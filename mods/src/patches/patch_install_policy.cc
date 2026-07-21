#include "patches/patch_install_policy.h"

PatchInstallDecision build_patch_install_decision(const PatchInstallInputs inputs)
{
  PatchInstallDecision decision;
  decision.requested = inputs.requested;

  if (!inputs.platform_available) {
    decision.reason = PatchInstallReason::PlatformUnavailable;
    return decision;
  }
  if (!inputs.isolation_allowed) {
    decision.reason = PatchInstallReason::IsolationBlocked;
    return decision;
  }
  if (inputs.requested) {
    decision.effective = true;
    decision.reason    = PatchInstallReason::ConfigEnabled;
    return decision;
  }
  if (inputs.dependency_enabled) {
    decision.effective = true;
    decision.reason    = PatchInstallReason::DependencyEnabled;
    return decision;
  }

  decision.reason = PatchInstallReason::ConfigDisabled;
  return decision;
}

std::string_view patch_install_reason_name(const PatchInstallReason reason)
{
  switch (reason) {
    case PatchInstallReason::ConfigEnabled:
      return "config-enabled";
    case PatchInstallReason::DependencyEnabled:
      return "dependency-enabled";
    case PatchInstallReason::ConfigDisabled:
      return "config-disabled";
    case PatchInstallReason::PlatformUnavailable:
      return "platform-unavailable";
    case PatchInstallReason::IsolationBlocked:
      return "isolation-blocked";
  }
  return "unknown";
}

PatchInstallAuditStatus audit_patch_install(const PatchInstallDecision& decision, const HookAuditModuleSnapshot& hooks,
                                            const bool registry_backed)
{
  if (!registry_backed) {
    return PatchInstallAuditStatus::NotRegistryBacked;
  }
  if (!decision.effective && hooks.installed > 0) {
    return PatchInstallAuditStatus::DisabledModuleInstalled;
  }
  if (decision.effective && hooks.total == 0) {
    return PatchInstallAuditStatus::MissingRegistryEvidence;
  }
  if (decision.effective && (hooks.failed > 0 || hooks.attempted > 0)) {
    return PatchInstallAuditStatus::HookInstallFailed;
  }
  return PatchInstallAuditStatus::Consistent;
}

std::string_view patch_install_audit_status_name(const PatchInstallAuditStatus status)
{
  switch (status) {
    case PatchInstallAuditStatus::NotRegistryBacked:
      return "unmanaged-coverage-gap";
    case PatchInstallAuditStatus::Consistent:
      return "consistent";
    case PatchInstallAuditStatus::MissingRegistryEvidence:
      return "missing-registry-evidence";
    case PatchInstallAuditStatus::HookInstallFailed:
      return "hook-install-failed";
    case PatchInstallAuditStatus::DisabledModuleInstalled:
      return "disabled-module-installed";
  }
  return "unknown";
}
