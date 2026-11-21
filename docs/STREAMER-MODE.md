# Streamer Mode

Open **Mod Settings → Streamer Mode** to choose the privacy protections you want. The master switch applies the selected protections and starts off. **Hide Player ID** starts selected so existing Streamer Mode configurations keep their Player ID mask. The other protections start clear. Turning the master switch off restores native presentation and keeps your choices for next time.

## Coverage

| Control | Protected presentation | Additional exposure to check |
| --- | --- | --- |
| Hide Player ID | General Settings shows **Hidden**. **Copy** puts the real ID on the clipboard in either state. The separate native Player ID label is covered when its display hook is available. | Account details in sign-in pages and support codes opened through other apps. |
| Hide commander identity | Commander and alliance profile details, player and alliance fleet labels, station labels, and Galaxy fleet names. | Other profile popups and identifying art outside the covered fields. |
| Hide locations | Galaxy and System names, regional labels, the HUD home pointer, location links, and bookmark names and coordinates. | Map geometry, fleet destinations, and tooltip text can still identify a location. |
| Hide chat and messages | HUD chat previews, message lists and drafts in full, side, and direct chat, direct-chat headings and channel previews, and blocked-player profiles. | Native attachment previews, share dialogs, and other popups can have their own display paths. |
| Hide report and alert details | Combat and destruction report rows, participant and location fields in report details, participant headers in detailed battle logs, supported native banners, and the bodies of mod-delivered desktop notifications. Round and outcome information remains readable. | Other native banner types, individual ship and log entries, and notifications delivered before the switch was enabled. |
| Hide balances | Resource amounts in the standard HUD, the shared currency and resource groups and their dropdown, and the shop showcase’s resource panel when its hook is available. | Tooltips, purchase history, and separately rendered commerce panels can still display account or transaction information. |

The settings page shows whether Streamer Mode is on. Each selectable control becomes unavailable when its required hooks or frame delivery cannot be qualified. The masks act on game presentation. The game still has the original values for account features, navigation, and messaging.

## Validation

Check new presentation coverage on every supported client build. Confirm the first visible frame with Streamer Mode saved on, toggle it with surfaces already open, and reopen or recycle the masked rows. Verify that native navigation and conversation input still work. Use synthetic identifiers where possible and record pass/fail results without capturing IDs, chat, or financial details.

The initial Player ID implementation was live-tested on Windows client 275. The expanded masks need separate rendered checks on the exact build being tested. A compile check establishes build coverage for a target architecture.
