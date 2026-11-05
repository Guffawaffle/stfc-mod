# PLAY-NOTIFY-MAC — permission-aware desktop notifications

Frozen `1baa55fe` `mods/src/notification_desktop_mac.mm` is preserved in target
`src/mod/features/notification_desktop_mac.mm` with exported API renames only.
Source comparison included all nested completion handlers and ARC/non-ARC
ownership paths. [Cohort](../FEATURE-COHORT-20261031-AUDIO-NOTIFICATIONS.md).

- Require a real application bundle; contain notification-center exceptions.
- Request Alert authorization on the main queue when notifications are enabled;
  do not replace the game's delegate or add an OS sound over custom audio.
- Copy UTF-8 title/body into owned NSStrings before asynchronous delivery.
  Only Authorized/Provisional settings permit a request with a fresh identifier.
  Preserve non-ARC releases and block custody through nested callbacks.
- Missing center/permission declines safely; log service errors without claiming
  delivery. No synthetic authorization status or fallback to another account.
- The shared initializer requests at most once per process; OS permission
  changes remain separate from config preferences and successful enqueue.

Source preservation and earlier CI compilation are not live delivery acceptance.
Current fixtures substitute the Mac sink and never request OS authorization.
Actual permission grants/denials, visible notifications, Intel/arm64 runtime and
bundle behavior remain unqualified. No platform implementation changed here.
