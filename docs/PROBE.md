# IL2CPP Runtime Probe

`probe.h` is a header-only toolkit for exploring game classes at runtime. It
dumps methods, fields, properties, events, hierarchy, and interfaces for any
IL2CPP class — no offline dumps or pre-built headers required.

All output goes to spdlog (tagged `[probe]`), so check `community_patch.log`.

---

## Include

```cpp
#include "probe/probe.h"
```

No additional link dependencies. It uses the IL2CPP API functions already
available through `il2cpp_helper.h`.

---

## Quick Start

Drop a one-shot guard into any hook or init function:

```cpp
static void investigate_once()
{
  static bool done = false;
  if (done) return;
  done = true;

  // Full dump: hierarchy, fields, properties, methods, events
  probe::dump_class("Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController");

  // Search every class in the assembly for methods matching a substring
  probe::search_methods("Assembly-CSharp", "ShowShipPanel", 10);
}
```

Call it from your hook:

```cpp
MH_HOOK(void, SomeHook, Foo* _this)
{
  investigate_once();
  original(_this);
}
```

Build, deploy, launch the game, trigger the hook, then read the log.

---

## API Reference

All functions live in the `probe::` namespace. Every function takes
`(assembly, namespace, class)` strings matching the IL2CPP metadata.

| Function | What it dumps |
|---|---|
| `dump_class(asm, ns, cls)` | Everything below, all at once |
| `dump_hierarchy(asm, ns, cls)` | Parent chain + implemented interfaces + instance size |
| `dump_fields(asm, ns, cls)` | Field name, type, byte offset |
| `dump_properties(asm, ns, cls)` | Property name, type, get/set accessors |
| `dump_methods(asm, ns, cls)` | Full signature + method pointer address |
| `dump_events(asm, ns, cls)` | Event names |
| `dump_namespace(asm, ns)` | All classes in a namespace (with method counts) |
| `search_methods(asm, needle, max)` | Cross-class substring search on method names |

### Typical assembly name

Almost everything in this game is in `"Assembly-CSharp"`.

### Finding the right namespace

If you don't know the namespace, use `dump_namespace` or `search_methods`:

```cpp
// List everything in a namespace
probe::dump_namespace("Assembly-CSharp", "Digit.Prime.Ships");

// Or search for a method name across ALL classes
probe::search_methods("Assembly-CSharp", "RequestSelect", 20);
```

---

## Reading the Output

Methods include the runtime function pointer, which you can use directly with
MinHook if you need to hook something you just discovered:

```
[probe]    13. System.Void RequestSelect(System.Int32 index, System.Boolean simulated) @ 0x7ffd8e70afb0
```

Fields include byte offsets from the object base, matching what you'd put in a
header struct:

```
[probe]     3. Digit.Prime.Ships.FleetLocalViewController _fleetPanelController (offset: 0x50)
```

---

## Tips

- **One-shot guard**: Always use a `static bool` guard. These dumps iterate
  every class/method in the assembly and are not cheap.
- **Don't ship diagnostics**: Remove or `#if 0` your probe calls before
  committing. The probe header itself stays in the tree as a permanent tool.
- **Namespace guessing**: Game classes sometimes live in unexpected namespaces.
  If `dump_class` warns "class not found", try `search_methods` with part of
  the class name to find where it actually lives.
- **Method pointer addresses**: These are only valid for the current game
  version. They'll change with every game update.
