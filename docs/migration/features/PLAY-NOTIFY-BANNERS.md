# PLAY-NOTIFY-BANNERS — native banners and desktop forwarding

Frozen `1baa55fe`: complete `parts/disable_banners.cc`,
`patches/notification_service.cc`, battle text parser, toast/localization bindings,
config and platform service. Target `features/toasts.cc`, `toast_text.h`, typed
UI/server bindings and desktop backend. [Cohort and limits](../FEATURE-COHORT-20261031-AUDIO-NOTIFICATIONS.md).

- Both EnqueueToast and EnqueueOrCombineToast process side effects before
  independently checking disabled_banner_types. Hidden banners still permit
  configured cue/desktop forwarding. Visible banners delegate once, retaining
  the combine comparison argument. Null toasts preserve native delegation.
- notify_banner_types controls forwarding, not native visibility. Preserve known
  title/category tables, readable battle/armada text, rich-text removal and
  no-details fallback; unknown unmapped states do not invent titles.
- Dedicated incoming-player handling precedes generic forwarding and suppresses
  duplicate side effects. Depletion's thread-local scoped suppression affects
  generic desktop forwarding only, restores after unwinding and keeps audio/native
  banners independent. Do not confuse suppression with disabling all notifications.
- Target's existing rooted/qualified parameter sources replace the old broad
  array-field scan; it does not scan arbitrary arrays to guess missing parameters.
  Earlier toast-parameter fixtures separately qualify that deliberate adaptation.
- `[patches].toastbannerhooks` owns one transactional pair. Optional missing
  installation leaves controls unavailable, not falsely ready. No extra hook,
  layout, banner-list redesign or persistence changes this pass.

Controlled actual hooks cover hidden/visible/null/combine paths and sound
independence. Text-policy fixtures cover parameter rules. No real Windows or Mac
desktop notification is sent; OS acceptance, real localization and client
pooling are not established by those tests.
