# macOS feature test

This branch enables the native Mod Settings adapter, OPC fleet indicators, fleet and galaxy
label controls, galaxy overlay composition, and desktop notification delivery on macOS. It is a test candidate, not a claim of
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

Changes save automatically; the game's Save button is not required. Confirm the
startup log says `Runtime config persistence ready=true`. Change the galaxy
threshold to 95%, close and reopen Mod Settings, then quit normally and relaunch.
Verify both the selected profile and 95% survive. Also test quitting immediately
after moving the slider. If saving fails, the page should show the amber
“Active this session; couldn't save” notice and the log should explain why.

## Fleet and galaxy labels

Start with the default Native profiles. In Mod Settings, change player and
non-player fleet labels independently to Expanded, Compact, and Threshold.
Zoom in and out across the threshold, switch systems, and recall/redeploy a
fleet to exercise pooled widgets. Return both profiles to Native and verify
normal game labels resume. Changes should persist after restarting.

In the galaxy, test major and minor system labels independently with Always
and Threshold, then restore Native. Enable multi-select and combine Default,
Mining, Hostiles, and Hazards. Check labels and icons while zooming, panning,
and entering/leaving a system. Disable multi-select and confirm the game's
single-overlay selection and layout resume. Restart to check persistence.
Defaults remain Native with multi-select off.

These controls appear only when their hook family validates successfully.
Capture `[GalaxyLabels]`, `Fleet label detail hooks`, and `[MacHookExtent]`
startup messages if either family is missing. Fleet and galaxy reuse a single
LOD hook; failure of a fleet-only binding should not disable galaxy controls.

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
identity**, not the mod launcher's. Check its entry in System Settings â†’
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
