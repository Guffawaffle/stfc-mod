# Startup frame-counter preflight correction

## Observed failure

The first native migration trial failed during startup. The last completed
feature log was `Patching 20 of 48 (PinnedShips)`. That message is written after
installation; the following feature is `PinnedShipClick`, not `PinnedShips`.

The coordinator symbolized the actual installed DLL with its matching PDB. The
reported stack identified `PinInputEpoch` in `ship_pin_input.cc:53`, inlined
through `ship_pin_input::Install:544`, called by `InstallPinnedShipClick:485`.
Those are locations in the failing candidate, before this correction. The trial
was source `43d6c7b732be72b0d0ed225adb733249121360e2`, with installed DLL
SHA-256 `3d98199ecfcbecf115b2137397e2aafd80e4a57d964c1bf34ae4e139e7420be2`.
The private evidence directory is
`D:/dev/stfc-workspace/.ax/ripper-startup-fix-20261025/`: retained
`failed-43d6-community_patch.log`, `failed-43d6-Player.log` and
`failed-43d6-crash.dmp` preserve the original failed trial. Symbolization uses
the matching installed artifact and PDB, not a later rebuilt DLL.

Installation had called `PinInputEpoch()` to test availability. That function
executed Unity's native `Time.get_frameCount()` internal call while installing
hooks from `il2cpp_init_Hook`. Metadata availability does not establish that
Unity's frame state is safe to read at that startup boundary. A successful
offline compile and controlled binding test did not qualify that engine
lifecycle assumption.

## Bounded correction

- [Unity Time](../../src/libs/unity/include/unity/core.h) exposes
  `FrameCountAvailable()`, which resolves the existing internal call and checks
  its presence without invoking the engine. It shares the same cached resolver
  with the existing runtime `frameCount()` read.
- [Pin-input installation](../../src/mod/features/ship_pin_input.cc) uses that
  presence-only check. Installation no longer requests a live input epoch.
- Runtime pin interactions continue to require their actual nonzero epoch.
  Missing frame-counter support remains unavailable; no zero or invented epoch
  is admitted. The existing duplicate/reentrant press and release protections
  are unchanged.

No hook target, feature default, persistence path, native dispatch ABI, or general
IL2CPP invocation policy changes. Resolving the internal call is an availability
test, not a claim that the engine is ready for an immediate invocation.

## Regression and qualification boundary

The existing [controlled-runtime fixture](../../tests/il2cpp_bindings.cc) now
counts native frame-counter invocations. Repeated `FrameCountAvailable()` calls
must leave that count unchanged; an explicit runtime `frameCount()` read must
invoke the shared entry point once and preserve the observed value. Its success
marker is:

`Unity frame-count startup preflight resolves without engine invocation passed`

The [pointer-policy fixture](../../tests/pin_pointer_policy_test.cc) separately
retains coverage that epoch zero is unavailable and that one release cannot
toggle a pin twice. This pure policy does not execute Unity.

Formatting and `git diff --check` passed. The focused Windows
`mods + test-il2cpp + test-ship-pins` run completed **75/75 build steps**,
including the new zero-engine-invocation marker and the existing pin/order,
receipt, pointer-policy and checked-list fixtures. This was a local correction
gate over the current source, not a new clean-candidate full-platform gate.
`build.log` in the evidence directory above has SHA-256
`4d3c0fd256ed2255aa68794b81acd8947262c92bed1617f18dd821f73352fa9a`;
`source-fix.diff` records the correction. Final DLL linking separately completed
**40/40 steps**, recorded in `link.log`. Static verification passed the expected
18 exports, Profiles-host closure, static CRT and provenance checks. No Linux or
macOS qualification is added here.

## Corrected native startup

The coordinator deployed and launched this exact local correction:

- DLL SHA-256:
  `e416cda26e6e785b1f37260d97596d30517195fbe1ef4c8f4c12c3c3178eb99e`.
- Length: `10957824` bytes.
- Embedded source identity: `local:3e8a04cf:startup-framecount-b338d9d1`.

Deployment used the existing AXF prebuilt flow to the selected ordinary game
installation. The ordinary session launched as PID `66824` on 2026-10-25 at
`14:02:40`. At `14:02:47.500`, the native log had completed **all 48 feature
groups** and reported **FrameTick ready**. The coordinator verified that the
process remained alive and responding for approximately 90 seconds, with no new
crash observed during that window. These are coordinator runtime observations;
the local source identity above is not a claim that an earlier clean commit
already contained the correction.

**Corrected startup passed.** Real-frame pin selection, double-clicking and
dragging still require human gameplay qualification. This startup result does
not establish full pin behavior, every gameplay cohort, macOS behavior, or
long-session stability. Preserve the initial crash as failed runtime evidence;
the successful correction does not retroactively qualify the failing binary.
