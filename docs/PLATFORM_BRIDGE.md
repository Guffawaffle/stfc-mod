# Platform Bridge Boundaries

The mod still has platform-specific behavior because it runs inside the game process and must call OS APIs directly. The goal is to keep those calls behind small adapters instead of hiding runtime constraints behind broad abstractions.

## Notification Delivery

- `mods/src/patches/notification_platform.h` is the OS notification adapter.
- Windows uses WinRT toast notifications in `notification_platform_show`.
- Tests and non-runtime checks can replace delivery with `notification_platform_set_delivery_override_for_testing`.
- Non-Windows notification delivery is currently a no-op. The service logs platform unsupported state rather than pretending delivery exists.

## Window Title

- `mods/src/windowtitle.h` is the platform-neutral interface.
- Windows implementation lives in `mods/src/titlewindows.h` and uses `Config::WindowHandle()` plus Win32 title APIs.
- Linux implementation lives in `mods/src/titlelinux.h` and uses X11 root-window title behavior.
- macOS is intentionally stubbed today. That is a known platform gap, not an adapter failure.

## Paths And Command Line

- `mods/src/platform_bridge.h` owns command-line option parsing and mod storage path resolution.
- Windows command-line parsing uses `GetCommandLineW` / `CommandLineToArgvW` and supports `-debug`, `-trace`, and `-ccm <profile>`.
- Windows storage paths remain process-relative filenames, preserving existing deployment behavior.
- Non-Windows storage paths route through the Library/Preferences folder manager and retain the legacy bundle-id path option for migration.

## Intentional Gaps

- macOS window-title support is not implemented.
- Non-Windows notification delivery is not implemented.
- Linux window-title support is legacy X11-oriented and has not been validated against Wayland.
- The platform bridge does not abstract IL2CPP pointer lifetime or hook safety; those remain explicit at hook boundaries.
