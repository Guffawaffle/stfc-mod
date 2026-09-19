# macOS feature test

This branch enables the native Mod Settings adapter, OPC fleet indicators, and
desktop notification delivery on macOS. It is a test candidate, not a claim of
in-game validation. Keep the previous working mod available for rollback.

## Settings

Open the game's Settings and look for **Mod Settings**. Open and close each
available page, change a harmless setting such as a fleet label, and verify it
survives restarting the game. Check the native settings still work. Pages only
show controls whose backing feature is available; this does not enable the
Windows keyboard shortcut editor or every other Windows-only feature.

The adapter verifies hook entries against the loaded client's Mach-O function
boundaries and instruction prologues. If a target cannot be verified, the
affected adapter stays disabled. Logs contain `[MacHookExtent]` and
`[ModSettings]` evidence; successful compilation alone is not hook validation.

## OPC and desktop notifications

Merge these keys into the existing sections of your config (do not duplicate
the section headers). Restart after changing them:

```toml
[ui]
highlight_opc_fleets = true
fleet_hud_opc_eta = true
notify_fleet_events = "MinerOPC"

[audio]
alert_fleet_miner_opc = "warning"
```

Allow notifications when macOS asks. Notifications use the **game's application
identity**, not the mod launcher's. Check its entry in System Settings →
Notifications if permission was denied. Focus/Do Not Disturb and the game's
existing notification delegate can suppress banners. Test desktop delivery with
the game in the background; this mod does not replace the game's delegate to
force foreground banners. Events while permission is pending are not replayed.
The desktop banner adds no sound; the audio setting independently selects it.

Start mining below protected cargo and cross the threshold. Verify the OPC ETA,
amber fleet portrait, one configured sound, and one desktop notification. Check
that login while already OPC is quiet, and that returning below the threshold
allows a later crossing to notify again. Switch fleets and open/close fleet
panels to check indicator cleanup. Repeat with either indicator disabled.

`notify_banner_types` also supports desktop delivery for configured native game
banners, using the existing `toastbannerhooks` setting. Empty notification lists
and `"none"` audio values keep alerts off; no permission prompt is requested
when desktop notifications are unconfigured.

Before merging, record the tested mod commit, game version, macOS version and
CPU architecture, plus startup logs and results for these checks. ARM64 and
x86_64 builds and hook-boundary tests run separately in CI.
