// Derived from stfc-profiles c5c4ba911f16f3376e828761714d72a26ebe2a96,
// adapters/community_mod/profile_isolation.cc (GPL-3.0).
// That adapter was extracted from Guffawaffle/stfc-mod 323fb857f51f4cb08231d4b150ea8b5bb59340d1.
// The pinned core, RuntimeApi, process admission and browser guardian remain unchanged external inputs.
// Host adaptations: guarded fail-closed reporting, shared signature/target preflight, the existing macOS entry
// extent checks, and one M2 HookSet transaction instead of thirteen permanent individual installs. Preference
// semantics and enrollment/readiness ordering are retained. No ordinary launch invokes these entry points.
#include "hook_targets.h"
#include "runtime_api.h"
#include "stfc_profiles/community_mod_adapter.h"
#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#if _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if defined(__aarch64__) || defined(_M_ARM64)
namespace spud::detail::arm64
{
uintptr_t maybe_resolve_jump(uintptr_t);
}
#else
namespace spud::detail::x64
{
uintptr_t maybe_resolve_jump(uintptr_t);
}
#endif

namespace stfc::profiles::community_mod
{

namespace
{

  std::string                         profile_id;
  std::filesystem::path               profile_root;
  SessionLease*                       profile_lease = nullptr;
  std::unique_ptr<detail::RuntimeApi> runtime;
  std::unique_ptr<ProfilePrefsStore>  profile_store;

  [[noreturn]] void FailClosed(const char* reason) noexcept
  {
    try {
      spdlog::critical("[ProfileIsolationProbe] {}", reason);
      spdlog::default_logger()->flush();
    } catch (...) {
      // Allocation or logger failure cannot turn a required isolation failure into continuation.
    }
    if (profile_lease) {
      try {
        profile_lease->MarkFailed(reason);
      } catch (...) {
      }
    }
#if _WIN32
    TerminateProcess(GetCurrentProcess(), 190);
#else
    _exit(190);
#endif
    std::abort();
  }

  std::u16string_view RequiredString(void* value)
  {
    if (!runtime || !value)
      FailClosed("The client supplied an invalid profile preference string");
    const auto size = runtime->string_length(value);
    auto       data = runtime->string_chars(value);
    if (size < 0 || size > 8 * 1024 * 1024 || !data)
      FailClosed("The client supplied an invalid profile preference string");
    return {data, static_cast<std::size_t>(size)};
  }

  template <typename Action> decltype(auto) WithProfileStore(const char* failure, Action&& action)
  {
    try {
      if (!profile_store)
        FailClosed("Profile preference store is unavailable");
      return std::forward<Action>(action)(*profile_store);
    } catch (...) {
      FailClosed(failure);
    }
  }

  bool TrySetInt_Hook(auto, void* key, int value)
  {
    return WithProfileStore("Could not persist an integer preference", [&](ProfilePrefsStore& store) {
      store.SetInt(RequiredString(key), value);
      return true;
    });
  }

  bool TrySetFloat_Hook(auto, void* key, float value)
  {
    return WithProfileStore("Could not persist a float preference", [&](ProfilePrefsStore& store) {
      store.SetFloat(RequiredString(key), value);
      return true;
    });
  }

  bool TrySetString_Hook(auto, void* key, void* value)
  {
    return WithProfileStore("Could not persist a string preference", [&](ProfilePrefsStore& store) {
      store.SetString(RequiredString(key), RequiredString(value));
      return true;
    });
  }

  int GetInt_Hook(auto, void* key, int fallback)
  {
    return WithProfileStore("Could not read an integer preference",
                            [&](ProfilePrefsStore& store) { return store.GetInt(RequiredString(key), fallback); });
  }

  float GetFloat_Hook(auto, void* key, float fallback)
  {
    return WithProfileStore("Could not read a float preference",
                            [&](ProfilePrefsStore& store) { return store.GetFloat(RequiredString(key), fallback); });
  }

  void* GetString_Hook(auto, void* key, void* fallback)
  {
    return WithProfileStore("Could not read a string preference", [&](ProfilePrefsStore& store) -> void* {
      const auto value = store.GetString(RequiredString(key));
      if (!value) {
        if (fallback)
          return fallback;
        // Unity's Windows PlayerPrefs.GetString(missing, nullptr) returns an empty string.
        constexpr char16_t empty[] = u"";
        auto*              result  = runtime->string_new_utf16(empty, 0);
        if (!result)
          throw std::runtime_error("could not allocate empty preference string");
        return result;
      }
      auto* result = runtime->string_new_utf16(value->data(), static_cast<std::int32_t>(value->size()));
      if (!result)
        throw std::runtime_error("could not allocate preference string");
      return result;
    });
  }

  bool HasKey_Hook(auto, void* key)
  {
    return WithProfileStore("Could not inspect a preference",
                            [&](ProfilePrefsStore& store) { return store.HasKey(RequiredString(key)); });
  }

  void DeleteKey_Hook(auto, void* key)
  {
    WithProfileStore("Could not delete a preference",
                     [&](ProfilePrefsStore& store) { store.DeleteKey(RequiredString(key)); });
  }

  void DeleteAll_Hook(auto)
  {
    WithProfileStore("Could not clear profile preferences", [](ProfilePrefsStore& store) { store.DeleteAll(); });
  }

  void Save_Hook(auto)
  {
    WithProfileStore("Could not save profile preferences", [](ProfilePrefsStore& store) { store.Save(); });
  }

  bool LaunchProfileBrowser(void* url)
  {
    if (!url)
      return false;
    return LaunchIsolatedBrowser(profile_root, profile_id, RequiredString(url));
  }

  bool PresentUrl_Hook(auto, void*, void* url)
  {
    try {
      if (LaunchProfileBrowser(url))
        return true;
    } catch (...) {
    }
    FailClosed("Could not open the isolated sign-in browser");
  }

} // namespace

void PrepareProfile(const std::filesystem::path& root, std::string_view id, ProfileOpenMode mode, SessionLease& lease)
{
  if (profile_store)
    FailClosed("Profile preference store was already prepared");
  profile_root  = root;
  profile_id    = id;
  profile_lease = &lease;
  try {
    profile_store = std::make_unique<ProfilePrefsStore>(root, id, mode, lease);
  } catch (const std::exception& error) {
    FailClosed(error.what());
  } catch (...) {
    FailClosed("Could not open the isolated preference store");
  }
}

void InstallProfileHooks()
{
  if (!profile_store)
    FailClosed("Profile preference store was not prepared");

  if (runtime)
    FailClosed("Profile hooks were already installed");
  try {
    runtime = std::make_unique<detail::RuntimeApi>();
  } catch (const std::exception& error) {
    FailClosed(error.what());
  }
  void* prefs = nullptr;
  void* oidc  = nullptr;
  try {
    prefs = runtime->Class("UnityEngine.CoreModule", "UnityEngine", "PlayerPrefs");
    oidc  = runtime->Class("Playgami.Sdk.Identity.Runtime", "Playgami.Identity.Api.Internal", "OidcAuthorizer");
  } catch (const std::exception& error) {
    FailClosed(error.what());
  }
  if (!prefs || !oidc)
    FailClosed("Required profile classes are unavailable");

  try {
    std::array<void*, detail::kProfileHookCount> methods{};
    for (std::size_t i = 0; i < methods.size(); ++i) {
      const auto& spec   = detail::kProfileHookSpecs[i];
      const auto  klass  = spec.klass == detail::ProfileHookClass::kPreferences ? prefs : oidc;
      auto*       method = runtime->Resolve(klass, spec.method, spec.is_static, spec.result, spec.arguments);
      if (!method)
        FailClosed(spec.klass == detail::ProfileHookClass::kPreferences ? "A preference method is unavailable"
                                                                        : "A browser handoff method is unavailable");
      // Windows preserves the pointer. macOS uses the same conservative entry/extent
      // checks as managed host hooks, before alias comparison or transaction staging.
      methods[i] = il2cpp::CheckedNativeEntry(method);
      if (!methods[i])
        FailClosed("A profile hook has no safely patchable native entry");
    }

    // Preserve the exact backend's canonical-target check on the addresses that
    // will actually be staged (including a checked macOS branch target).
    static_assert(sizeof(void*) == 8);
    const auto canonical_target = [](void* method) {
#if defined(__aarch64__) || defined(_M_ARM64)
      return spud::detail::arm64::maybe_resolve_jump(reinterpret_cast<uintptr_t>(method));
#else
      return spud::detail::x64::maybe_resolve_jump(reinterpret_cast<uintptr_t>(method));
#endif
    };
    std::array<uintptr_t, detail::kProfileHookCount> targets{};
    std::transform(methods.begin(), methods.end(), targets.begin(), canonical_target);
    const auto target_status = detail::ProfileHookTargetsStatus(targets);
    if (target_status == detail::ProfileHookTargetStatus::kAliased)
      FailClosed("Profile hook methods share a native target");
    if (target_status != detail::ProfileHookTargetStatus::kReady)
      FailClosed("The complete profile hook target inventory is unavailable");

    const auto target = [&methods](detail::ProfileHookId id) { return methods[detail::ProfileHookIndex(id)]; };
    using Id          = detail::ProfileHookId;
    il2cpp::HookSet hooks("ProfileIsolation");
    IL2CPP_STAGE_AT(hooks, target(Id::kTrySetInt), TrySetInt_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kTrySetFloat), TrySetFloat_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kTrySetString), TrySetString_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kGetInt), GetInt_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kGetFloat), GetFloat_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kGetString), GetString_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kHasKey), HasKey_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kDeleteKey), DeleteKey_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kDeleteAll), DeleteAll_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kSave), Save_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kPresentLogin), PresentUrl_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kPresentLogout), PresentUrl_Hook);
    IL2CPP_STAGE_AT(hooks, target(Id::kPresentLink), PresentUrl_Hook);
    const auto installed = hooks.CommitDetailed();
    if (!installed.Installed())
      FailClosed(installed.Unsafe() ? "Profile hook state is indeterminate" : "A profile hook could not be installed");
  } catch (...) {
    FailClosed("A profile hook failed to install");
  }
  try {
    profile_store->FinishNewProfile();
    profile_lease->MarkReady();
  } catch (...) {
    FailClosed("Could not finish profile enrollment");
  }
  try {
    spdlog::info("[ProfileIsolationProbe] Active for a dedicated launch profile");
  } catch (...) { /* Installed isolation and readiness do not depend on informational logging. */
  }
}

} // namespace stfc::profiles::community_mod
