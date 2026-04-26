/**
 * @file hook_registry.h
 * @brief Lightweight hook install health reporting.
 *
 * This keeps hook diagnostics discoverable without hiding the raw IL2CPP
 * class and method lookup details needed when a game update breaks hooks.
 */
#pragma once

#include <exception>
#include <string>
#include <string_view>
#include <vector>

#include <spud/detour.h>

struct HookTarget {
  std::string_view assembly;
  std::string_view namespc;
  std::string_view class_name;
  std::string_view method_name;
};

struct HookDescriptor {
  std::string_view name;
  std::string_view purpose;
  HookTarget       target;
  std::string_view likely_symptom;
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

private:
  std::string                    module_;
  std::vector<HookInstallRecord> records_;

  HookInstallRecord& upsert(const HookDescriptor& descriptor);
  void log_record(const HookInstallRecord& record) const;
};

#define HOOK_REGISTRY_SPUD_STATIC_DETOUR(registry, descriptor, addr, fn) \
  do { \
    (registry).record_detour_attempted((descriptor)); \
    try { \
      SPUD_STATIC_DETOUR((addr), fn); \
      (registry).record_detour_installed((descriptor)); \
    } catch (const std::exception& ex) { \
      (registry).record_detour_failed((descriptor), ex.what()); \
    } catch (...) { \
      (registry).record_detour_failed((descriptor), "unknown exception"); \
    } \
  } while (false)
