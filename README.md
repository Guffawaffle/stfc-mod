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

## What's different in this fork?

This fork (`main` for published builds, `guffa-dev` for current integration work) bundles experimental features that haven't been accepted upstream yet, or are too opinionated for the main project:

| Feature | Status | Upstream PR |
|---------|--------|-------------|
| Hotkey dispatch table refactor | Pending review | [#126](https://github.com/netniV/stfc-mod/pull/126) |
| Double-tap Escape to exit | Pending review | [#124](https://github.com/netniV/stfc-mod/pull/124) |
| Key fallthrough config option | Pending review | — |
| OS toast notifications (victory/defeat) | Pending review | [#131](https://github.com/netniV/stfc-mod/pull/131), [#132](https://github.com/netniV/stfc-mod/pull/132) |
| Fleet-bar arrival notifications | Landed in fork | [#13](https://github.com/Guffawaffle/stfc-mod/issues/13), [#14](https://github.com/Guffawaffle/stfc-mod/pull/14) |
| Duplicate hook crash fix | Pending review | [#130](https://github.com/netniV/stfc-mod/pull/130) |

**If any of these features get merged upstream, use the official mod instead.** This fork is a playground, not a competing project.

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
- Enable extended donation slider (alliance)
- Fleet-bar arrival notifications for player ships
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
