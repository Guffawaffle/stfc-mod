#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

enum class HookAuditStatus {
  Skipped,
  Missing,
  Attempted,
  Installed,
  Failed,
};

struct HookAuditModuleSnapshot {
  std::string module;
  size_t      installed = 0;
  size_t      failed    = 0;
  size_t      skipped   = 0;
  size_t      attempted = 0;
  size_t      total     = 0;
};

void hook_install_audit_record(std::string_view module, std::string_view hook, HookAuditStatus status);

[[nodiscard]] HookAuditModuleSnapshot              hook_install_audit_snapshot(std::string_view module);
[[nodiscard]] std::vector<HookAuditModuleSnapshot> hook_install_audit_snapshots();

/// Test-only: clear process-global hook install observations.
void hook_install_audit_reset_for_testing();
