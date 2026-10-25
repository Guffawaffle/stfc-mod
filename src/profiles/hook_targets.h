#pragma once

// The exact c5c4ba9 Profiles adapter inventory, shared with game-free preflight fixtures.
// These are managed signatures and native-target identity checks, not account identity or hook installation.

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>

namespace stfc::profiles::community_mod::detail
{
enum class ProfileHookClass { kPreferences, kOidc };
enum class ProfileHookId : std::size_t {
  kTrySetInt,
  kTrySetFloat,
  kTrySetString,
  kGetInt,
  kGetFloat,
  kGetString,
  kHasKey,
  kDeleteKey,
  kDeleteAll,
  kSave,
  kPresentLogin,
  kPresentLogout,
  kPresentLink,
  kCount
};

constexpr std::size_t ProfileHookIndex(ProfileHookId id) noexcept
{ return static_cast<std::size_t>(id); }

inline constexpr std::size_t kProfileHookCount = ProfileHookIndex(ProfileHookId::kCount);

struct ProfileHookSpec {
  ProfileHookClass                   klass;
  const char*                        method;
  bool                               is_static;
  const char*                        result;
  std::initializer_list<const char*> arguments;
};

// TrySetSetString is the native managed name in the pinned qualified adapter; do not correct it by spelling.
inline constexpr std::array<ProfileHookSpec, kProfileHookCount> kProfileHookSpecs{{
    {ProfileHookClass::kPreferences, "TrySetInt", true, "System.Boolean", {"System.String", "System.Int32"}},
    {ProfileHookClass::kPreferences, "TrySetFloat", true, "System.Boolean", {"System.String", "System.Single"}},
    {ProfileHookClass::kPreferences, "TrySetSetString", true, "System.Boolean", {"System.String", "System.String"}},
    {ProfileHookClass::kPreferences, "GetInt", true, "System.Int32", {"System.String", "System.Int32"}},
    {ProfileHookClass::kPreferences, "GetFloat", true, "System.Single", {"System.String", "System.Single"}},
    {ProfileHookClass::kPreferences, "GetString", true, "System.String", {"System.String", "System.String"}},
    {ProfileHookClass::kPreferences, "HasKey", true, "System.Boolean", {"System.String"}},
    {ProfileHookClass::kPreferences, "DeleteKey", true, "System.Void", {"System.String"}},
    {ProfileHookClass::kPreferences, "DeleteAll", true, "System.Void", {}},
    {ProfileHookClass::kPreferences, "Save", true, "System.Void", {}},
    {ProfileHookClass::kOidc, "PresentLoginUrlToUser", false, "System.Boolean", {"System.String"}},
    {ProfileHookClass::kOidc, "PresentLogoutUrlToUser", false, "System.Boolean", {"System.String"}},
    {ProfileHookClass::kOidc, "PresentLinkUrlToUser", false, "System.Boolean", {"System.String"}},
}};

enum class ProfileHookTargetStatus { kReady, kWrongCount, kMissing, kAliased };

constexpr ProfileHookTargetStatus ProfileHookTargetsStatus(std::span<const std::uintptr_t> targets) noexcept
{
  if (targets.size() != kProfileHookCount)
    return ProfileHookTargetStatus::kWrongCount;
  for (std::size_t i = 0; i < targets.size(); ++i) {
    if (!targets[i])
      return ProfileHookTargetStatus::kMissing;
    for (std::size_t j = 0; j < i; ++j)
      if (targets[i] == targets[j])
        return ProfileHookTargetStatus::kAliased;
  }
  return ProfileHookTargetStatus::kReady;
}
} // namespace stfc::profiles::community_mod::detail
