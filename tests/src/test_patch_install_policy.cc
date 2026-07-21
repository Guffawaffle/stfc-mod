#include <doctest/doctest.h>

#include "patches/hook_install_audit.h"
#include "patches/hook_registry.h"
#include "patches/patch_install_policy.h"

namespace {
void registry_test_function()
{
}
}

TEST_SUITE("patch_install_policy")
{
  TEST_CASE("config and dependency intent produce explainable effective decisions")
  {
    auto decision = build_patch_install_decision({.requested = true});
    CHECK(decision.requested);
    CHECK(decision.effective);
    CHECK(decision.reason == PatchInstallReason::ConfigEnabled);

    decision = build_patch_install_decision({.dependency_enabled = true});
    CHECK_FALSE(decision.requested);
    CHECK(decision.effective);
    CHECK(decision.reason == PatchInstallReason::DependencyEnabled);

    decision = build_patch_install_decision({});
    CHECK_FALSE(decision.effective);
    CHECK(decision.reason == PatchInstallReason::ConfigDisabled);
  }

  TEST_CASE("platform and isolation policy override requested state")
  {
    auto decision = build_patch_install_decision({.requested = true, .platform_available = false});
    CHECK_FALSE(decision.effective);
    CHECK(decision.reason == PatchInstallReason::PlatformUnavailable);

    decision = build_patch_install_decision({.requested = true, .isolation_allowed = false});
    CHECK_FALSE(decision.effective);
    CHECK(decision.reason == PatchInstallReason::IsolationBlocked);
  }

  TEST_CASE("hook audit retains the latest process-lifetime status per module and hook")
  {
    hook_install_audit_reset_for_testing();
    hook_install_audit_record("ZoomHooks", "Update", HookAuditStatus::Attempted);
    hook_install_audit_record("ZoomHooks", "Update", HookAuditStatus::Installed);
    hook_install_audit_record("ZoomHooks", "SetDepth", HookAuditStatus::Missing);
    hook_install_audit_record("ZoomHooks", "Optional", HookAuditStatus::Skipped);

    const auto snapshot = hook_install_audit_snapshot("ZoomHooks");
    CHECK(snapshot.module == "ZoomHooks");
    CHECK(snapshot.total == 3);
    CHECK(snapshot.installed == 1);
    CHECK(snapshot.failed == 1);
    CHECK(snapshot.skipped == 1);
    CHECK(snapshot.attempted == 0);
  }

  TEST_CASE("hook owner keys preserve object and typed function pointer identity")
  {
    int   object = 0;
    auto* object_ptr = &object;
    auto* function_ptr = &registry_test_function;

    const auto object_key = hook_registry_target_key(object_ptr);
    const auto function_key = hook_registry_target_key(function_ptr);

    CHECK(object_key != 0);
    CHECK(function_key != 0);
    CHECK(object_key == hook_registry_target_key(object_ptr));
    CHECK(function_key == hook_registry_target_key(function_ptr));

    hook_registry_reset_owners_for_testing();
    const HookDescriptor object_descriptor{
      .name = "ObjectTarget",
      .purpose = "test object-pointer key",
      .target = {.class_name = "Object", .method_name = "Target"},
      .likely_symptom = "test failure",
    };
    const HookDescriptor function_descriptor{
      .name = "FunctionTarget",
      .purpose = "test typed-function-pointer key",
      .target = {.class_name = "Function", .method_name = "Target"},
      .likely_symptom = "test failure",
    };

    CHECK(hook_registry_claim_owner(object_descriptor, "RegistryTests", object_key));
    CHECK(hook_registry_claim_owner(function_descriptor, "RegistryTests", function_key));
    CHECK(hook_registry_owner_count_for_testing() == 2);
  }

  TEST_CASE("reconciliation identifies leaks failures missing evidence and coverage gaps")
  {
    const auto enabled  = build_patch_install_decision({.requested = true});
    const auto disabled = build_patch_install_decision({});

    CHECK(audit_patch_install(enabled, {.module = "HotkeyHooks", .installed = 2, .total = 2}, true)
          == PatchInstallAuditStatus::Consistent);
    CHECK(audit_patch_install(enabled, {.module = "HotkeyHooks"}, true)
          == PatchInstallAuditStatus::MissingRegistryEvidence);
    CHECK(audit_patch_install(enabled, {.module = "HotkeyHooks", .failed = 1, .total = 1}, true)
          == PatchInstallAuditStatus::HookInstallFailed);
    CHECK(audit_patch_install(disabled, {.module = "HotkeyHooks", .installed = 1, .total = 1}, true)
          == PatchInstallAuditStatus::DisabledModuleInstalled);
    CHECK(audit_patch_install(enabled, {}, false) == PatchInstallAuditStatus::NotRegistryBacked);
  }
}
