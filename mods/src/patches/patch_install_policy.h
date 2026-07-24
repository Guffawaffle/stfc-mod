#pragma once

#include "patches/hook_install_audit.h"

#include <string_view>

enum class PatchInstallReason {
  ConfigEnabled,
  DependencyEnabled,
  ConfigDisabled,
  PlatformUnavailable,
  IsolationBlocked,
};

struct PatchInstallInputs {
  bool requested          = false;
  bool dependency_enabled = false;
  bool platform_available = true;
  bool isolation_allowed  = true;
};

struct PatchInstallDecision {
  bool               requested = false;
  bool               effective = false;
  PatchInstallReason reason    = PatchInstallReason::ConfigDisabled;
};

enum class PatchInstallAuditStatus {
  NotRegistryBacked,
  Consistent,
  MissingRegistryEvidence,
  HookInstallFailed,
  DisabledModuleInstalled,
};

[[nodiscard]] PatchInstallDecision build_patch_install_decision(PatchInstallInputs inputs);
[[nodiscard]] std::string_view     patch_install_reason_name(PatchInstallReason reason);

[[nodiscard]] PatchInstallAuditStatus audit_patch_install(const PatchInstallDecision&    decision,
                                                          const HookAuditModuleSnapshot& hooks, bool registry_backed);
[[nodiscard]] std::string_view        patch_install_audit_status_name(PatchInstallAuditStatus status);
