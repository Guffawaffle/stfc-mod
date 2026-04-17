# SPUD → MinHook Migration Guide

Quick reference for the hooking API change. Every pattern has a 1:1 mapping.

---

## 1. Simple hooks (single target)

**Before (SPUD):**
```cpp
#include <spud/detour.h>

void MyHook(auto original, Foo* _this, int x)
{
  // ... pre-hook logic ...
  original(_this, x);
  // ... post-hook logic ...
}

// Installation:
SPUD_STATIC_DETOUR(method_ptr, MyHook);
```

**After (MinHook):**
```cpp
#include "hook/detour.h"

MH_HOOK(void, MyHook, Foo* _this, int x)
{
  // ... pre-hook logic ...
  MyHook_original(_this, x);     // <── named trampoline, not `original`
  // ... post-hook logic ...
}

// Installation:
MH_ATTACH(method_ptr, MyHook);
```

### What changed

| Aspect | SPUD | MinHook |
|--------|------|---------|
| **Header** | `<spud/detour.h>` | `"hook/detour.h"` |
| **Hook signature** | `void Hook(auto original, ...)` | `MH_HOOK(void, Hook, ...)` |
| **Calling original** | `original(args...)` | `Hook_original(args...)` |
| **Return type** | Inferred from `original` | Explicit first arg to `MH_HOOK` |
| **Installation** | `SPUD_STATIC_DETOUR(ptr, Hook)` | `MH_ATTACH(ptr, Hook)` |
| **No-arg hooks** | Same as above | `MH_HOOK_NOARGS(ret, Hook)` |

### Return values

SPUD inferred the return type. MinHook needs it spelled out:

```cpp
// SPUD — return type implicit
void* GetFoo(auto original, void* _this) {
  return original(_this);
}

// MinHook — return type explicit
MH_HOOK(void*, GetFoo, void* _this) {
  return GetFoo_original(_this);
}
```

### Static (classless) methods

If a hooked method has no `_this` parameter, the hook just omits it. Nothing
special — `MH_HOOK` doesn't assume the first param is a `this` pointer:

```cpp
MH_HOOK(int, ExtractBuffsOfType, int buffType, void* buffList)
{
  return ExtractBuffsOfType_original(buffType, buffList);
}
```

---

## 2. Initialization

MinHook requires one-time init before any hooks are installed, and cleanup
at shutdown. This lives in `patches.cc`:

```cpp
#include "hook/detour.h"

void ApplyPatches()
{
  detour::init();        // Must be called BEFORE any MH_ATTACH
  // ... install all hooks ...
}

// On DLL unload:
detour::shutdown();      // MH_DisableHook(MH_ALL_HOOKS) + MH_Uninitialize()
```

SPUD had no global init/shutdown — each detour was self-contained.

---

## 3. Multi-target hooks (same logic, N different targets)

This is the only non-trivial pattern. When the **same hook function** is
installed on **multiple different method pointers**, each installation needs
its own `original` trampoline. SPUD handled this implicitly (each
`SPUD_STATIC_DETOUR` was an independent detour). With MinHook, we use
**template slots**.

### The problem

```cpp
// BAD — both installations write to the SAME MyHook_original pointer.
// The second MH_ATTACH overwrites the first's trampoline.
MH_HOOK(void, MyHook, void* _this) { MyHook_original(_this); }

MH_ATTACH(target_A, MyHook);   // MyHook_original → trampoline for A
MH_ATTACH(target_B, MyHook);   // MyHook_original → trampoline for B (A's is lost!)
```

### The fix: template slots

Each slot is a separate type with its own static `original` pointer:

```cpp
// 1. Define the function pointer type
using MyHook_fn = void(*)(void*);

// 2. Define the template
template<int Slot>
struct MyHook {
  static inline MyHook_fn original = nullptr;
  static void detour(void* _this)
  {
    // ... shared hook logic ...
    original(_this);
  }
};

// 3. Install with MH_ATTACH_SLOT — each target gets a unique slot number
MH_ATTACH_SLOT(target_A, MyHook, 0);
MH_ATTACH_SLOT(target_B, MyHook, 1);
MH_ATTACH_SLOT(target_C, MyHook, 2);
```

Each `MyHook<0>`, `MyHook<1>`, `MyHook<2>` is a distinct type. Their
`original` pointers are independent. The slot numbers must be unique per
hook type — reusing a slot is a fatal error caught at startup (the
`MH_ATTACH_SLOT` macro aborts if `original != nullptr`).

### Real example: sync.cc

SPUD installed `DataContainer_ParseBinaryObject` on 9 different
`DataContainer` subclasses:

```cpp
// BEFORE (SPUD) — each call was an independent detour
SPUD_STATIC_DETOUR(alliance_ptr,   DataContainer_ParseBinaryObject);
SPUD_STATIC_DETOUR(starbase_ptr,   DataContainer_ParseBinaryObject);
SPUD_STATIC_DETOUR(officer_ptr,    DataContainer_ParseBinaryObject);
// ... 6 more
```

```cpp
// AFTER (MinHook) — template slots
using ParseBinaryObject_fn = void(*)(void*, EntityGroup*, bool);

template<int Slot>
struct ParseBinaryObjectHook {
  static inline ParseBinaryObject_fn original = nullptr;
  static void detour(void* _this, EntityGroup* group, bool isPlayerData)
  {
    HandleEntityGroup(group);
    return original(_this, group, isPlayerData);
  }
};

MH_ATTACH_SLOT(alliance_ptr,  ParseBinaryObjectHook, 0);
MH_ATTACH_SLOT(starbase_ptr,  ParseBinaryObjectHook, 1);
MH_ATTACH_SLOT(officer_ptr,   ParseBinaryObjectHook, 2);
// ... slots 3–8
```

### Dynamic slot assignment: object_tracker.cc

When the number of unique targets isn't known at compile time (e.g.,
multiple types may share a base `.ctor`), we use a **runtime dispatch table**:

```cpp
template<int Slot>
struct TrackCtorHook {
  static inline track_ctor_fn original = nullptr;
  static void* detour(void* _this) { /* ... */ return original(_this); }
};

// Pre-instantiate slots 0–15 into a dispatch table
using install_fn = bool(*)(void*);
static install_fn ctor_installers[] = {
  &CtorInstaller<0>::install, &CtorInstaller<1>::install, /* ... */
};

// At runtime, assign the next available slot to each unique method
static int next_ctor_slot = 0;
if (seen_ctor.find(ctor) == seen_ctor.end()) {
  ctor_installers[next_ctor_slot++](ctor);
  seen_ctor.emplace(ctor);
}
```

This handles the case where 13 tracked types may or may not share base
class methods — each unique method gets its own slot, shared methods
are deduplicated by the `seen_ctor` set.

---

## 4. Signature scanning

**Before (SPUD):**
```cpp
#include <spud/signature.h>
auto addr = spud::find_in_module("GameAssembly", "48 89 5C 24 ...");
```

**After:**
```cpp
#include "hook/signature.h"
auto addr = sig::find_in_module("GameAssembly", "48 89 5C 24 ...");
```

Drop-in replacement. Same byte-pattern format (`??` for wildcards).

---

## 5. Architecture checks

**Before:** `SPUD_ARCH_ARM64`
**After:** `defined(__aarch64__) || defined(_M_ARM64)`

---

## Macro reference

| Macro | Purpose |
|-------|---------|
| `MH_HOOK(ret, name, ...)` | Declare hook function + `name_original` trampoline pointer |
| `MH_HOOK_NOARGS(ret, name)` | Same, for zero-parameter functions |
| `MH_ATTACH(target, name)` | Install hook on `target`, store trampoline in `name_original` |
| `MH_ATTACH_SLOT(target, Type, N)` | Install template hook `Type<N>::detour`, store in `Type<N>::original` |
| `detour::init()` | Call once before any hooks |
| `detour::shutdown()` | Disable all hooks + uninitialize MinHook |

---

## Files changed

| File | What changed |
|------|-------------|
| `mods/src/hook/detour.h` | **New** — MinHook wrapper macros + init/shutdown |
| `mods/src/hook/signature.h` | **New** — Standalone byte-pattern scanner |
| `mods/src/patches/patches.cc` | Added `detour::init()` call, switched includes |
| `mods/src/patches/parts/sync.cc` | Template slots for 3 multi-target hooks (13 total slots) |
| `mods/src/patches/parts/object_tracker.cc` | Runtime dispatch tables for ctor/destroy hooks |
| All other `parts/*.cc` (11 files) | Mechanical `MH_HOOK`/`MH_ATTACH` transformation |
| `xmake.lua` | `minhook` replaces `spud` |
| `mods/xmake.lua` | Removed `capstone` package |
| `xmake-packages/packages/s/spud/` | **Deleted** — vendored SPUD source |
