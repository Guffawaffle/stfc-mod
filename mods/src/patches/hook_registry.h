/**
 * @file hook_registry.h
 * @brief Lightweight hook install health reporting.
 *
 * This keeps hook diagnostics discoverable without hiding the raw IL2CPP
 * class and method lookup details needed when a game update breaks hooks.
 */
#pragma once

#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <spud/detour.h>

struct HookTarget {
  std::string_view assembly;
  std::string_view namespc;
  std::string_view class_name;
  std::string_view method_name;
};

enum class HookSupportTier {
  Production,
  Science,
  Dormant,
  Internal,
};

struct HookDescriptor {
  std::string_view name;
  std::string_view purpose;
  HookTarget       target;
  std::string_view likely_symptom;
  HookSupportTier  support_tier = HookSupportTier::Production;
};

enum class HookInstallStatus {
  Skipped,
  MissingHelper,
  MissingMethod,
  DetourAttempted,
  DetourInstalled,
  DetourFailed,
};

struct HookInstallRecord {
  HookDescriptor    descriptor;
  HookInstallStatus status = HookInstallStatus::Skipped;
  std::string       detail;
  bool              method_found = false;
  bool              detour_attempted = false;
};

using HookTargetKey = std::uintptr_t;

/**
 * @brief Normalize object and function pointers into an opaque registry key.
 *
 * Hook targets are never dereferenced by the owner registry. Keeping their
 * identity as an integer avoids non-portable function-pointer-to-object-pointer
 * conversions while preserving the typed pointer passed to SPUD.
 */
template <typename Pointer>
  requires std::is_pointer_v<Pointer>
[[nodiscard]] HookTargetKey hook_registry_target_key(Pointer ptr) noexcept
{
  return reinterpret_cast<HookTargetKey>(ptr);
}

class HookModuleHealth {
public:
  explicit HookModuleHealth(std::string_view module);

  void record_skipped(const HookDescriptor& descriptor, std::string_view reason);
  void record_missing_helper(const HookDescriptor& descriptor);
  void record_missing_method(const HookDescriptor& descriptor);
  void record_detour_attempted(const HookDescriptor& descriptor);
  void record_detour_installed(const HookDescriptor& descriptor);
  void record_detour_failed(const HookDescriptor& descriptor, std::string_view error);
  void log_summary() const;
  [[nodiscard]] std::string_view module_name() const;

private:
  std::string                    module_;
  std::vector<HookInstallRecord> records_;

  HookInstallRecord& upsert(const HookDescriptor& descriptor);
  void log_record(const HookInstallRecord& record) const;
};

/**
 * @brief Process-global single-owner registry for IL2CPP detour targets.
 *
 * The community patch must never install two `SPUD_STATIC_DETOUR` hooks against
 * the same IL2CPP method — on macOS the second installation can crash, and on
 * both platforms it produces unpredictable call-through chains. Each call to
 * `HOOK_REGISTRY_SPUD_STATIC_DETOUR` claims its target here, keyed on the
 * resolved IL2CPP method pointer (not the descriptor name) so distinct
 * overloads of the same method name resolve to distinct keys. A duplicate
 * claim logs an error and, in `_MODDBG` builds, triggers `std::abort` so the
 * conflict is impossible to miss in development.
 *
 * @param target_key The normalized address SPUD will detour. Zero is treated
 *                   as a no-op claim (returns true) since callers must already
 *                   have reported a missing method.
 * @return true on the first claim of a given method pointer. false (with an
 *         error log) on duplicate claims.
 */
bool hook_registry_claim_owner(const HookDescriptor& descriptor, std::string_view module, HookTargetKey target_key);

/// Test-only: clear the global owner registry. Not for runtime use.
void hook_registry_reset_owners_for_testing();

/// Test-only: number of claims currently held in the owner registry.
size_t hook_registry_owner_count_for_testing();

#define HOOK_REGISTRY_SPUD_STATIC_DETOUR(registry, descriptor, addr, fn) \
  do { \
    const auto hook_registry_addr = (addr); \
    (registry).record_detour_attempted((descriptor)); \
    if (!hook_registry_claim_owner((descriptor), (registry).module_name(), hook_registry_target_key(hook_registry_addr))) { \
      (registry).record_detour_failed((descriptor), "duplicate detour owner — single-owner policy"); \
      break; \
    } \
    try { \
      SPUD_STATIC_DETOUR(hook_registry_addr, fn); \
      (registry).record_detour_installed((descriptor)); \
    } catch (const std::exception& ex) { \
      (registry).record_detour_failed((descriptor), ex.what()); \
    } catch (...) { \
      (registry).record_detour_failed((descriptor), "unknown exception"); \
    } \
  } while (false)
