# Prime Wrapper And Generated Constant Audit

This audit covers the hand-maintained `mods/src/prime/*.h` wrappers and game-derived constants used by hooks and feature code.

## Direct Include Fixes

The following wrappers called `ErrorMsg::...` without directly including `errormsg.h`; they now include it explicitly:

- `mods/src/prime/BundleDataWidget.h`
- `mods/src/prime/CanvasController.h`
- `mods/src/prime/CanvasScaler.h`
- `mods/src/prime/DeploymentManager.h`
- `mods/src/prime/FleetBarViewController.h`
- `mods/src/prime/FleetLocalViewController.h`
- `mods/src/prime/FleetsManager.h`
- `mods/src/prime/Hub.h`
- `mods/src/prime/SpecManager.h`

Additional direct include fixes:

- `mods/src/prime/ActionData.h` now includes `<cstdint>` for `int32_t`.
- `mods/src/prime/SpecManager.h` now includes `<cstdint>` for `int64_t`.

Audit command used for the `ErrorMsg` pass:

```powershell
Get-ChildItem mods/src/prime/*.h |
  Where-Object { (Select-String -Path $_.FullName -SimpleMatch 'ErrorMsg::' -Quiet) -and
                 -not (Select-String -Path $_.FullName -Pattern 'errormsg\.h' -Quiet) } |
  ForEach-Object { $_.Name }
```

## Generated Constants

`SectionID` values in `mods/src/prime/Hub.h` are sourced from the IL2CPP dump:

- Dump file: `tools/il2cpp-dump/dump.cs`
- Source declaration: `Namespace: Digit.Client.Sections`, `public enum SectionID`, `TypeDefIndex: 1641`
- Refresh command: `pwsh -NoProfile -File tools/dump-il2cpp.ps1`

`ActionType` values in `mods/src/prime/ActionData.h` are game-derived action identifiers used by request/action wrappers. The current `dump.cs` contains many `ActionType` usages, but this audit did not find a standalone `public enum ActionType` declaration in the dump. Treat these values as manually curated from runtime/dump investigation until a stronger generated source is added.

## Regeneration And Check Workflow

1. Refresh the IL2CPP dump against the installed game with `pwsh -NoProfile -File tools/dump-il2cpp.ps1`.
2. Diff `tools/il2cpp-dump/dump.cs` for wrapper source types, field names, method signatures, enum values, and type indexes.
3. Update `mods/src/prime/*.h` wrappers with direct includes for every type or helper used in the header.
4. For generated constants, include the source namespace/type in comments or this audit doc when the dump provides it.
5. Build with `xmake build mods`.
6. Run `pwsh -NoProfile -File .ax/ax.ps1 cycle` for deploy/boot/hook health before closing wrapper maintenance work.

## Known Follow-Ups

- Add an automated direct-include check when a reliable compiler invocation for one-header translation units is available in AX.
- Add a stronger generation path for `ActionType` constants if the source enum can be recovered from a newer dump or metadata index.
