# IL2CPP binding reference

The mod calls generated IL2CPP code through typed C++ bindings.
Bindings resolve members on first use, check metadata types, and cache the result for the process.
Runtime initialization must happen before resolution.

## Library boundaries

| Layer | Owns |
| --- | --- |
| `src/libs/il2cpp/` | Metadata, invocation, strings, boxing, allocation, GC handles, and detours. No engine or game dependency. |
| `src/libs/unity/` | UnityEngine, UI, TextMeshPro, input, and live MonoBehaviour tracking. No game dependency. |
| `src/mod/game/` | STFC class bindings, grouped by domain in headers. |

Feature files call bindings. They do not declare bindings or use raw `il2cpp_*` APIs.
A helper that works for any IL2CPP game belongs in `il2cpp`; one that works for any Unity game belongs in `unity`.
The IL2CPP library reports errors through `il2cpp::SetErrorHandler`, without depending on mod settings or logging.

## Binding shape

A binding is an empty struct derived from `il2cpp::Object`.
Its pointer is the managed object itself. `Class` names the assembly, namespace, and managed type.

```cpp
#include <il2cpp/il2cpp.h>

struct ShipView : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.Ships", "ShipView">;

  float Scale() { return Get<"Scale", float>(); }
  void SetScale(float scale) { Set<"Scale">(scale); }
  int32_t count() { return Field<"_count", int32_t>(); }
  void SetCount(int32_t count) { SetField<"_count">(count); }
  bool Select(int32_t index) { return Call<"Select", bool>(index); }
  bool TryPick(int32_t& slot) { return Call<"TryPick", bool(int32_t&)>(slot); }

  static ShipView* Current() { return Class::Get<"Current", ShipView*>(); }
};
```

Accessor names follow managed members. Fields are lowerCamel without a leading `_` or `m_` (`_count` is `count()`).
Properties and methods keep their PascalCase name. Setters use `SetX`.
A helper that converts the managed value takes the PascalCase name: `Name()` returns `std::string`, `name()` the raw
`Il2CppString*`.
Nested types use `"Outer/Inner"` as the class name.
Fixed-width numeric types keep the native ABI explicit.

| Managed class | Binding |
| --- | --- |
| MonoBehaviour | Derives from `unity::Behaviour`, which supplies `isActiveAndEnabled()`, `transform()` and `gameObject()`. |
| Other component | Derives from `unity::Component`. |
| Static members only | `struct X { using Class = il2cpp::Class<...>; static ... };` |
| Only ever hooked | `using X = il2cpp::Class<...>;` |
| Mirrored enum | `enum class X : int32_t` with the managed member names. Mod-owned enums use `kCamelCase`. |

## Members and calls

| Operation | API |
| --- | --- |
| Instance property | `Get<"Name", T>()`, `Set<"Name">(value)` |
| Instance field | `Field<"_name", T>()`, `SetField<"_name">(value)` |
| Instance method | `Call<"Name", R>(args...)` |
| Explicit signature | `Call<"Name", R(P...)>(args...)` |
| Static member | The same operation through `Class` |

`R` defaults to `void`.
Calls jump to native code with a trailing `MethodInfo*`. Virtual calls dispatch on the receiver.
Fields use metadata offsets for reads and runtime setters for writes, including GC write barriers.

Argument types select overloads.
`Foo::Class::TypeObject()` selects a `System.Type` overload rather than a string overload.
`std::ref(slot)` passes a `ref` or `out` argument.

An explicit `R(P...)` signature converts arguments to `P` and rejects narrowing conversions.
`Call<"Select", void(int32_t)>(3)` accepts an integer literal but rejects an `int64_t` variable.
The conversion check runs after forwarding, so narrow integers and floats need typed values such as `uint8_t{7}` and `1.0f`.
A `T&` parameter binds the caller's variable directly, without `std::ref`.

### Metadata checks

| C++ type | Required metadata match |
| --- | --- |
| Pointer with a bound class | Managed reference related to the declared class, including named bindings, `Il2CppString*`, and `Il2CppReflectionType*`. |
| Untyped pointer | Managed reference, `IntPtr`, `UIntPtr`, or unmanaged pointer |
| Integer or enum | Managed integer of the same size |
| `bool`, `float`, `double`, or struct | Matching kind and size |

A missing or drifted class or member is reported once.
Calls then do nothing and return a value-initialized result.
A managed exception is contained at the binding call, reported once per method, and produces the same default result.
It cannot unwind through the hook and skip the game's code.

When a decision needs to distinguish a real zero/null from an unavailable observation, use
`TryCall<"Method", R(P...)>(args...)` on an Object or Class. Its `CallResult<R>` has `succeeded` and `value`
(void has only `succeeded`). Resolution, virtual dispatch and exception reporting are shared with Call; no
exception and a valid zero, false, null reference or void result means success. Missing/drifted members, null
receivers, contained exceptions and an absent numeric box mean failure. Fully shared generic calls preserve the
runtime exception slot too. This reports whether invocation succeeded, not whether game-specific values are valid;
an individual ship ID must still be a positive BaseShip instance ID, never a fleet/hull ID or a guessed zero.

Receivers must be non-null. Check pointers returned by the game before calling methods through them.

### Runtime classes and collections

A binding without `Class` resolves against each receiver's runtime class, cached per class and thread.
This supports generic bases such as `Widget<T>`, `MonoSingleton<T>`, and `il2cpp::List<T>`.
Some game bindings use this form when their managed class has moved between client builds.
They still belong in `src/mod/game/`.

`Object::HasMethod<F>(name)` checks a method on such a runtime class without calling it.
`CallOnField<"field", T, "Method", R>()` calls a parameterless method on a value-type field in place.

`il2cpp::List<T>` supports indexed reads and `Set`, `Add`, `RemoveAt`, and `Clear`.
Its iterator reads `Count` once. Do not mutate the list during iteration.
`il2cpp::Array<T>` views a managed array; its `Set` applies reference write barriers.

### Unity internal calls

Unity bindings use function-local static `il2cpp::ICall<Signature>` objects for engine internal calls.
For example, `unity::Input` binds `UnityEngine.Input::GetKeyInt(UnityEngine.KeyCode)`.
Resolution happens on first use. Missing calls are reported once and return value-initialized results.
`operator bool` tests availability when a different fallback is needed.

## Hooks

Every hook installs through an `il2cpp::HookSet`, one per installer. Stage each hook with `IL2CPP_STAGE(set, method,
hook)`, or `IL2CPP_STAGE_AT(set, address, hook)` for a native function such as an IL2CPP export, then call `Commit()`.
Hook names follow `Class_Method_Hook`. The first parameter is `auto original`; the receiver is `_this`.

```cpp
#include <il2cpp/hook.h>

void ShipView_Select_Hook(auto original, ShipView* _this, int32_t index)
{
  original(_this, index);
}

bool InstallShipView()
{
  il2cpp::HookSet hooks("ShipView");
  IL2CPP_STAGE(hooks, ShipView::Class::Method("Select", 1), ShipView_Select_Hook);
  return hooks.Commit();
}
```

A set installs through one SPUD transaction. A missing method or null address marks it incomplete and installs
nothing. SPUD attempts reverse rollback after an installation failure, but rollback/protection/suspension failures
and exceptions can leave native state indeterminate. Do not interpret every failure as proof that no hook remains.

`CommitDetailed()` returns a cached `HookInstallResult`: installed, missing target, empty, rejected or indeterminate.
The SPUD error is retained separately; `kOk` can mean no SPUD code was observed, so inspect the state. Repeated commits
return the historical outcome without retrying; further staging is an error. The compatible bool `Commit()` returns
success/refusal, and throws `HookInstallError` when native state is unsafe, including a later unsafe process latch.

Indeterminate state latches the process unsafe before reporting or cleanup. Owned handles are quarantined intact;
no subsequent set may apply hooks. Startup must terminate rather than continue into the game. This avoids further
cleanup and deliberate continuation; it cannot repair vendor-side corruption or undo threads already resumed.
SPUD itself can release failed-install resources before returning an error. These guards are not a native rollback
guarantee. Unknown future vendor errors remain indeterminate until qualified.

Ordinary refusal is reported as `[IL2CPP] <set> not installed: <why>`; unsafe state is reported as
`[IL2CPP] <set> hook state indeterminate: <why>`. Thread-local detail is labeled post-transaction because rollback can
overwrite the original failure detail. Features do not duplicate hook reports. Committed removable hooks are
detached and remain for the process. A shared capability has one terminal installer owner, not a display-name map;
intentional chains on one target remain supported.

SPUD chains detours on one function safely, so two hooks may target the same native code. Identical methods can share
generated code, though, and a hook on one of them then also runs for the others, with their receiver. Where a feature
knows its targets can share a body, it compares the addresses and hooks the shared body once.

| Lookup | Selects by |
| --- | --- |
| `Class::Method("Select")` | Name, any arity |
| `Class::Method("Select", 1)` | Name and parameter count |
| `Class::Method("Select", {"System.Int32"})` | Managed parameter type names |
| `Class::Method<void(int32_t)>("Select")` | Complete instance signature, without `this` |
| `Class::StaticMethod<void(int32_t)>("Select")` | Complete static signature, without running the class constructor |

Name-based lookups fall back to a base class's method when the class itself does not declare one.
Hooking that detours the base for every subclass; check `Method::Info()->klass` when that matters.
`Class::HasField<"_name", T>()` and `Class::HasProperty<"Name", T>()` check a member without reading an object.
Reserve them for a hook that would act on a value read as zero; a missing member is already reported.

Prefer one detour per game method, then route feature work through it.

Before detouring a small wrapper or thunk, verify its native extent against SPUD's overwrite size.
Check every supported architecture for the exact client build.
On macOS, `il2cpp::CheckedEntry(method)` measures the function from the image's function starts and returns the address
to detour, or null after a report. Stage it with `IL2CPP_STAGE_AT`.
A successful build does not prove the hook fits. Prefer a substantive downstream method when exact extents are unavailable.
During drift repair, match the game-visible signature in `dump.cs`.
Change hidden `MethodInfo*` handling only when runtime evidence points to it.

### Shared frame and capability readiness

`src/mod/frame.h` exposes the one `ScreenManager.Update` owner. `EnsureFrameTick()` is a bootstrap/game-thread-only,
terminal install request; `FrameTickInstallation()` reads its result without retrying. `RegisterFrameCallback()`
accepts process-lifetime callbacks and owns their diagnostic names. Registration is not proof that the native anchor
installed. Duplicate callbacks are harmless; null and during-dispatch registrations are rejected. Recursive dispatch
is suppressed, and callback/reporting exceptions do not skip later callbacks or unwind into the host frame.
`FrameDeliveryAvailable()` also requires host initialization to be complete. An early anchor request during feature
installation leaves input polling, reload, subscribers and shortcuts dormant; the game's original Update continues.

`FeatureInstallations()` reports capability installation, callback registration and frame readiness separately.
Only successfully installed features register their frame work; reload/input/shortcut ordering remains at the
single host anchor. `InstallGuard` prevents repeated/reentrant initialization, including side effects outside a SPUD
transaction. Optional refusal can leave independent features available; required failure or any indeterminate
native state stops startup. Profiles' required three-phase bootstrap is a later integration, outside the optional
feature table. No profile isolation capability is enabled or claimed by these interfaces.

## Objects and lifetimes

| API | Behavior |
| --- | --- |
| `il2cpp::New<T>(args...)` | Allocate and run the matching constructor |
| `il2cpp::Allocate<T>()` | Allocate without a constructor, for classes whose parameterless constructor has no generated code |
| `NewString`, `ToUtf8`, `Equals` | Create, convert, or compare managed strings |
| `ToString` | A managed string for any object |
| `Box`, `Unbox` | Box a primitive, or read one of exactly that type |
| `Nullable<T>` | `System.Nullable<T>` by value |
| `Is<T>`, `As<T>` | Test or cast a receiver against a bound class |
| `InvokeBound` | Run a single-cast delegate's method directly; managed exceptions propagate so the caller can fall back |
| `NewDelegate` | Bind a delegate to a static method, with an optional native redirect |
| `Guarded` | Contain Windows access violations when reading layouts that may have drifted |

The GC scans native stacks, so managed pointers in local variables remain visible to it.
Pointers stored in C++ statics or heap objects need `GcHandle::Strong` roots.
Release roots before runtime shutdown. Roots do not destroy Unity native resources.

`unity::Track<T>()` tracks a class and its subclasses from hook installation onward.
It cannot discover instances created earlier.
`unity::Newest<T>()` returns the newest tracked instance.
`unity::Instances<T>()` owns a snapshot of borrowed pointers in creation order.
Use those pointers on the game thread within the current frame.

The raw SDK is available to the libraries through `<il2cpp/api.h>`.
On macOS, `il2cpp::Initialize` resolves exports from the loaded GameAssembly image.
Windows uses the checked-in import library.

See [contributor checks](../CONTRIBUTING.md#run-checks-before-submitting) for tests and platform builds,
and [binding repair](../CONTRIBUTING.md#repair-bindings-after-a-game-update) for dump comparison commands.
