# Extended system selection

Allows selecting minor systems at far galaxy zoom while keeping native label and
full-entity culling behavior. Disabled by default:

```toml
[graphics]
galaxy_extended_selection = false
```

On builds with the in-game Mod Settings page, use Galaxy Labels > Extended system
selection. That toggle applies live and persists. Otherwise edit the TOML and
restart. Availability depends on resolving required bindings and installing the hooks.

Windows and macOS resolve the tap handlers by managed name and signature.
macOS ARM64 and Intel also verify the GalaxyNode value layout before hooking.
The Mac adapter preserves the native tap handler and only forwards a far-galaxy
tap when the native handler did not dispatch it. Unsupported bindings fail closed.
Mac compilation/CI and player runtime validation are pending for this branch;
Windows runtime evidence for the upstream extraction does not establish Mac parity.

The implementation creates only click-local POIs, keeps identities immutable, and
retains at most 64 entries. Old unreferenced entries can be retired; active preview
references and recent clicks are protected. More than 16 candidate systems within
the click prefilter, or no safely retireable entry, falls back for that click.
These bounded fallbacks do not bypass native selection or system-access rules.

The upstream TOML-only change remains separate in the public checkout. This
branch is based on downstream play and restores the settings/persistence adapter.
The superseded local feature/galaxy-extended-selection branch was at 8e2f941e;
experiment/galaxy-selection-zoom preserves the science and benchmark history.
