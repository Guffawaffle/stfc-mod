# Star Trek Fleet Command - Community Mod

<p align="center">
  <img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPLv3">
  <img src="https://img.shields.io/github/sponsors/netniv" alt="Sponsorship">
</p>

<p align="center">
   A community mod (patch) that adds a couple of tweaks to the mobile game <b>Star Trek Fleet Command&#8482;</b>
</p>

## Valid shortcut key strings

Each game function can be assigned a shortcut KEY (with the exception of escape that is fixed).

To use a Modifier, combine it with a single KEY (and optionally other modifiers) separated by a minus symbol:

Value | Keys Required
--: | :--
"SHIFT-Q" | (Left Or Right) Shift + Q
"ALT-LSHIFT-T" | (Left Or Right) Alt + Left Shift + T
"Q" | Q (no Shift, Ctrl, etc)

### Modifiers

Modifiers are not required, any specified must be used together:

Value | Key | Value | Key | Value | Key
--: | :-- | --: | :-- | --: | :--
"SHIFT" | LeftShirt or RightShift | "LSHIFT" | LeftShift | "RSHIFT" | RightShift
"CTRL" | LeftControl or RightControl | "LCTRL" | LeftControl | "RCTRL" | RightControl
"ALT" | LeftAlt or RightAlt | "LALT" | LeftAlt | "RALT" | RightAlt
"WIN" | LeftWindows or RightWindows | "LWIN" | LeftWindows | "RWIN" | RightWindows
"ALTGR" | AltGr

`ALT` is a valid binding modifier, but some STFC/Unity input paths may not deliver Alt-modified chords reliably in game. If an `ALT-*` shortcut does not fire, rebind that action under `[input.bindings]` with a Ctrl/Shift chord; the fork README has ready-to-copy examples.

### Keys

Only key can be set at one time:

Value | Key | Value | Key | Value | Key | Value | Key
--: | :-- | --: | :-- | --: | :-- | --: | :--
"HOME" | Home | "END" | End  | "PGUP" | PageUp | "PGDOWN" | PageDown
"LEFT" | LeftArrow | "RIGHT" | RightArrow | "UP" | UpArrow | "DOWN" | DownArrow
"BACKSPACE" | Backspace | "CLEAR" | Clear | "CAPS" | CapsLock | "BREAK" | Break
"INSERT" | Insert | "DELETE" | Delete | "HELP" | Help | "MENU" | Menu
"PAUSE" | Pause | "PRINT" | Print | "SPACE" | Space | "RETURN" | Return
"SCROLL" | ScrollLock | "SYSREQ" | SysReq | "TAB" | Tab | "SPACE" | Space
"MOUSE0" | Mouse0 | "MOUSE1" | Mouse1 | "MOUSE2" | Mouse2 | "MOUSE3" | Mouse3
"MOUSE4" | Mouse4 | "MOUSE5" | Mouse5 | "MOUSE6" | Mouse6 | "MINUS" | Minus |
"'" | Quote | "_" | Underscore | "," | Comma | "." | Period
";" | Semicolon | ":" | Colon | "!" | Exclaim | "?" | Question
"(" | LeftParen | ")" | RightParen | "[" | LeftBracket | "]" | RightBracket
"{" | LeftCurlyBracket | "}" | RightCurlyBracket | "@" | At | "*" | Asterisk
"/" | Slash | "\" | Backslash | "\"" | DoubleQuote | "&" | Ampersand
"#" | Hash | "%" | Percent | "`" | BackQuote | "^" | Caret
"+" | Plus | "<" | Less | "=" | Equals | ">" | Greater
"|" | Pipe | "~" | Tilde | "$" | Dollar | - | -
"F1" | F1 | "F2" | F2 | "F3" | F3 | "F4" | F4
"F5" | F5 | "F6" | F6 | "F7" | F7 | "F8" | F8
"F9" | F9 | "F10" | F10 | "F11" | F11 | "F12" | F12
"F13" | F13 | "F14" | F14 | "F15" | F15
"0" | Alpha0 | "1" | Alpha1 | "2" | Alpha2 | "3" | Alpha3
"4" | Alpha4 | "5" | Alpha5 | "6" | Alpha6 | "7" | Alpha7
"8" | Alpha8 | "9" | Alpha9
"A" | A | "B" | B | "C" | C | "D" | D
"E" | E | "F" | F | "G" | G | "H" | H
"I" | I | "J" | J | "K" | K | "L" | L
"M" | M | "N" | N | "O" | O | "P" | P
"Q" | Q | "R" | R | "S" | S | "T" | T
"U" | U | "V" | V | "W" | W | "X" | X
"Y" | Y | "Z" | Z
"KEY0" | Keypad0 | "KEY1" | Keypad1 | "KEY2" | Keypad2 | "KEY3" | Keypad3
"KEY4" | Keypad4 | "KEY5" | Keypad5 | "KEY6" | Keypad6 | "KEY7" | Keypad7
"KEY8" | Keypad8 | "KEY9" | Keypad9 | "KEYDIVIDE" | KeypadDivide | "KEYENTER" | KeypadEnter
"KEYEQUAL" | KeypadEquals | "KEYMINUS" | KeypadMinus | "KEYMULTI" | KeypadMultiply | "KEYPERIOD" | KeypadPeriod
"KEYPLUS" | KeypadPlus | "NUMLOCK" | Numlock

**NOTE**: Default shortcuts are shown in the [README.md](README.md)

## Input Binding Compatibility

The canonical input schema lives under `[input.bindings]`. If an action is not
listed below, the compatibility alias under `[shortcuts]` uses the same key
name as the canonical action id.

<!-- GENERATED INPUT BINDING COMPATIBILITY START -->
| [input.bindings] key | Default | Legacy [shortcuts] keys |
| --- | --- | --- |
| fleet_primary | SPACE|MOUSE1 | action_primary |
| fleet_secondary | TAB|MOUSE4 | action_secondary |
| fleet_queue_add | SPACE|MOUSE1 | action_queue |
| fleet_recall_cancel | SPACE|MOUSE1 | action_recall_cancel |
| fleet_recall | R|MOUSE3 | action_recall |
| fleet_repair | R|MOUSE3 | action_repair |
| fleet_view_info | V|MOUSE2 | action_view |
| fleet_queue_clear | CTRL-C | action_queue_clear |
| fleet_queue_toggle | CTRL-Q | toggle_queue |
| hotkeys_disable | CTRL-ALT-MINUS | set_hotkeys_disable, set_hotkeys_disble (deprecated), set_hotkeys_disabled (deprecated) |
| hotkeys_enable | CTRL-ALT-= | set_hotkeys_enable, set_hotkeys_enabled (deprecated) |
| ui_scale_up | PGUP | ui_scaleup |
| ui_scale_down | PGDOWN | ui_scaledown |
| ui_viewer_scale_up | SHIFT-PGUP | ui_scaleviewerup |
| ui_viewer_scale_down | SHIFT-PGDOWN | ui_scaleviewerdown |
<!-- GENERATED INPUT BINDING COMPATIBILITY END -->

### Binding Notes

Set a binding value to `NONE` to disable that action. Removing a key from the
TOML does not disable it; the mod falls back to the default binding.

Fleet primary and fleet queue are separate actions. If you want `SPACE` and
`MOUSE1` to share queue behavior, bind both actions explicitly, for example:

```toml
action_primary = "SPACE|MOUSE1"
action_queue = "SPACE|MOUSE1"
```

## License

- GPLv3
