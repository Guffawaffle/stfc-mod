# Streamer Mode

Open **Mod Settings → Streamer Mode** to cover account identifiers, your profile name and alliance, and optionally chat sender names. The master switch starts off. **Hide my IDs, name, and alliance** starts selected. **Hide chat sender names** is opt-in. Turning the master switch off restores native presentation and keeps your choices for next time.

## Coverage

| Control | Protected presentation | Additional exposure to check |
| --- | --- | --- |
| Hide my IDs, name, and alliance | General Settings shows **Hidden** for the Player ID and covers the identifier next to **Signed into Scopely as**. Player ID **Copy** uses the real ID and **Switch Account** stays available. The separate native Player ID label is covered when its display hook is available. In your own Player Profile, your name, alliance name, tag, and emblem are covered. Inspected profiles of other players display normally. | Profile titles, avatars, stats, and account details opened through other apps. |
| Hide chat sender names | Sender names in HUD previews, full and side chat, direct chat and channel lists, and blocked-user rows. Messages stay readable. Use the game's chat controls to close or open the panel. | Message text, titles, avatars, attachments, and other popups can still identify a sender. |

The menu shows **Protection unavailable** when the required hooks or frame delivery are missing. **On · Check protections** means a selected protection is unavailable or an active mask could not be verified. You can still turn an installed protection off. The masks act on game presentation. The game still has the original values for account features, navigation, and messaging.

## Validation

Check General Settings, your own Player Profile, an inspected profile from another player, and chat with Streamer Mode saved on. Turn it off and verify that your profile fields and account values return. Verify the Player ID Copy action and readable chat. Use synthetic identifiers where possible and record pass/fail results without capturing IDs or chat contents. Record rendered checks against the exact client and mod build tested.
