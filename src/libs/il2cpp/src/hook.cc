// Owns hook set commits, their failure reports, and the macOS fit check for small targets.

#include <il2cpp/hook.h>

#include <spud/diagnostics.h>

#include <atomic>
#include <format>
#include <string>

#if __APPLE__
#include <dlfcn.h>
#include <mach-o/loader.h>

#include <algorithm>
#include <cstring>
#endif

namespace il2cpp
{
namespace
{
  std::atomic<bool> hook_state_indeterminate = false;

  constexpr const char* DetourErrorName(spud::detour_error error)
  {
    using E = spud::detour_error;
    switch (error) {
      case E::kOk:
        return "ok";
      case E::kUnsupportedArch:
        return "unsupported architecture";
      case E::kDecodeFailed:
        return "prologue could not be decoded";
      case E::kInsufficientBytes:
        return "function too short for a detour";
      case E::kProtectionFailed:
        return "page protection could not be changed";
      case E::kSuspensionUnavailable:
        return "other threads could not be suspended";
      case E::kTargetChanged:
        return "target changed while installing";
      case E::kBusy:
        return "detour busy";
      case E::kChainFull:
        return "too many detours on one function";
      case E::kControlFlowEnforced:
        return "control-flow protection";
      case E::kNotFound:
        return "not found";
      case E::kUnwindFailed:
        return "unwind metadata could not be registered";
      case E::kRollbackFailed:
        return "rollback failed";
    }
    return "unknown error";
  }

  bool UnsafeDetourError(spud::detour_error error) noexcept
  {
    using E = spud::detour_error;
    switch (error) {
      case E::kOk:
      case E::kUnsupportedArch:
      case E::kDecodeFailed:
      case E::kInsufficientBytes:
      case E::kTargetChanged:
      case E::kBusy:
      case E::kChainFull:
      case E::kControlFlowEnforced:
      case E::kNotFound:
      case E::kUnwindFailed:
        return false;
      case E::kProtectionFailed:
      case E::kSuspensionUnavailable:
      case E::kRollbackFailed:
      default:
        // A new vendor error needs qualification before it can be a clean refusal.
        return true;
    }
  }

  void ReportNotInstalled(std::string_view set, std::string_view why)
  { Report(std::format("[IL2CPP] {} not installed: {}", set, why)); }

#if __APPLE__
  // The byte length of the function starting exactly at `entry`, from the image's LC_FUNCTION_STARTS: ULEB128
  // deltas from the start of __TEXT. Zero when the entry is not a recorded function start, or is the last one,
  // whose end is not recorded.
  size_t FunctionExtent(const void* entry)
  {
    Dl_info info{};
    if (!entry || !dladdr(entry, &info) || !info.dli_fbase)
      return 0;
    const auto* header = static_cast<const mach_header_64*>(info.dli_fbase);
    if (header->magic != MH_MAGIC_64)
      return 0;

    const auto*                  cursor   = reinterpret_cast<const uint8_t*>(header + 1);
    const auto*                  end      = cursor + header->sizeofcmds;
    const segment_command_64*    text     = nullptr;
    const segment_command_64*    linkedit = nullptr;
    const linkedit_data_command* starts   = nullptr;
    for (uint32_t i = 0; i < header->ncmds; ++i) {
      if (size_t(end - cursor) < sizeof(load_command))
        return 0;
      const auto* command = reinterpret_cast<const load_command*>(cursor);
      if (command->cmdsize < sizeof(load_command) || command->cmdsize > size_t(end - cursor))
        return 0;
      if (command->cmd == LC_SEGMENT_64 && command->cmdsize >= sizeof(segment_command_64)) {
        const auto* segment = reinterpret_cast<const segment_command_64*>(command);
        if (std::strncmp(segment->segname, "__TEXT", 16) == 0)
          text = segment;
        if (std::strncmp(segment->segname, "__LINKEDIT", 16) == 0)
          linkedit = segment;
      }
      if (command->cmd == LC_FUNCTION_STARTS && command->cmdsize >= sizeof(linkedit_data_command))
        starts = reinterpret_cast<const linkedit_data_command*>(command);
      cursor += command->cmdsize;
    }
    if (!text || !linkedit || !starts || starts->dataoff < linkedit->fileoff)
      return 0;
    const uint64_t offset = starts->dataoff - linkedit->fileoff;
    if (offset > linkedit->filesize || starts->datasize > linkedit->filesize - offset || offset > linkedit->vmsize
        || starts->datasize > linkedit->vmsize - offset)
      return 0;

    const uintptr_t base   = reinterpret_cast<uintptr_t>(header);
    const uintptr_t target = reinterpret_cast<uintptr_t>(entry) - base;
    if (target >= text->vmsize)
      return 0;
    const auto slide = base - text->vmaddr;
    cursor           = reinterpret_cast<const uint8_t*>(slide + linkedit->vmaddr + offset);
    end              = cursor + starts->datasize;
    uint64_t address = 0;
    bool     found   = false;
    while (cursor < end) {
      uint64_t delta = 0;
      unsigned shift = 0;
      uint8_t  byte;
      do {
        if (cursor == end || shift >= 64)
          return 0;
        byte = *cursor++;
        if (shift == 63 && (byte & 0x7e))
          return 0;
        delta |= uint64_t(byte & 0x7f) << shift;
        shift += 7;
      } while (byte & 0x80);
      if (!delta || delta > text->vmsize - address)
        return 0;
      address += delta;
      if (found)
        return address - target;
      if (address > target)
        return 0;
      found = address == target;
    }
    return 0;
  }

  // SPUD's far jump must fit inside the function, and SPUD must patch the entry itself rather than follow it.
  bool DetourFits(const void* entry)
  {
    // Anything shorter cannot hold a far jump on either architecture.
    constexpr size_t kMinimumExtent = 32;
    const auto       extent         = FunctionExtent(entry);
    if (extent < kMinimumExtent)
      return false;
    const auto plan = spud::inspect_detour(entry, spud::Arch::kHost, extent);
    return plan.ok() && plan.resolved_target == reinterpret_cast<uintptr_t>(entry) && plan.far_jump_size != 0
           && plan.far_required_patch_bytes >= plan.far_jump_size && plan.far_required_patch_bytes <= extent;
  }

#if __aarch64__
  // The target of an unconditional `b imm26` at `entry`, when it lands in a loaded image; null otherwise.
  const void* BranchTarget(const void* entry)
  {
    const auto     address     = reinterpret_cast<uintptr_t>(entry);
    const uint32_t instruction = *reinterpret_cast<const uint32_t*>(address);
    if ((instruction & 0xFC000000u) != 0x14000000u)
      return nullptr;
    // Sign-extend the 26-bit word offset.
    const auto imm26 =
        static_cast<int32_t>((instruction & 0x03FFFFFFu) | (instruction & 0x02000000u ? 0xFC000000u : 0));
    const auto target = address + static_cast<intptr_t>(imm26) * 4;
    Dl_info    info{};
    if (!dladdr(reinterpret_cast<const void*>(target), &info) || !info.dli_fbase
        || reinterpret_cast<uintptr_t>(info.dli_fbase) == target)
      return nullptr;
    return reinterpret_cast<const void*>(target);
  }
#endif
#endif
} // namespace

bool HookStateIndeterminate() noexcept
{ return hook_state_indeterminate.load(std::memory_order_acquire); }

void HookSet::RequireOpen() const
{
  if (committing_ || result_.state != HookInstallState::kNotAttempted)
    throw std::logic_error("A hook set is sealed after its first commit attempt");
}

HookInstallResult HookSet::Indeterminate(spud::detour_error error) noexcept
{
  // Publish the unsafe process state before reporting, ownership release or any
  // operation that might allocate. A returning caller may not install more sets.
  hook_state_indeterminate.store(true, std::memory_order_release);
  result_     = {HookInstallState::kIndeterminate, error};
  committing_ = false;
  // Even the destructor must not attempt another uninstall or free a callable
  // still reachable from a partial patch. The container was allocated at create.
  (void)owners_.release();
  return result_;
}

void HookSet::ReportOutcome(std::string_view why) const noexcept
{
  try {
    // SPUD's rollback operations can overwrite thread-local diagnostics. This is
    // a post-transaction detail, not an asserted original failing-step identity.
    const auto  detail = spud::last_failure_detail();
    std::string description(why);
    if (result_.error != spud::detour_error::kOk)
      description += std::format("; {}", DetourErrorName(result_.error));
    if (result_.error != spud::detour_error::kOk && detail.reason)
      description += std::format("; post-transaction detail: {}", detail.reason);
    if (result_.Unsafe())
      Report(std::format("[IL2CPP] {} hook state indeterminate: {}", name_, description));
    else
      ReportNotInstalled(name_, description);
  } catch (...) {
    // A throwing logger or allocation failure cannot change the cached outcome
    // or clear the safety latch. Startup reads the structured result/latch.
  }
}

HookInstallResult HookSet::Refused(HookInstallState state, spud::detour_error error, std::string_view why)
{
  try {
    // A transaction deliberately leaves handles that were installed beforehand
    // installed. Since this set took their custody, preserve them for the process
    // rather than letting ordinary owner destruction uninstall them now.
    for (auto& owner : owners_->hooks)
      if (owner.before)
        owner.detach(owner.handle.get());
  } catch (...) {
    const auto outcome = Indeterminate(error);
    ReportOutcome("pre-existing hook ownership could not be preserved");
    return outcome;
  }
  result_     = {state, error};
  committing_ = false;
  ReportOutcome(why);
  owners_->hooks.clear();
  return result_;
}

HookInstallResult HookSet::CommitDetailed()
{
  if (result_.state != HookInstallState::kNotAttempted)
    return result_;
  RequireOpen();
  committing_ = true;
  if (HookStateIndeterminate()) {
    const auto outcome = Indeterminate(spud::detour_error::kOk);
    ReportOutcome("an earlier hook set left the process unsafe; no transaction attempted");
    return outcome;
  }
  try {
    for (auto& owner : owners_->hooks)
      owner.before = owner.installed(owner.handle.get());
  } catch (...) {
    const auto outcome = Indeterminate(spud::detour_error::kOk);
    ReportOutcome("pre-commit hook state could not be established; no transaction attempted");
    return outcome;
  }
  if (missing_) {
    return Refused(HookInstallState::kMissingTarget, spud::detour_error::kOk, "a method it hooks is missing");
  }
  if (owners_->hooks.empty())
    return Refused(HookInstallState::kEmpty, spud::detour_error::kOk, "no hooks were staged");

  spud::detour_error error = spud::detour_error::kOk;
  try {
    error = transaction_.commit();
  } catch (...) {
    const auto outcome = Indeterminate(spud::detour_error::kOk);
    ReportOutcome("the transaction threw; no clean rollback is established");
    return outcome;
  }
  if (error != spud::detour_error::kOk) {
    // Protection failure can follow a target memcpy and failed cache flush.
    // Suspension failure can follow partial thread-IP redirection. Neither error
    // alone proves that native state survived, even if handle flags match.
    if (UnsafeDetourError(error)) {
      const auto outcome = Indeterminate(error);
      ReportOutcome("the transaction did not establish a safe native postcondition");
      return outcome;
    }
    try {
      for (auto& owner : owners_->hooks) {
        if (owner.installed(owner.handle.get()) != owner.before) {
          const auto outcome = Indeterminate(error);
          ReportOutcome("a failed transaction left a staged handle in a different installed state");
          return outcome;
        }
      }
    } catch (...) {
      const auto outcome = Indeterminate(error);
      ReportOutcome("post-transaction hook state could not be established");
      return outcome;
    }
    return Refused(HookInstallState::kRejected, error, "the transaction was refused and handle states were preserved");
  }
  try {
    for (auto& owner : owners_->hooks)
      owner.detach(owner.handle.get());
  } catch (...) {
    const auto outcome = Indeterminate(spud::detour_error::kOk);
    ReportOutcome("the transaction applied but process-lifetime ownership was not established");
    return outcome;
  }
  result_     = {HookInstallState::kInstalled, spud::detour_error::kOk};
  committing_ = false;
  owners_->hooks.clear();
  return result_;
}

bool HookSet::Commit()
{
  const auto outcome = CommitDetailed();
  if (outcome.Unsafe())
    throw HookInstallError(outcome);
  // Detailed results preserve this set's historical outcome. A successful old
  // set must not authorize continued startup after another set latches unsafe.
  if (HookStateIndeterminate())
    throw HookInstallError({HookInstallState::kIndeterminate, spud::detour_error::kOk});
  return outcome.Installed();
}

void* CheckedEntry(const Method& method)
{
  void* entry = method.Pointer();
#if __APPLE__
  if (!entry || DetourFits(entry))
    return entry;
#if __aarch64__
  if (const auto* body = BranchTarget(entry); body && DetourFits(body))
    return const_cast<void*>(body);
#endif
  Report(std::format("[IL2CPP] {} is too small for a detour; not detoured", method.Name()));
  return nullptr;
#else
  return entry;
#endif
}
} // namespace il2cpp
