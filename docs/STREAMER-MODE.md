# Streamer Mode

Open **Mod Settings → Streamer Mode** to choose which personal information appears while you stream. The master switch starts off. Your four personal protections start selected; broader chat masks and compact ship labels are opt-in. Turning the master switch off restores your usual presentation and keeps your choices for next time.

For Player ID-only privacy, turn Streamer Mode and **Hide my player ID** on, then turn the other protections off. Names, alliances and chat remain visible.

## Coverage

| Control | Protected presentation | Additional exposure to check |
| --- | --- | --- |
| Hide my player ID | General Settings shows **Hidden** for your Player ID. The separate native Player ID label is covered when its display hook is available. Player ID **Copy** still uses the real ID. | Other apps and identifier text someone types into a message. |
| Hide my account identifier | Covers the account value beside **Signed into Scopely as** in General Settings. **Switch Account** stays available. | Account details opened through other apps. |
| Hide my name | Covers your own Player Profile name and hides your own sender name in supported chat views, even when **Hide chat sender names** is off. Other players' names can remain visible. | Profile titles, avatars, stats and other identifying content. |
| Hide my alliance | Covers your own Player Profile alliance name, tag and emblem, plus your own alliance tags in supported chat views. | Other players from the same alliance can still display their tags. |
| Hide chat sender names | Hides sender names in HUD previews, full and side chat, direct chat and channel lists, and blocked-user rows. Alliance tags have their own control. Messages stay readable. | Message text, titles, avatars, attachments and other popups can still identify a sender. |
| Hide chat alliance tags | Hides alliance tags in supported chat views independently of sender names. | Alliance information outside the supported chat surfaces. |
| Compact player ship labels | Player ships on the System map use the Compact layout, keeping the ship icon and level while omitting commander names and alliance tags together. Your usual label detail returns afterward. | Selected ship cards and inspected profiles can show details about other players. |

The menu shows **Protection unavailable** when the required hooks or frame delivery are missing. **On · Check protections** means a selected protection is unavailable or an active mask could not be verified. An unverified chat sender is treated conservatively while a personal mask is requested; the menu reports incomplete protection. You can still turn an installed protection off. Masks act on game presentation; original values remain available for account features, navigation and messaging.

Location privacy is a separate coverage task. System names, coordinates, bookmarks and chat location links are not hidden by these personal controls.

## Existing settings

The old `ui.streamer_hide_account` choice supplies the default for each of the four new personal controls when that control is not explicitly configured. The old `ui.streamer_hide_chat_names` (or older `ui.streamer_hide_chat`) choice supplies both new chat controls, preserving the previous name-and-tag protection. Each explicit new value, including Off, takes precedence. New menu edits save only the corresponding new key.

## Validation

Use synthetic identifiers and messages where possible; record pass/fail without capturing personal data. Verify Player ID-only masking and both directions of the Player ID/account-identifier split. Toggle name and alliance independently on your own profile, then inspect another player. In chat, verify your own name stays hidden with broader sender masking off while another sender remains visible. Exercise independent names and tags, live changes, private headers, pooled self/other rebinding and restoration. With compact ship labels selected, switch systems and verify their icon/level-only layout. Turn Streamer Mode off and confirm usual presentation returns while selected choices remain saved. Verify real-ID Copy, readable chat and navigation still work. Bind rendered checks to the exact client and mod build tested.
