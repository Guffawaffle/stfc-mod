# Streamer Mode design

The first Streamer Mode setting lives in **Mod Settings → Interface & Scale**. It defaults off. Enable it to replace the Player ID in **General Settings** with **Hidden** and disable that row’s Copy action. The selected protections below describe possible extensions.

## Proposed controls

- Put one **Streamer Mode** switch in Mod Settings. It starts off and saves its state in the current mod configuration. When enabled, it protects the account fields covered by the current build.
- Offer independent switches for the other groups as their screens gain coverage. Start those switches off so players can keep using the parts of the interface they want on stream.
- Show a small **Streamer Mode on** status indicator. Report individual protections as unavailable when the current client cannot apply them.
- Consider an unbound **Enable Streamer Mode** shortcut. Pressing the shortcut while protection is already on leaves it enabled. Turning protection off takes an explicit choice in Mod Settings.

## What to cover

| Protection | What a viewer could see | Proposed presentation |
| --- | --- | --- |
| Account details | Player ID in General Settings first. Audit other account identifiers, linked email, support or verification codes, and QR codes separately. | Replace text with fixed **Hidden**. Cover codes rendered as images with an opaque placeholder. Disable the masked row’s copy or reveal actions while covered. |
| Commander identity | Commander names, alliance tags, server labels, and identifying cosmetics on profiles, fleet labels, station cards, and chat rows. | Use **You** for the local commander and session-local aliases such as **Player 01** for other covered names. Hide associated tags and optionally portraits or frames. Keep ship and combat state readable. |
| Locations | Station location, system names, coordinates, bookmarks, fleet destinations, and location links. | Replace exact values with neutral labels and cover location previews where necessary. Visible map geometry, routes, and landmarks can still identify a system, so offer a full map cover for streams that require location privacy. |
| Chat and messages | HUD previews, full and side chat, direct messages, drafts, and inbox messages. | Cover the contents and sender details together. Keep navigation and a generic new-message indication. The game displays chat messages in its map HUD as well as in the chat screen. [STFC chat guide](https://scopely.helpshift.com/hc/en/19-star-trek-fleet-command/faq/3741-how-do-i-use-and-access-chat/) |
| Reports and notifications | Report list and details, opponent and alliance names, battle locations, and in-game or mod desktop notifications. | Apply the selected identity and location protection to report displays. Offer generic notification text such as **Battle report received** and keep outcomes and round information readable. |
| Balances and purchases | Personal balances, purchase history, and account details shown by in-game commerce screens. | Replace only identified balances. Use an opaque cover for sensitive commerce panels that lack reliable field-level coverage. |

## First implementation

The General Settings Player ID row uses `game_settings_option_0`. Its value comes from `GetUserId`, and its native copy action reads the ID separately. Protect both parts of that row. Scopely uses the ID for support, so turning Streamer Mode off should restore the current value and the ordinary copy action. [Settings hook](../src/mod/features/settings.cc) · [Scopely support guide](https://scopely.helpshift.com/hc/en/19-star-trek-fleet-command/faq/6507-how-to-contact-us-through-support/)

Reuse the existing General Settings hook owner and live settings catalog. Apply the mask before the row binds or becomes visible. Keep it in place through refreshes and widget reuse, then rebind from the current native model when protection is turned off. Keep the real ID in its original data path so account services continue to receive it. Audit the separate `PlayerIdViewController` before claiming coverage for other ID screens. [Settings catalog](../src/mod/settings/config_settings.cc)

Qualify each later mask at its own display and notification paths. A location hidden in a bookmark row can still appear in its editor, a report, or a toast. Where a covered panel cannot safely bind its fields, keep it covered and show its protection status. Streamer Mode applies to the desktop game and supported mod notifications. A stream that captures a browser, the launcher, or the full desktop can also show information from those sources.

## Acceptance checks

- Use synthetic IDs to inspect the first frame of saved-on startup, opening and reopening General Settings, and enabling protection while the panel is already visible.
- Check that refreshes and reused rows stay masked, the ID copy action stays covered, and turning the mode off restores the native value and action. The client version row also uses a copy action and should keep its own value and behavior.
- Verify rendered captures on Windows and macOS before advertising coverage. Record coverage by surface so additions such as chat previews and desktop notifications can be tested independently.
