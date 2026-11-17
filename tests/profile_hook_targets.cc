// Exact pinned signature inventory and canonical-address preflight, without native code or managed metadata.
#include "../src/profiles/hook_targets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

namespace
{
namespace policy = stfc::profiles::community_mod::detail;
using Class      = policy::ProfileHookClass;
using Id         = policy::ProfileHookId;
using Status     = policy::ProfileHookTargetStatus;

void Require(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

void ExactInventory()
{
  // Independent reference from c5c4ba9 profile_isolation.cc, not generated from the production array.
  struct Expected {
    Id                              id;
    Class                           klass;
    std::string_view                method;
    bool                            is_static;
    std::string_view                result;
    std::size_t                     arity;
    std::array<std::string_view, 2> arguments;
  };
  constexpr std::array<Expected, 13> expected{{
      {Id::kTrySetInt, Class::kPreferences, "TrySetInt", true, "System.Boolean", 2, {"System.String", "System.Int32"}},
      {Id::kTrySetFloat,
       Class::kPreferences,
       "TrySetFloat",
       true,
       "System.Boolean",
       2,
       {"System.String", "System.Single"}},
      {Id::kTrySetString,
       Class::kPreferences,
       "TrySetSetString",
       true,
       "System.Boolean",
       2,
       {"System.String", "System.String"}},
      {Id::kGetInt, Class::kPreferences, "GetInt", true, "System.Int32", 2, {"System.String", "System.Int32"}},
      {Id::kGetFloat, Class::kPreferences, "GetFloat", true, "System.Single", 2, {"System.String", "System.Single"}},
      {Id::kGetString, Class::kPreferences, "GetString", true, "System.String", 2, {"System.String", "System.String"}},
      {Id::kHasKey, Class::kPreferences, "HasKey", true, "System.Boolean", 1, {"System.String", {}}},
      {Id::kDeleteKey, Class::kPreferences, "DeleteKey", true, "System.Void", 1, {"System.String", {}}},
      {Id::kDeleteAll, Class::kPreferences, "DeleteAll", true, "System.Void", 0, {}},
      {Id::kSave, Class::kPreferences, "Save", true, "System.Void", 0, {}},
      {Id::kPresentLogin, Class::kOidc, "PresentLoginUrlToUser", false, "System.Boolean", 1, {"System.String", {}}},
      {Id::kPresentLogout, Class::kOidc, "PresentLogoutUrlToUser", false, "System.Boolean", 1, {"System.String", {}}},
      {Id::kPresentLink, Class::kOidc, "PresentLinkUrlToUser", false, "System.Boolean", 1, {"System.String", {}}},
  }};
  static_assert(policy::kProfileHookCount == 13);
  Require(policy::kProfileHookSpecs.size() == expected.size(), "the required target inventory changed size");
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const auto& actual = policy::kProfileHookSpecs[index];
    const auto& pinned = expected[index];
    Require(policy::ProfileHookIndex(pinned.id) == index, "the target IDs changed their pinned order");
    Require(actual.klass == pinned.klass && actual.is_static == pinned.is_static,
            "a target changed class or instance/static semantics");
    Require(actual.method && std::string_view(actual.method) == pinned.method, "a pinned target name changed");
    Require(actual.result && std::string_view(actual.result) == pinned.result, "a pinned result type changed");
    Require(actual.arguments.size() == pinned.arity, "a pinned target arity changed");
    std::size_t argument = 0;
    for (const auto* type : actual.arguments) {
      Require(type && std::string_view(type) == pinned.arguments[argument], "a pinned parameter type/order changed");
      ++argument;
    }
  }
}

void CanonicalAddresses()
{
  // Synthetic address identities are never dereferenced. Canonicalization belongs to the native adapter.
  std::array<std::uintptr_t, 13> unique{};
  for (std::size_t index = 0; index < unique.size(); ++index)
    unique[index] = 0x10000 + index * 0x100;
  Require(policy::ProfileHookTargetsStatus(unique) == Status::kReady, "13 distinct canonical identities were rejected");

  for (std::size_t index = 0; index < unique.size(); ++index) {
    auto missing   = unique;
    missing[index] = 0;
    Require(policy::ProfileHookTargetsStatus(missing) == Status::kMissing, "a missing target was admitted");
  }
  std::size_t pairs = 0;
  for (std::size_t first = 0; first < unique.size(); ++first) {
    for (std::size_t second = first + 1; second < unique.size(); ++second) {
      auto duplicate    = unique;
      duplicate[second] = duplicate[first];
      Require(policy::ProfileHookTargetsStatus(duplicate) == Status::kAliased,
              "two required semantics sharing a canonical target were admitted");
      ++pairs;
    }
  }
  Require(pairs == 78, "not every pair of required target positions was exercised");

  std::array<std::uintptr_t, 14> sized{};
  for (std::size_t index = 0; index < sized.size(); ++index)
    sized[index] = 0x20000 + index * 0x100;
  for (std::size_t count = 0; count <= sized.size(); ++count) {
    if (count == unique.size())
      continue;
    Require(policy::ProfileHookTargetsStatus(std::span(sized).first(count)) == Status::kWrongCount,
            "a partial or oversized inventory was admitted");
  }
  Require(policy::ProfileHookTargetsStatus({}) == Status::kWrongCount, "a default empty span was admitted");
}
} // namespace

int main()
{
  try {
    ExactInventory();
    CanonicalAddresses();
    std::cout << "Profile hook target policy passed: 13 pinned signatures, 13 missing positions, 78 duplicate pairs "
                 "and wrong inventory sizes\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
