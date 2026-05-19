# ⚠️ UNOFFICIAL FORK — Guffawaffle's STFC Community Mod

> **This is NOT the official STFC Community Mod.**
> This is a personal fork with experimental features. The official project is **[netniV/stfc-mod](https://github.com/netniV/stfc-mod)**.

<p align="center">
    <a href="https://github.com/netniV/stfc-mod"><img src="https://img.shields.io/badge/Official_Mod-netniV%2Fstfc--mod-blue?style=for-the-badge" alt="Official Mod"></a>
    <a href="https://github.com/sponsors/netniV"><img src="https://img.shields.io/badge/Sponsor-netniV-ea4aaa?style=for-the-badge&logo=github-sponsors" alt="Sponsor netniV"></a>
    <a href="https://img.shields.io/badge/License-GPLv3-blue.svg"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPLv3"></a>
</p>

<p align="center">
    <b>All credit goes to <a href="https://github.com/netniV">netniV</a></b> for creating and maintaining the STFC Community Mod.<br>
    This fork exists purely for personal experimentation. If you want to support the project,<br>
    <b><a href="https://github.com/sponsors/netniV">sponsor netniV</a></b> — he built this.
</p>

---

## Read This First

> **Short version**
> If you want the standard, official STFC Community Mod experience, use **[netniV/stfc-mod](https://github.com/netniV/stfc-mod/releases/latest)**.
> If you want this fork's validated public builds, use this fork's **`main`** releases.
> If you want the in-progress integration branch where new fork work lands first, use **`guffa-dev`**.

This fork exists so I can ship and test changes on my own branch without presenting them as official upstream behavior.

- Use **`netniV/stfc-mod` `main` releases** if you want the official/default mod experience.
- Use **`Guffawaffle/stfc-mod` `main` releases** if you want the current published state of this fork.
- Use **`Guffawaffle/stfc-mod` `guffa-dev`** if you want the current working integration branch before changes are promoted to `main`.

## How This Fork Is Structured

The official project lives on **`netniV/stfc-mod`**. That is the source of truth.

This fork keeps two named branches with different jobs:

- **`main`** is the default branch and public release branch for the fork. This is the branch people should see first when they open the repository.
- **`guffa-dev`** is the working integration branch. New fork-only features land here first and are promoted to `main` once they are ready.

Feature work in this fork should branch from **`guffa-dev`**. Upstream PRs should still be prepared from fresh branches based on **`upstream/main`**, not from the fork integration branch.

The plain **`dev`** branch is intentionally not part of that layout anymore. It collides with upstream's branch naming and makes both human and agent workflows harder to reason about.

The fork-only delta is a small set of extra commits that are either:

- still being tested here before they are proposed upstream
- already proposed upstream but not merged yet
- intentionally fork-only because they are more opinionated or personal

That is the entire reason this fork exists: keep experimental changes easy to download for testers, while keeping a clear line between the official mod and my branch.

## Since `v1.1.0-guffa.rc3`

Compared with the release tag, `main` now leans much harder on a shared input-dispatcher path instead of the older one-off hotkey wiring. The practical result is more consistent hotkey behavior and clearer fallthrough rules, while also making the fork easier to maintain.

- Runtime-bound hotkeys now flow through a centralized dispatcher and action registry.
- Zoom, native ship selection, cargo interaction, and related hotkey paths now share the same binding and fallthrough logic.
- The hotkey router internals were split into smaller concern-scoped files so future edits stay local.

## Realtime Fleet Telemetry Closeout

The realtime fleet telemetry MVP is complete enough to park. The mod can emit
change-driven `fleet_runtime` snapshots to a local companion target, and the
current sidecar/Majel proof path has working local projection reads.

- Native trigger and suppression behavior is instrumented. Trigger, capture,
  unchanged/non-meaningful suppression, queue, and post results are logged and
  mirrored into recent-event breadcrumbs for operator review.
- `fleet-slot-cargo-gained` is a live-debug diagnostic event, not the default
  full runtime snapshot trigger. Full runtime sync is driven by deployment and
  fleet-transition hooks such as set-course, battle, stale-data, and arrival
  transitions.
- Remaining weirdness is documented rather than promoted into a new roadmap:
  packaged Fleet-page live-update still wants an explicit operator smoke
  record, reconnect/stale-state notes belong in Companion QA, and oversized
  `battlelogs_realtime` payloads remain a separate bug.
- This is maintenance/harvest mode, not a new roadmap. The useful chain
  exists, the diagnostics are in place, and follow-on work such as durable
  outbox, cloud auth/token lifecycle, and storage generalization is
  intentionally deferred.

## What's different in this fork?

This fork (`main` for published builds, `guffa-dev` for current integration work) bundles experimental features that haven't been accepted upstream yet, or are too opinionated for the main project:

| Feature | Status | Upstream PR |
|---------|--------|-------------|
| Hotkey dispatch table refactor | Pending review | [#126](https://github.com/netniV/stfc-mod/pull/126) |
| Double-tap Escape to exit | Pending review | [#124](https://github.com/netniV/stfc-mod/pull/124) |
| Input fallthrough policy controls | Pending review | — |
| OS toast notifications (victory/defeat) | Pending review | [#131](https://github.com/netniV/stfc-mod/pull/131), [#132](https://github.com/netniV/stfc-mod/pull/132) |
| Fleet-bar arrival notifications | Landed in fork | [#13](https://github.com/Guffawaffle/stfc-mod/issues/13), [#14](https://github.com/Guffawaffle/stfc-mod/pull/14) |
| Audio notification cues | Landed in fork | — |
| Duplicate hook crash fix | Pending review | [#130](https://github.com/netniV/stfc-mod/pull/130) |

**If any of these features get merged upstream, use the official mod instead.** This fork is a playground, not a competing project.

## Configuring fork-only features

Edit `community_patch_settings.toml` in your STFC game folder. The full commented sample in this repo is [example_community_patch_settings.toml](example_community_patch_settings.toml).

Desktop and audio notification delivery are currently Windows-focused; macOS builds ignore those notification delivery switches for now.

### Hotkey fallthrough

The mod normally owns the per-frame hotkey path. If you want unhandled keys to keep reaching the game's original input layer, use the explicit input policy block:

```toml
[control]
hotkeys_enabled = true
use_scopely_hotkeys = false

[input]
scopely_shortcuts = "off"
original_frame_policy = "fallthrough_unhandled"
```

`scopely_shortcuts = "off"` keeps Scopely's shortcut layer from registering native shortcut callbacks while the mod owns hotkeys. `original_frame_policy = "fallthrough_unhandled"` calls the original `ScreenManager::Update` path only when the mod router does not consume the frame. Runtime bindings use exact modifiers: `E` does not match `CTRL-E`, and a handled modified key-down chord such as `CTRL-E` or `ALT-1` consumes the original game key event so the bare key does not also fire. If you intentionally want Scopely's shortcut layer alive, set `scopely_shortcuts = "fallback"` or `scopely_shortcuts = "native"` explicitly.

The older compatibility switch still works:

```toml
[control]
allow_key_fallthrough = true
```

That legacy switch maps to unhandled-frame fallthrough only; it no longer initializes Scopely shortcuts unless `use_scopely_hotkeys = true` or `[input].scopely_shortcuts` explicitly opts in.

### Alt shortcut fallback

Some STFC/Unity input paths may not reliably deliver `ALT-*` chords to the mod. If an Alt shortcut such as `ALT-C` or `ALT-1` does not fire for you, leave the feature enabled and rebind that action to a Ctrl/Shift chord instead:

```toml
[input.bindings]
show_chatside1 = "CTRL-SHIFT-C"
show_alliance = "CTRL-SHIFT-'"
toggle_cargo_default = "CTRL-SHIFT-1"
toggle_cargo_player = "CTRL-SHIFT-2"
toggle_cargo_station = "CTRL-SHIFT-3"
toggle_cargo_hostile = "CTRL-SHIFT-4"
toggle_cargo_armada = "CTRL-SHIFT-5"
hotkeys_disable = "CTRL-SHIFT-F6"
hotkeys_enable = "CTRL-SHIFT-F5"
```

The older `[shortcuts]` keys still work, but `[input.bindings]` is the preferred override location for this fork. Avoid plain `CTRL-1`, `CTRL-2`, `CTRL-3`, `CTRL-C`, and `CTRL-Q` unless you also move the existing chat-channel and fleet queue shortcuts that already use those chords.

### Desktop notifications

Desktop notifications are off by default. Turn on the system notification master switch, then enable the individual events you care about:

```toml
[notifications.system]
enabled = true

[notifications]
notifications_victory = true
notifications_defeat = true
notifications_armada_created = true
notifications_armada_canceled = true

[notifications.events.fleet]
arrived_in_system = { system = true, audio = false, sound = "arrival" }
arrived_at_destination = { system = true, audio = false, sound = "soft" }
started_mining = { system = true, audio = false, sound = "ping" }
node_depleted = { system = true, audio = false, sound = "warning" }
docked = { system = true, audio = false, sound = "soft" }
repair_complete = { system = true, audio = false, sound = "repair" }
```

Battle, armada, event, and experimental toast-backed notifications are controlled by the flat `notifications_*` booleans in `[notifications]`. Fleet-bar derived notifications use the compact rows in `[notifications.events.fleet]`, where `system` controls the OS notification.

### Audio notifications

Audio notifications are independent from desktop notifications. Turn on the audio master switch, then enable `audio = true` on the events you want to hear:

```toml
[notifications.audio]
enabled = true
default_sound = "default"

[notifications.events.fleet]
arrived_in_system = { system = true, audio = true, sound = "arrival" }
started_mining = { system = false, audio = true, sound = "ping" }
node_depleted = { system = true, audio = true, sound = "warning" }
repair_complete = { system = false, audio = true, sound = "repair" }
```

Supported sound names are `default`, `info`, `success`, `warning`, `alarm`, `arrival`, `soft`, `ping`, `repair`, and `none`. Use `none`, `off`, or `silent` when you want an event row to keep its desktop notification policy but suppress its sound.

## Downloads

Download builds from this fork's **[Releases page](https://github.com/Guffawaffle/stfc-mod/releases)**.

Windows releases include both `stfc-community-mod.zip` and a direct `version.dll` asset, plus `SHA256SUMS.txt` if you want to verify what you downloaded.

The install process is identical to the official mod — see [Installation](#installing) below.

> **Prefer the official mod?** Get it at **[netniV/stfc-mod releases](https://github.com/netniV/stfc-mod/releases/latest)**.

## IMPORTANT NOTE:

The latest full release will always be available on the GitHub site. However, for interim patches, please see the #info channel
on the [STFC Community Mod](https://discord.gg/PrpHgs7Vjs) discord server. This channel will always contain any hotfixes that
enable the game to work even if the full release does not.

## Contributing / Development

If you wish to contribute to the project, or simply compile the DLL yourself, please see [CONTRIBUTING.md](CONTRIBUTING.md)

There is a discord server with friendly, helpful people who will assist if you have issues (see the support section below).

This project is maintained solely at my own cost of time, energy and money. Any contributions and help are greatly welcomed.

## Features

- Set system UI scale + adjustment factor
- Set viewer UI scale
- Set system zoom
  - default
  - maximum
  - keyboard speed
  - presets (1-5)
- Set transition time
- Disable various toast banners
- Disable galaxy chat
- Enable/Disable hotkeys (community mod or scopely)
- Let unhandled hotkeys fall through to the game's original input path
- Enable extended donation slider (alliance)
- Fleet-bar arrival notifications for player ships
- In-game audio cues for notification events
- Show alternative cargo screens for:
  - default
  - player
  - station
  - hostile
  - armada
- Press ESCAPE to remove pre-scan viewers
- Skip reveal sections when opening chests
- Exit section when collecting gifts
- Create default toml file settings file if none exists
- Create parsed toml file to show what settings have been applied
- Customise your keyboard shortcuts

## Installing

Please see the [INSTALL.md](INSTALL.md) instructions which has steps on how to use this mod with Star Trek Fleet Command.

Please note, that whilst Mac support was added in this version, it's supported on an as-is basis due to lack of Mac development environments.

## Keyboard shortcuts

Most keyboard shortcuts can be modified by updated your TOML file. If your
file is empty, see the VARS file which has all the runtime settings that have
been applied. Valid values for any short can be found in [KEYMAPPING.md](KEYMAPPING.md)

### UI shortcuts

|          Key | Shortcut                         |
| -----------: | -------------------------------- |
|          F10 | Bug fixer (exits game)           |
|        F1-F5 | Zoom presets                     |
|            Q | Zoom Out                         |
|            E | Zoom In                          |
|        MINUS | Zoom (min)                       |
|       EQUALS | Zoom (default)                   |
|    BACKSPACE | Zoom (max)                       |
|            C | Open/Focus Chat - Full Screen    |
|        Alt-C | Open/Focus Chat - Side of Screen |
|            ` | Open/Focus Chat - Side of Screen |
|         PGUP | UI Scale Up                      |
|       PGDOWN | UI Scale Down                    |
|   SHIFT-PGUP | UI Viewer Scale Up               |
| SHIFT-PGDOWN | UI Viewer Scale Down             |

### Combat/Navigation shorcuts

|             Key | Shortcut                                                                   |
| --------------: | -------------------------------------------------------------------------- |
| SPACE or MOUSE1 | Perform default action (MOUSE1 = right mouse click)                        |
| SPACE or MOUSE1 | Add to Kir'Shara queue (if owned) and already attacking                    |
|             1-8 | Ship select/focus                                                          |
|               R | When ship selected, recall ship                                            |
|               R | When clicking on mine/player/enemy, perform non-default action (eg, scan)  |
|               V | When clicking on mine/player/enemy, toggle view of cargo or default screen |
|          CTRL-Q | Enable/Disable Kir'Shara queue (if owned)                                  |
|          CTRL-C | Clear Kir'Shara queue (if owned)                                           |

NOTE: There are some common changes made to allow both mouse and keyboard to
action items such as:

- set action_queue, action_primary and action_recall_cancel to `SPACE|MOUSE1`
  allowing both right mouse click and spacebar to action attacks on (or
  queuing of) hostiles or cancel a warp.

- set action_recall to `R|MOUSE3` to allow recalling using
  both spacebar and the side mouse button

### Section shortcuts

|     Key | Shortcut         |
| ------: | ---------------- |
|       T | Events           |
|       G | Galaxy           |
|       H | System           |
| Shift-G | Exterior View    |
| Shift-H | Interior View    |
|       B | Bookmarks        |
|       F | Factions         |
| Shift-F | Refinery         |
| Shift-I | Artifact Gallery |
|       U | Research         |
|       Y | Scrap Yard       |
|       I | Inventory        |
|       M | Active Missions  |
|       O | Command Center   |
| Shift-O | Officers         |
| Shift-Q | Q-Trials         |
| Shift-T | Away Teams       |
|       X | ExoComp          |
|       Z | Daily Missions   |

## Support

For STFC Community Mod items, please visit the [STFC Community Mod](https://discord.gg/PrpHgs7Vjs) discord server.

Tashcan has now retired all things STFC from [Ripper's Corner](https://discord.gg/gPuQ5sPYM9) but still swing by to say hello to the wonderful man.

## Disclaimer

This is intended to give people insight and possibility to add new things for QoL improvements.

There is no guarantee or promise that using this for features outside of what is officially offered via this repository will not result actions against your account.

All features and additions provided here via this repository are sanctioned by Scopely and thus aren't subject to account actions.

## License

- GPLv3
