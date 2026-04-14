# Hooking Engine Replacement Plan

## Thesis

SPUD should be replaced by an **IL2CPP-aware hook backend** that uses
metadata/vtable pointer patching by default and falls back to minimal native
inline hooking only where runtime dispatch bypasses those slots.

This is not "SPUD out, methodPointer swap in, done." It is: build a new
IL2CPP-first hook backend, then keep a minimal fallback path for the cases it
does not cover.

## Problem Statement

The project uses [SPUD](https://github.com/tashcan/spud) (v0.2.0-2) for all
runtime function hooking. SPUD is an inline hooking library that overwrites
function prologues with absolute jumps and generates trampolines for calling the
original function.

This architecture has a recurring instability problem: **probable SPUD
trampoline corruption**. Crashes manifest as `0xc0000005` (access violation) in
"unknown module" — the dynamically allocated trampoline memory. The crashes are:

- **Deterministic per binary** but sensitive to unrelated code changes
- **Plausibly not caused by code bugs** — the same logic works or doesn't
  depending on compiled function layout in the `.obj`
- **Likely prologue analysis failures** — SPUD's Zydis-based instruction decoder
  appears to mishandle certain target function prologues, producing trampolines
  with invalid relocated instructions

These are engineering hypotheses based on observed evidence, not proven verdicts.
The evidence is consistent with prologue analysis failures, but we have not
instrumented SPUD's decoder to confirm the exact failure path.

### Evidence

| Symptom | Frequency | Hypothesis |
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

## Proposed Solution: IL2CPP-Aware Hybrid Hook Backend

### Core Insight

Most hooks target IL2CPP methods resolved through the metadata system. These
methods have writable dispatch pointers at the metadata level. By swapping those
pointers instead of patching native code, we eliminate the entire class of
prologue/trampoline failures — **for hooks where runtime dispatch actually reads
those pointers**.

That caveat is critical. A method being *discoverable* through IL2CPP metadata
does **not** prove that all callers go through `MethodInfo->methodPointer` every
time. IL2CPP-generated code frequently calls method bodies directly — the
metadata is used for reflection, lookup, some runtime dispatch, delegates,
virtual/interface resolution, and various engine-side interactions, but not
necessarily every ordinary managed-to-managed call.

So the plan is not "swap all pointers and declare victory." It is: **build a
metadata-first hooking backend, prove which hooks it covers empirically, and
keep a minimal inline-hook fallback for the rest.**

### IL2CPP Dispatch Surfaces

There are multiple pointer slots in the IL2CPP runtime, each used by different
call paths:

#### 1. `MethodInfo::methodPointer`

```cpp
typedef struct MethodInfo {
    Il2CppMethodPointer methodPointer;       // ← non-virtual direct pointer
    Il2CppMethodPointer virtualMethodPointer; // ← used in some virtual paths
    InvokerMethod invoker_method;            // ← used by runtime_invoke
    const char* name;
    Il2CppClass *klass;
    uint16_t slot;                           // ← vtable slot index
    // ...
} MethodInfo;
```

This is the pointer our `GetMethod<T>()` currently returns. Swapping it catches:
- Calls that resolve through the metadata at call time
- `il2cpp_runtime_invoke` (reads `MethodInfo::methodPointer` internally)
- Delegate construction that reads this field at bind time (but NOT delegates
  that already cached the old pointer before our swap)

It does NOT catch:
- IL2CPP-generated code that calls the native body address directly
- Callers that cached the pointer before we installed
- Virtual dispatch (which goes through a different slot — see below)

#### 2. `VirtualInvokeData::methodPtr` (vtable slots)

Virtual dispatch goes through `Il2CppClass::vtable[]`, which is an array of
`VirtualInvokeData`:

```cpp
typedef struct VirtualInvokeData {
    Il2CppMethodPointer methodPtr;   // ← the function pointer for virtual calls
    const MethodInfo* method;        // ← metadata (not the dispatch pointer!)
} VirtualInvokeData;
```

The generated dispatch code is:
```cpp
// il2cpp_codegen_get_virtual_invoke_data(slot, obj)
return obj->klass->vtable[slot];  // returns VirtualInvokeData, caller uses .methodPtr
```

So for virtual methods, we need to swap `vtable[slot].methodPtr`, NOT
`MethodInfo::methodPointer`. They are separate fields. The original plan's
claim that "same technique applies" was wrong — it is the same *idea* (swap a
data pointer) but a different *slot* that needs patching.

Interface dispatch has its own resolution path through
`ClassInlines::GetInterfaceInvokeDataFromVTable()` which computes an offset
into the same vtable array. The slot is different but the mechanism is the same.

#### 3. `MethodInfo::virtualMethodPointer`

Used in some generic virtual dispatch paths:
```cpp
invokeData->methodPtr = invokeData->method->virtualMethodPointer;
```

This is yet another field that may need patching for generic virtual methods.

#### 4. Direct calls (no pointer indirection)

IL2CPP-generated code can call a method body directly by address — no metadata
lookup, no vtable, just `call <address>`. This is common for non-virtual,
non-generic, same-assembly calls. **Metadata pointer swapping cannot intercept
these.** If a hook target is primarily reached through direct calls, we need
inline hooking as a fallback.

### Architecture: Hybrid Backend

```
┌─────────────────────────────────────────────────────────────────┐
│                       il2cpp_hook.h                             │
│                                                                 │
│  Strategy A: Metadata/vtable pointer swap (preferred)           │
│    il2cpp_hook_install(MethodInfo*, hook_fn)                    │
│      → swaps methodPointer (non-virtual)                       │
│      → swaps vtable[slot].methodPtr (virtual)                  │
│      → optionally swaps virtualMethodPointer (generic virtual) │
│      → returns original as callable function pointer            │
│                                                                 │
│  Strategy B: Inline hook fallback (for bypass cases)            │
│    inline_hook_install(void* target, void* hook_fn)             │
│      → uses MinHook/Detours for the native body                │
│      → used only where Strategy A is empirically insufficient  │
│                                                                 │
│  Strategy C: Native export hook (il2cpp_init only)              │
│    → MinHook/Detours for the single GetProcAddress case         │
│    → or restructure initialization to avoid hooking entirely   │
│                                                                 │
│  IL2CPP_HOOK(method_ptr, hook_fn) → replaces SPUD_STATIC_DETOUR│
│  il2cpp_hook_remove(handle) → restores original pointer         │
│  il2cpp_hook_remove_all() → bulk restore for shutdown           │
└─────────────────────────────────────────────────────────────────┘
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

### What About the 1 Native Hook?

`il2cpp_init` (resolved via `GetProcAddress`) is the only non-IL2CPP hook.

**IAT hooking is probably not viable here.** If the caller obtains `il2cpp_init`
dynamically via `GetProcAddress`, there may be no IAT entry to patch. The
fallback options are:

1. **Minimal inline hook** — use MinHook or Detours for just this one target.
   Simple, well-tested, and we already need a fallback path for Strategy B.
2. **Restructure initialization** — `il2cpp_init` is the entry point hook that
   triggers all other initialization. We could potentially use `DllMain` or a
   different initialization trigger to avoid hooking it at all.
3. **Hook `GetProcAddress` itself** — intercept the resolution and return our
   wrapper. Works but is heavy-handed.

Recommendation: Option 1 (minimal inline hook) since we need the fallback
library anyway, or Option 2 (restructure) if it is feasible.

### Generics and Shared Methods

This is a real concern that does not exist with inline hooking.

With inline hooking (SPUD), you hook the **native body address**. That catches
everything that reaches that body, regardless of which metadata entry points
to it.

With metadata pointer swapping, you hook a **dispatch slot**. That is more
selective, which means:

- **Multiple `MethodInfo*`s → same native body**: SPUD catches all callers;
  pointer swap only catches callers that go through the specific slot you
  patched. Generic sharing can create multiple metadata entries pointing at the
  same compiled body.
- **One `MethodInfo*` → shared generic body**: Some methods share compiled code
  across type parameters. You may need to patch more than one metadata slot to
  catch all instantiations.

Before migrating each hook, we should build an **address census**: record the
resolved `MethodInfo*`, `methodPointer`, `virtualMethodPointer`, vtable slot,
and check for duplicates and fan-out. This is part of Phase 0.

### Delegates and Cached Call Targets

If something grabs the function pointer before we swap it and stores it in a
delegate, callback table, or local variable, later calls bypass our hook
entirely. This is especially relevant for:

- UI event handlers (registered at setup time)
- Engine-registered lifecycle callbacks (`Update`, `Awake`, `LateUpdate`)
- Unity's internal method group caching

The real questions are:
- Does the hook fire in the live path we care about?
- Does it fire consistently after registration/caching moments?
- Does install timing matter?

These need explicit testing in Phase 0, not assumptions.

### Thread Safety

`InterlockedExchangePointer` makes the pointer write itself atomic. That is
sufficient for the swap operation.

It does **not** solve:
- Call sites that already cached the old pointer in a register or local
- Places that do not read the swapped slot at call time
- Dispatch paths using a different slot entirely

So atomicity is about correctness of the write, not coverage of interception.
Coverage is determined by whether the live call path reads the slot we patched,
which is an empirical question answered in Phase 0.

## Implementation Plan

### Phase 0: Proof Matrix (the real gate)

Before building infrastructure, prove which dispatch paths are actually
interceptable via pointer swapping. Pick representative hooks that exercise
different dispatch modes:

#### Test Hooks

| # | Hook | Dispatch Mode | Why |
|---|------|--------------|-----|
| A | `TransitionManager.Awake` | Unity lifecycle callback | Engine calls Awake — does it read MethodInfo or call directly? |
| B | `ScreenManager.Update` or `AspectRatio.Update` | Virtual/Update loop | Unity Update goes through... what? Likely cached. |
| C | `ChatPanelHudController.AboutToShow` | UI event/delegate-ish | Probably registered as a callback — cached pointer? |
| D | `ParseBinaryResponse` (any sync hook) | Direct call from generated code | High-traffic, likely compiled as direct call |

For each test hook, answer:

- [ ] **Does `methodPointer` swap catch it?** Hook it, log entry, run the game,
  trigger the path. If the log fires → this method is reachable via metadata.
- [ ] **Does `vtable[slot].methodPtr` swap matter?** Check if the method has a
  vtable slot (`method->slot != 0xFFFF`). If so, also swap the vtable entry and
  test.
- [ ] **Does `virtualMethodPointer` matter?** Compare `methodPointer` vs
  `virtualMethodPointer` — are they different addresses?
- [ ] **Does install timing matter?** Hook before vs after the first call to the
  method. Does behavior differ?
- [ ] **Is the original callable safely?** Call the saved original pointer. Does
  it return correct results?
- [ ] **Does unhook restore behavior?** Restore the original pointer. Does the
  game behave normally?

#### Address Census

For each of the 57 current hooks, record:
- `MethodInfo*` address
- `methodPointer` value
- `virtualMethodPointer` value
- `vtable[slot].methodPtr` value (if virtual)
- Whether multiple `MethodInfo*`s share the same `methodPointer` (generic
  sharing detection)

Build a small diagnostic that dumps this data at hook install time.

#### Phase 0 Exit Criteria

- **Green**: Most hooks interceptable via pointer swap → proceed with hybrid
  backend, Strategy A as default
- **Yellow**: Some hooks need inline fallback → proceed with hybrid backend,
  document which hooks need Strategy B
- **Red**: Most hooks bypass metadata dispatch → pointer swap is not viable as
  the primary strategy, pivot to MinHook/Detours as the primary backend

### Phase 1: Infrastructure (`il2cpp_hook.h/.cc`)

Create the core hooking primitives. Architecture as a **strategy-based backend**:

- [ ] `enum class HookStrategy { MetadataSwap, VtableSwap, InlineHook }`
- [ ] `struct HookHandle` — stores MethodInfo*, original pointer, hook pointer,
  strategy used, vtable slot (if applicable)
- [ ] `il2cpp_hook_install(MethodInfo*, hook_fn, strategy)` → HookHandle
  - `MetadataSwap`: swaps `methodPointer` via `InterlockedExchangePointer`
  - `VtableSwap`: swaps `vtable[slot].methodPtr` (and optionally
    `methodPointer` + `virtualMethodPointer` for belt-and-suspenders)
  - `InlineHook`: delegates to MinHook/Detours
- [ ] `il2cpp_hook_remove(HookHandle&)` → restores original
- [ ] `il2cpp_hook_remove_all()` → bulk restore for shutdown
- [ ] `IL2CPP_HOOK(method_ptr, hook_fn)` → macro returning original fn pointer
- [ ] Registration table: vector of all active HookHandles for cleanup
- [ ] Diagnostic mode: log at install time which strategy was used, dump address
  census data, optionally verify the hook fires on first call

### Phase 2: Migrate Hooks (ordered by representativeness)

Convert each detour file from SPUD to the new backend. Order chosen to answer
architectural questions early, not just land easy wins:

**Wave 1 — Validator hooks** (exercise different dispatch modes):

1. **`improve_responsiveness.cc`** (1 hook: `Awake`) — Unity lifecycle
2. **`zoom.cc`** (3 hooks: `Update`, `SetDepth`, `SetViewParameters`) — virtual
   Update loop + non-virtual methods in the same system
3. **`chat.cc`** (6 hooks: `AboutToShow`, `TabChanged`, etc.) — UI event /
   delegate paths
4. **`sync.cc`** (select 2-3 of ~13) — high-traffic direct-call paths. Test a
   subset before committing to the full file.

After Wave 1: evaluate results. Which strategies worked? Which hooks needed
inline fallback? Update the strategy map for remaining files.

**Wave 2 — Bulk migration** (apply proven strategy):

5. **`buff_fixes.cc`** (1 hook)
6. **`disable_banners.cc`** (2 hooks)
7. **`fix_pan.cc`** (2 hooks)
8. **`ui_scale.cc`** (2 hooks)
9. **`free_resize.cc`** (2 hooks)
10. **`misc.cc`** (6 hooks)
11. **`testing.cc`** (4 hooks)
12. **`hotkeys.cc`** (4 hooks)
13. **`object_tracker.cc`** (3 hooks)
14. **`sync.cc`** (remaining hooks)

After each file conversion: build, deploy, test in-game.

### Phase 3: Handle il2cpp_init (the 1 native hook)

- [ ] Evaluate restructured initialization (can we avoid hooking il2cpp_init?)
- [ ] If not, use MinHook/Detours (already available from Strategy B fallback)
- [ ] Remove SPUD dependency from `patches.cc`

### Phase 4: Remove SPUD

- [ ] Remove `spud` from xmake package dependencies
- [ ] Remove `xmake-packages/packages/s/spud/` directory
- [ ] Remove `#include <spud/detour.h>` from all files
- [ ] Update build configuration (remove `SPUD_NO_LTO` etc.)
- [ ] Test whether LTO can now be re-enabled (SPUD was the reason it was
  disabled)

## Risks & Open Questions

### Method Pointer Caching (HIGH — central risk)

**Risk**: IL2CPP-generated code can call method bodies directly by compiled
address. If the game's own code never reads `MethodInfo::methodPointer` at call
time for a given method, swapping it has no effect — the hook installs cleanly
and simply never fires.

This is not a minor edge case. It is the central architectural question of this
plan.

**Mitigation**: Phase 0 proof matrix. Test representative hooks empirically
before building infrastructure. If most hooks are reached via direct calls,
metadata swapping is not viable as the primary strategy.

### Virtual Dispatch Complexity (MEDIUM)

**Risk**: Virtual dispatch goes through `VirtualInvokeData::methodPtr` in the
vtable, which is a separate field from `MethodInfo::methodPointer`. Patching
one does not patch the other.

Additional edge cases:
- Interface dispatch resolves through its own path with offset computation
- Constrained/value-type calls can behave differently
- `virtualMethodPointer` may matter separately for generic virtual dispatch
- Direct non-virtual calls to a virtual method's body bypass vtable entirely

**Mitigation**: Phase 0 testing verifies actual dispatch paths. The hooking
backend supports multiple strategies so we can patch the right slot per hook.

### Generic / Shared Methods (MEDIUM)

**Risk**: With inline hooking, you hook the native body — catching all callers.
With pointer swapping, you hook a metadata slot — catching only callers that
go through that slot. Generic sharing can create multiple `MethodInfo*`s
pointing at the same native body, or share one body across type parameters.

**Mitigation**: Address census in Phase 0 detects fan-out. For shared methods,
we may need to patch multiple slots or fall back to inline hooking.

### Delegates and Cached Targets (MEDIUM)

**Risk**: If code grabs the function pointer before we swap it (into a delegate,
callback table, or local variable), later calls bypass our hook. Relevant for
UI events and engine-registered lifecycle callbacks.

**Mitigation**: Install timing testing in Phase 0. Our hooks install during
`il2cpp_init` processing (before most game code runs), which should be early
enough — but this needs verification, not assumption.

### DLL Unload Semantics (LOW)

**Risk**: If our DLL unloads before restoring method pointers, the game crashes
calling into unmapped memory.

**Mitigation**: Two cases:
- **Process exit**: Cleanup is less important — the process is dying anyway.
  No need for complex restoration logic.
- **Live unload**: Would need a proper shutdown path that quiesces features
  before restoring pointers. Currently we do not support live unload, so this
  is theoretical.

Do not assume `DllMain(DLL_PROCESS_DETACH)` is a safe place for complex
restoration. For process exit, the simple approach is to just let it go. For
live unload (if ever needed), a dedicated shutdown function called before
`FreeLibrary` is the right path.

### Performance (LOW — net positive)

Metadata pointer swapping eliminates trampoline indirection entirely. The
"original" function is called directly through the saved pointer — this is
**faster** than SPUD's trampoline path. The inline hook fallback (MinHook/
Detours) has comparable overhead to SPUD but with much better reliability.

### Fallback Library Choice (LOW)

For Strategy B (inline hook fallback) and Strategy C (native hook), we need a
small, reliable inline hooking library. Candidates:

- **[MinHook](https://github.com/TsudaKageworthy/minhook)** — single-file,
  MIT, widely tested, 2GB-proximate allocation, good x64 support
- **[Microsoft Detours](https://github.com/microsoft/Detours)** — Microsoft-
  maintained, MIT, battle-tested, slightly heavier dependency

Either works. MinHook is lighter. The choice is not load-bearing since it only
handles the cases metadata swapping cannot.

## Expected Outcome

Likely result:

- **Most IL2CPP hooks** → metadata/vtable pointer swapping (Strategy A)
- **A smaller subset** → inline hook fallback (Strategy B) where the live call
  path bypasses the metadata slot
- **il2cpp_init** → handled separately (Strategy C or restructured)
- **SPUD** → fully removed
- **Net architecture** → materially better stability, no TU-sensitivity for
  Strategy A hooks, proven fallback for the rest

This is a big win even if it does not cover 100% of hooks via metadata swapping.

## Success Criteria

- [ ] Phase 0 proof matrix completed — dispatch paths mapped for representative
  hooks
- [ ] All 57 hooks migrated from SPUD to the new hybrid backend
- [ ] Each hook's strategy (A/B/C) documented and justified
- [ ] Zero trampoline-related crashes from SPUD
- [ ] All existing functionality preserved (hotkeys, chat, zoom, sync, etc.)
- [ ] SPUD dependency fully removed
- [ ] Build with LTO re-enabled (if SPUD was the only blocker)
- [ ] TU-sensitivity eliminated for Strategy A hooks

## References

- [IL2CPP internals: MethodInfo struct](../third_party/libil2cpp/il2cpp-class-internals.h) (line ~320)
- [IL2CPP internals: VirtualInvokeData](../third_party/libil2cpp/il2cpp-class-internals.h) (line ~30)
- [IL2CPP codegen: virtual dispatch](../third_party/libil2cpp/codegen/il2cpp-codegen.h) (line ~717)
- [Current method resolution: IL2CppClassHelper::GetMethod](../mods/src/il2cpp/il2cpp_helper.h) (line ~166)
- [SPUD source (vendored)](../xmake-packages/packages/s/spud/spud-src/)
