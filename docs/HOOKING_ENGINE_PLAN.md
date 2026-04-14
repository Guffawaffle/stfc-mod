# Hooking Engine Replacement Plan

## Problem Statement

The project uses [SPUD](https://github.com/tashcan/spud) (v0.2.0-2) for all
runtime function hooking. SPUD is an inline hooking library that overwrites
function prologues with absolute jumps and generates trampolines for calling the
original function.

This architecture has a recurring instability problem: **SPUD trampoline
corruption**. Crashes manifest as `0xc0000005` (access violation) in "unknown
module" — the dynamically allocated trampoline memory. The crashes are:

- **Deterministic per binary** but sensitive to unrelated code changes
- **Not caused by code bugs** — the same logic works or doesn't depending on
  compiled function layout in the `.obj`
- **Diagnosed as prologue analysis failures** — SPUD's Zydis-based instruction
  decoder sometimes mishandles the target function's prologue, producing a
  trampoline with invalid relocated instructions

### Evidence

| Symptom | Frequency | Root Cause |
|---------|-----------|------------|
| Crash at ~20% game load | Intermittent (~20%) | Trampoline corruption from prologue misanalysis |
| Crash 3s after hook install | Deterministic | Same — first call to corrupted trampoline |
| Adding 3 lines to detour TU triggers crash | Reproducible | MSVC reorders `.obj` layout → changes target prologue alignment → different Zydis analysis path |
| Adding `spdlog::info()` masks crash | Reproducible | Stack frame size change shifts function addresses, giving SPUD a different prologue to analyze |

### SPUD-Specific Weaknesses

1. **No 2GB proximity allocation** — `VirtualAlloc(NULL, ...)` gets arbitrary
   addresses. Every trampoline needs far relocations (absolute 64-bit jumps
   instead of near 32-bit relative jumps), increasing complexity and failure
   surface.

2. **No thread safety during install** — No `SuspendThread`/`FlushInstructionCache`
   coordination. The game's main thread may call a function while SPUD is halfway
   through overwriting its prologue.

3. **Per-trampoline VirtualAlloc** — Each of the ~57 hooks gets a separate memory
   allocation instead of a shared executable pool.

4. **Context via r11** — Uses the volatile r11 register to pass context
   (trampoline pointer, user function pointer) to the wrapper. Legal per Win64
   ABI at function entry, but unconventional and adds complexity to the generated
   code.

5. **Limited testing surface** — Small user base (the author's projects), compared
   to Detours/MinHook which are used by thousands of production tools.

## Current Hook Inventory

**57 active SPUD detours** across 11 source files:

| File | Hooks | Target Type |
|------|-------|-------------|
| `sync.cc` | ~13 | IL2CPP (ParseBinary ×9, ParseRTC ×2, ProcessResult, HandleBinary) |
| `chat.cc` | 6 | IL2CPP (AboutToShow, TabChanged, Focused, GlobalMsg, RegionalMsg) |
| `hotkeys.cc` | 4 | IL2CPP (Update, InitializeActions, OnDidBindContext, ShowWithFleet) |
| `testing.cc` | 4 | IL2CPP (SetCursor, LoadConfigs, SetActive, IsQueueUnlocked) |
| `zoom.cc` | 3 | IL2CPP (Update, SetDepth, SetViewParameters) |
| `misc.cc` | 6 | IL2CPP (MaxItemsToUse, OnAction, GetResolutions, ExtractBuffs, ShouldShowReveal, Interstitial) |
| `object_tracker.cc` | 3 | IL2CPP (ctor, destroy, liveness_finalize) |
| `free_resize.cc` | 2 | IL2CPP (AspectRatio.Update, WndProc) |
| `disable_banners.cc` | 2 | IL2CPP (EnqueueToast, EnqueueOrCombine) |
| `fix_pan.cc` | 2 | IL2CPP (populateWithPosition, LateUpdate) |
| `ui_scale.cc` | 2 | IL2CPP (UpdateCanvasScale, CanvasController.Show) |
| `buff_fixes.cc` | 1 | IL2CPP (IsBuffConditionMet) |
| `improve_responsiveness.cc` | 1 | IL2CPP (TransitionManager.Awake) |
| `patches.cc` | 1 | **NATIVE** (il2cpp_init via GetProcAddress) |

**Key finding: 56 of 57 hooks target IL2CPP methods resolved via `GetMethod()`.
Only 1 hook (il2cpp_init) targets a raw native export.**

## Proposed Solution: IL2CPP Method Pointer Hooking

### Core Insight

Every IL2CPP method is represented by a `MethodInfo` struct:

```cpp
typedef struct MethodInfo {
    Il2CppMethodPointer methodPointer;    // ← direct native function pointer
    Il2CppMethodPointer virtualMethodPointer;
    InvokerMethod invoker_method;
    const char* name;
    Il2CppClass *klass;
    // ...
} MethodInfo;
```

The `methodPointer` field is a **writable function pointer**. The project already
resolves it at runtime:

```cpp
// Current pattern — resolve method pointer, then inline-hook it
auto ptr = screen_manager_helper.GetMethod("Update");
original = SPUD_STATIC_DETOUR(ptr, ScreenManager_Update_Hook);
```

Instead of patching the native function body, we can **swap the pointer itself**:

```cpp
// New pattern — swap the method pointer, save original
auto method_info = il2cpp_class_get_method_from_name(cls, "Update", -1);
auto original = method_info->methodPointer;          // save
method_info->methodPointer = (Il2CppMethodPointer)&our_hook;  // swap
```

### Why This Works

When IL2CPP calls a managed method, it resolves `MethodInfo::methodPointer` and
calls through it. By swapping this pointer:

- **No machine code is modified** — no prologue analysis, no relocation, no
  trampoline
- **No executable memory allocation** — our hook function lives in our DLL's
  normal `.text` section
- **No instruction cache issues** — we're changing a data pointer, not code
- **Thread-safe with InterlockedExchangePointer** — atomic pointer swap
- **Immune to TU sensitivity** — our compiled function layout is irrelevant
  because we never patch code bytes

### Architecture

```
┌─────────────────────────────────────────────────────┐
│                    il2cpp_hook.h                     │
│                                                     │
│  il2cpp_hook_install(class, method, hook_fn)        │
│    → resolves MethodInfo*                           │
│    → saves original methodPointer                   │
│    → swaps methodPointer to hook_fn                 │
│    → returns original as callable function pointer  │
│                                                     │
│  il2cpp_hook_remove(handle)                         │
│    → restores original methodPointer                │
│                                                     │
│  IL2CPP_HOOK(class_helper, method_name, hook_fn)    │
│    → macro that replaces SPUD_STATIC_DETOUR         │
│    → returns original function pointer (same type)  │
└─────────────────────────────────────────────────────┘
```

### Hook Signature Compatibility

Current SPUD pattern passes the trampoline (original) as first argument:

```cpp
// SPUD hook signature
bool ScreenManager_Update_Hook(auto original, ScreenManager* self) {
    // ... logic ...
    return original(self);  // call original via trampoline
}
```

The replacement macro can preserve this pattern by storing the original pointer
in a static and generating a thin wrapper:

```cpp
// Generated by IL2CPP_HOOK macro
static OriginalFn_t s_original_Update = nullptr;

bool ScreenManager_Update_Hook_wrapper(ScreenManager* self) {
    return ScreenManager_Update_Hook(s_original_Update, self);
}
```

Or we can update hook signatures to receive the original differently. The exact
API design is TBD — the goal is minimal churn at hook sites.

### What About VTable / Virtual Methods?

IL2CPP virtual dispatch goes through `Il2CppClass::vtable[]` slots, which
contain `MethodInfo` entries. Same technique applies — find the vtable slot,
swap `methodPointer`. The existing `GetVirtualMethod()` helper already resolves
virtual slots.

For non-virtual IL2CPP methods, direct `MethodInfo::methodPointer` swap is
sufficient since IL2CPP resolves methods through the metadata system.

### What About the 1 Native Hook?

`il2cpp_init` (resolved via `GetProcAddress`) is the only non-IL2CPP hook. For
this single case, we can either:

1. **Import Address Table (IAT) hooking** — swap the function pointer in the
   importing module's IAT. No code patching needed. Simple and well-understood.
2. **Keep a minimal inline hook** — use Microsoft Detours or MinHook for just
   this one hook.
3. **Restructure** — `il2cpp_init` is the entry point hook that triggers all
   other initialization. We could potentially hook `DllMain` or use a different
   initialization trigger.

Recommendation: Option 1 (IAT hook) or Option 3 (restructure). Both avoid any
inline code patching.

## Implementation Plan

### Phase 1: Infrastructure (`il2cpp_hook.h/.cc`)

Create the core hooking primitives:

- [ ] `struct HookHandle` — stores MethodInfo*, original pointer, hook pointer
- [ ] `il2cpp_hook_install(MethodInfo*, hook_fn)` → HookHandle
- [ ] `il2cpp_hook_remove(HookHandle&)` → restores original
- [ ] `IL2CPP_HOOK(method_ptr, hook_fn)` → macro returning original fn pointer
- [ ] Thread-safe swap via `InterlockedExchangePointer`
- [ ] Registration table for cleanup on DLL unload
- [ ] Unit test: hook a known method, verify original is callable, verify hook
  fires, verify unhook restores original

### Phase 2: Migrate Hooks (one file at a time)

Convert each detour file from SPUD to IL2CPP method pointer hooking. Order by
risk (lowest risk first):

1. **`improve_responsiveness.cc`** (1 hook) — simplest, good proof of concept
2. **`buff_fixes.cc`** (1 hook) — simple condition check
3. **`disable_banners.cc`** (2 hooks) — already split into thin wrapper pattern
4. **`fix_pan.cc`** (2 hooks) — touch input
5. **`ui_scale.cc`** (2 hooks) — canvas scaling
6. **`free_resize.cc`** (2 hooks) — aspect ratio + WndProc
7. **`chat.cc`** (6 hooks) — chat UI controllers
8. **`misc.cc`** (6 hooks) — various UI patches
9. **`testing.cc`** (4 hooks) — cursor, queue, etc.
10. **`zoom.cc`** (3 hooks) — navigation zoom
11. **`hotkeys.cc`** (4 hooks) — hotkey system (most complex)
12. **`object_tracker.cc`** (3 hooks) — lifecycle tracking
13. **`sync.cc`** (~13 hooks) — data processing (most hooks, highest risk)

After each file conversion: build, deploy, test in-game.

### Phase 3: Handle il2cpp_init (the 1 native hook)

- [ ] Evaluate IAT hooking vs restructured initialization
- [ ] Implement chosen approach
- [ ] Remove SPUD dependency from `patches.cc`

### Phase 4: Remove SPUD

- [ ] Remove `spud` from xmake package dependencies
- [ ] Remove `xmake-packages/packages/s/spud/` directory
- [ ] Remove `#include <spud/detour.h>` from all files
- [ ] Update build configuration (remove `SPUD_NO_LTO` etc.)
- [ ] Verify LTO can now be re-enabled (the original reason LTO was disabled
  was SPUD incompatibility)

## Risks & Open Questions

### Method Pointer Caching

**Risk**: The game might cache `MethodInfo::methodPointer` into a local variable
at startup, bypassing our swap for subsequent calls.

**Mitigation**: IL2CPP's generated code typically calls through
`MethodInfo::methodPointer` at each call site (the metadata-driven dispatch
model). However, hot paths might be optimized. We need to verify empirically
with the first few hooks:
- Hook `TransitionManager.Awake` (Phase 2, step 1)
- Add logging to confirm our hook fires
- If it doesn't fire, the method pointer was cached → need inline hook fallback

### Virtual Method Dispatch

**Risk**: Some hooks may target virtual methods where the actual dispatch goes
through `Il2CppClass::vtable[]` instead of `MethodInfo::methodPointer`.

**Mitigation**: For virtual methods, we swap the vtable entry instead of (or in
addition to) the MethodInfo pointer. The existing `GetVirtualMethod()` helper
already resolves the correct vtable slot.

### il2cpp_runtime_invoke

**Risk**: Code paths that use `il2cpp_runtime_invoke` (reflection-style calls)
resolve method pointers differently.

**Mitigation**: `il2cpp_runtime_invoke` also reads `MethodInfo::methodPointer`
internally, so our swap should be transparent. Verify with sync.cc hooks.

### DLL Unload Ordering

**Risk**: If our DLL unloads before restoring method pointers, the game will
crash calling into unmapped memory.

**Mitigation**: Restore all hooks in `DllMain(DLL_PROCESS_DETACH)` before
unload. SPUD already has this issue (trampolines are freed on detour
destruction), so this isn't a new risk.

### Performance

**Concern**: Method pointer hooking eliminates the trampoline indirection
entirely. The "original" function is called directly through the saved pointer
— this is actually **faster** than SPUD's trampoline path (which copies and
relocates prologue instructions).

## Success Criteria

- [ ] All 57 hooks migrated from SPUD to IL2CPP method pointer hooking
- [ ] Zero trampoline-related crashes
- [ ] All existing functionality preserved (hotkeys, chat, zoom, sync, etc.)
- [ ] SPUD dependency fully removed
- [ ] Build with LTO re-enabled (if SPUD was the only blocker)
- [ ] No "heisenbug" sensitivity to TU changes — hook stability is independent
  of compiled function layout

## References

- [IL2CPP internals: MethodInfo struct](../third_party/libil2cpp/il2cpp-class-internals.h) (line ~320)
- [Current method resolution: IL2CppClassHelper::GetMethod](../mods/src/il2cpp/il2cpp_helper.h) (line ~166)
- [SPUD source (vendored)](../xmake-packages/packages/s/spud/spud-src/)
- [Known issues: SPUD trampoline corruption](./VERSION_SUBSTITUTION.md)
