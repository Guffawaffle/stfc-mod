# Quasel's Mac profiles preview

This test candidate combines the downstream `play` mod with named profiles and a
preview panel in the existing Mac launcher. It targets Apple Silicon, macOS 14.6
or later. The parked system-background experiments are excluded.

## Install and launch

1. Download the **complete preview installer/app bundle** linked with the candidate.
   Keep your existing launcher and give this app a separate name, such as
   **STFC Profiles Preview.app**, when copying it to Applications. Do not extract
   just the dylib: the bundled `stfc-profiles` helper is also required.
2. Install the ordinary STFC Mac client. Install Google Chrome or Microsoft Edge
   in `/Applications`; the preview uses a separate browser store for each profile's
   Scopely sign-in.
   A managed browser storage override is refused; use a browser that can honor
   the private profile directory.
3. Open the preview app and click **Profiles Preview…** at the top of the window,
   beside **TEST BUILD**. You can also use **View → Profiles Preview…** or
   **Command-Shift-P**. Create **Quasel Test**.
   This creates a fresh profile; it does not copy your ordinary game login or
   Spocks configuration.
   **Open TOML** also exists in ordinary downstream releases and does not identify
   this preview; check the preview header and bundled `Contents/stfc-profiles`
   helper if you have several launcher copies installed.
4. Check the selected game path. **Choose Game…** selects
   **Star Trek Fleet Command.app** if the automatic launcher path is unavailable
   or you want to use another installed game copy.
5. Select **Quasel Test** and click **Launch Profile**. The panel waits for the game
   to confirm profile isolation. **Ready (PID …)** means the requested profile's
   preference hooks and session lease were admitted; it does not mean account
   login has finished.
6. Sign in to the intended test account in the separate browser window. Test the
   normal mod features in that game. Close the panel and use **Engage** to launch
   your ordinary setup. Different profiles can use the same game app, with separate
   preferences, mod configuration, logs and sign-in browsers.

Use the candidate's stated signing status. If an **ad-hoc signed Actions build**
is supplied, macOS may require approving that specific app in **System Settings →
Privacy & Security → Open Anyway**. A Developer ID signed and notarized installer
uses the normal opening flow. Do not disable Gatekeeper globally.

## Configuration and repeat launches

**Open Profile Folder** opens the selected directory. Its `config.toml` is the
profile's mod configuration; `logs/Player.log` and `logs/community-mod.log` hold
its game/mod logs. Quit that profile's game before editing its configuration, then
launch the same profile again. Changes made through ordinary **Open TOML** belong
to ordinary play.
The preview updater refuses to modify an installation used by a modded game;
named launches are likewise refused while the preview holds update access.

Named profiles live under
`~/Library/Application Support/STFC Profiles/profiles/<immutable ID>/`. Their
saved login is encrypted using your Mac user Keychain. Keep the profile directory
and its Keychain identity together; copying the directory to another Mac does not
transfer the login. This preview has no import of an existing Mac account.

Quit the profile's game and isolated browser before retrying its launch. A second
launch of the same live profile is refused. After a timeout, inspect whether its
game is still running before retrying; the coordinator does not silently launch
the ordinary account instead.

## What to check

- Start the ordinary game and **Quasel Test** together. Confirm the named profile
  opens a separate game process and the ordinary account/configuration remain as
  expected.
- Sign in, quit the test game/browser, and relaunch **Quasel Test**. Confirm its
  login and mod settings persist independently.
- Create a second named profile and verify that it starts fresh and can run
  alongside the first. Trying to relaunch the same active profile should fail.
- Confirm ordinary Engage still opens the original setup with the full mod.

Report the candidate revision, macOS version, selected game version and the step
that succeeded or failed. The native build/fixture results and actual game results
are separate evidence: Quasel's gameplay and Scopely sign-in are the purpose of
this test candidate.

## Terminal fallback

The preview panel uses this same bundled coordinator. With the app installed as
`/Applications/STFC Profiles Preview.app`, you can inspect profiles with:

```sh
"/Applications/STFC Profiles Preview.app/Contents/stfc-profiles" list --json
```

To launch an existing ID against an explicitly selected game:

```sh
"/Applications/STFC Profiles Preview.app/Contents/stfc-profiles" launch \
  --profile YOUR_32_CHARACTER_ID \
  --game "/path/to/Star Trek Fleet Command.app/Contents/MacOS" \
  --runtime "/Applications/STFC Profiles Preview.app/Contents/libstfc-community-mod.dylib" \
  --json
```

`sessions --json` shows current named-session readiness and process identities.
