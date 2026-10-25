#pragma once

// Detours on managed methods and native functions, installed through SPUD transactions. Hooks take the original
// first:
//   void ScreenManager_Update_Hook(auto original, ScreenManager* _this) { ...; original(_this); }
//
//   il2cpp::HookSet hooks("Toasts");
//   IL2CPP_STAGE(hooks, ToastObserver::Class::Method("EnqueueToast"), ToastObserver_EnqueueToast_Hook);
//   return hooks.Commit();

#include "il2cpp.h"

#include <spud/detour.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace il2cpp
{
// The address to detour for `method` on macOS, once SPUD's patch is known to fit inside the function. Small
// wrappers and thunks can be shorter than the patch, and patching past their end corrupts the next function. On
// arm64 an entry that is only a branch yields the branch target instead. Null, after a report, when nothing fits.
// Other platforms return the entry unchecked.
void* CheckedEntry(const Method& method);
// The same platform entry/extent checks for an already signature-qualified native pointer. Returns null when
// unsafe; the named caller owns its diagnostic. Does not resolve or initialize a managed class.
void* CheckedNativeEntry(void* entry);

enum class HookInstallState { kNotAttempted, kInstalled, kMissingTarget, kEmpty, kRejected, kIndeterminate };

struct HookInstallResult {
  HookInstallState state = HookInstallState::kNotAttempted;
  // kOk means no SPUD error was observed, including missing/empty sets and C++ exceptions.
  // It does not imply success; use Installed() and Unsafe().
  spud::detour_error error = spud::detour_error::kOk;

  bool Installed() const noexcept
  { return state == HookInstallState::kInstalled; }
  bool Unsafe() const noexcept
  { return state == HookInstallState::kIndeterminate; }
};

// Process-monotonic safety latch. No later set may apply hooks after an indeterminate outcome.
bool HookStateIndeterminate() noexcept;

class HookInstallError : public std::runtime_error
{
public:
  explicit HookInstallError(HookInstallResult result)
      : std::runtime_error("Hook installation state is indeterminate")
      , result_(result)
  {
  }
  const HookInstallResult& Result() const noexcept
  { return result_; }

private:
  HookInstallResult result_;
};

// One attempt to install one SPUD transaction. Missing/empty sets apply nothing; a rejected transaction must have
// restored each owned handle's prior installed state. Rollback/protection/suspension failures or exceptions are
// indeterminate, not evidence that no native patch remains. Such outcomes latch the process unsafe and retain every
// handle intact. CommitDetailed records the outcome; bool Commit throws HookInstallError for unsafe state so startup
// must stop. Reporting cannot turn an unsafe outcome into an optional feature skip.
// This does not repair backend cleanup or thread redirection performed before SPUD returns the failure; retaining
// wrapper-owned handles and stopping the host is not a guarantee that an unsafe native patch was contained.
//
// SPUD stages only removable hooks, which count calls in and out. After a successful commit they are detached and
// stay for the process. A set is committed once; a hook several features share goes in its own set, installed once.
//
// SPUD chains detours on one function safely, so two hooks may share a target. Identical methods can share
// generated code, though, and a hook on one of them then also runs for the others.
class HookSet
{
public:
  // Names the set in reports; the feature's descriptor name reads best.
  explicit HookSet(std::string_view name)
      : name_(name)
      , owners_(std::make_unique<Owners>())
  {
  }

  template <typename Detour> void Stage(Detour&& detour)
  {
    RequireOpen();
    using Handle = std::remove_cvref_t<Detour>;
    static_assert(std::is_nothrow_constructible_v<Handle, Detour&&>,
                  "Staging must not lose hook custody in a throwing constructor");
    auto& hooks     = owners_->hooks;
    bool  published = false;
    try {
      // Allocate owner capacity before taking custody. A failed growth must
      // leave the caller's handle untouched; earlier staged custody still needs
      // preservation if this allocation or make_shared throws.
      if (hooks.size() == hooks.capacity()) {
        const auto maximum = hooks.max_size();
        if (hooks.size() == maximum)
          throw std::length_error("Too many staged hooks");
        const auto grown = hooks.capacity() > maximum / 2 ? maximum : hooks.capacity() * 2;
        hooks.reserve(grown ? grown : 1);
      }
      // Construction cannot throw after taking custody, and this insertion has
      // reserved storage and noexcept members. Retain before publishing a raw
      // reference into the transaction, whose vector insertion can allocate.
      auto handle = std::make_shared<Handle>(std::forward<Detour>(detour));
      hooks.push_back({handle, [](void* value) { static_cast<Handle*>(value)->detach(); },
                       [](void* value) { return static_cast<Handle*>(value)->is_installed(); }});
      published = true;
      transaction_.install(*handle);
    } catch (...) {
      // Preserve every installed/unknown owner before stack unwinding. Checking
      // only the newest owner would miss earlier preinstalled handles in the set.
      bool installed_or_unknown = false;
      try {
        for (auto& owner : hooks)
          if (owner.installed(owner.handle.get())) {
            installed_or_unknown = true;
            break;
          }
      } catch (...) {
        installed_or_unknown = true;
      }
      if (installed_or_unknown) {
        Indeterminate(spud::detour_error::kOk);
        ReportOutcome("staging failed while installed hook custody could not be safely released");
      } else if (published) {
        hooks.pop_back();
      }
      throw;
    }
  }
  // Marks a member of the set whose method or address is missing.
  void Missing()
  {
    RequireOpen();
    missing_ = true;
  }

  // Terminal results are cached, including success. There is no implicit retry.
  HookInstallResult CommitDetailed();
  bool              Commit();

private:
  struct Owner {
    std::shared_ptr<void> handle;
    void (*detach)(void*);
    bool (*installed)(void*);
    bool before = false;
  };
  struct Owners {
    std::vector<Owner> hooks;
  };

  void              RequireOpen() const;
  HookInstallResult Indeterminate(spud::detour_error error) noexcept;
  HookInstallResult Refused(HookInstallState state, spud::detour_error error, std::string_view why);
  void              ReportOutcome(std::string_view why) const noexcept;

  std::string       name_;
  bool              missing_    = false;
  bool              committing_ = false;
  HookInstallResult result_;
  spud::transaction transaction_;
  // Allocated before any patch can run. Releasing this pointer quarantines all
  // owners on an unsafe path without allocation, destruction or detach calls.
  std::unique_ptr<Owners> owners_;
};
} // namespace il2cpp

// Macros because SPUD_AUTO_HOOK names the hook template itself to instantiate it for the target's signature.

// Stages a managed method's hook in an il2cpp::HookSet. A missing method marks the set incomplete.
#define IL2CPP_STAGE(set, method, hook)                                                                                \
  ([&](const ::il2cpp::Method& method_) { IL2CPP_STAGE_AT(set, method_.Pointer(), hook); }(method))

// Stages a hook on a native function, such as a runtime export or a checked entry. A null address marks the set
// incomplete.
#define IL2CPP_STAGE_AT(set, address, hook)                                                                            \
  ([&](void* target_) {                                                                                                \
    if (target_)                                                                                                       \
      (set).Stage(SPUD_AUTO_HOOK(target_, hook));                                                                      \
    else                                                                                                               \
      (set).Missing();                                                                                                 \
  }(reinterpret_cast<void*>(address)))
