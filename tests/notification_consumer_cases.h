#pragma once

// Production consumers with controlled settings, payload decoding and sound delivery.
// No desktop notification, file access or game action is requested by these cases.
#include "features/fleet_watch.h"
#include "features/notification_audio.h"
#include "features/notifications.h"
#include "game/notifications.h"

struct NotificationFixtureConfig {
  using Patches = Config::Patches;
  Config::Audio                     audio{};
  Config::Ui                        ui{};
  static NotificationFixtureConfig& Get();
};
NotificationFixtureConfig  notification_first, notification_second;
NotificationFixtureConfig* notification_config = &notification_first;
NotificationFixtureConfig& NotificationFixtureConfig::Get()
{ return *notification_config; }
std::vector<NotificationSound>      notification_played;
std::function<void()>               notification_on_payload;
std::optional<IncomingPlayerAttack> notification_attack;
std::optional<IncomingPlayerAttack> NotificationFixtureAttack(Toast*)
{
  if (notification_on_payload)
    notification_on_payload();
  return notification_attack;
}
void NotificationFixturePlay(const NotificationAudioCue& cue)
{ notification_played.push_back(cue.sound); }
std::span<const FeatureInstallation> NotificationFixtureInstallations()
{ return {}; }

// Keep notification subscriptions separate from unrelated production watcher fixtures.
namespace notification_watch_fixture
{
using Subscription = fleet_watch::Subscription;
using Snapshot     = fleet_watch::Snapshot;
using Transition   = fleet_watch::Transition;
bool Subscribe(Subscription)
{ return true; }
} // namespace notification_watch_fixture
#if __APPLE__
void NotificationDesktopMacInit() {}
void NotificationDesktopMacEmit(std::string_view, std::string_view)
{ throw std::runtime_error("fixture must not deliver a desktop notification"); }
#endif
#define Config NotificationFixtureConfig
#define PlayNotificationSound NotificationFixturePlay
#define ReadIncomingPlayerAttack NotificationFixtureAttack
#define FeatureInstallations NotificationFixtureInstallations
#include "../src/mod/features/toasts.cc"
#undef FeatureInstallations
#undef ReadIncomingPlayerAttack
#define fleet_watch notification_watch_fixture
#include "../src/mod/features/fleet_notifications.cc"
#undef fleet_watch
#include "../src/mod/features/audio.cc"
#undef PlayNotificationSound
#undef Config

namespace
{
struct NotificationToast {
  Il2CppObject header{};
  ToastState   state = ToastState::Victory;
};
ToastState NotificationState(Toast* value, const MethodInfo*)
{ return reinterpret_cast<NotificationToast*>(value)->state; }

void NotificationConsumerCases(bool reload)
{
  auto&       toast_class = Define("Digit.Prime.HUD", "Toast", IL2CPP_TYPE_CLASS, &object_class);
  const auto& state =
      AddMethod(toast_class, "get_State", reinterpret_cast<void*>(&NotificationState), T(int_class), {});
  AddProperty(toast_class, "State", &state);
  NotificationToast value{{&toast_class.klass}};
  auto*             toast = reinterpret_cast<Toast*>(&value);
  if (reload) {
    value.state                                            = ToastState::IncomingAttack;
    notification_first.audio.alert_incoming_player_attack  = NotificationAudioCue(NotificationSound::kWarning);
    notification_second.audio.alert_incoming_player_attack = NotificationAudioCue(NotificationSound::kAlarm);
    notification_attack = IncomingPlayerAttack{.fleet_id = 123, .slot = 1, .attacker = "fixture", .event_id = "unique"};
    // Retain the old C++ object so this detects stale semantics without relying on freed memory.
    notification_on_payload = [] { notification_config = &notification_second; };
    ScopedToastNotificationSuppression no_desktop;
    Require(HandleIncomingPlayerAlert(toast), "valid incoming payload is handled");
    Require(notification_config == &notification_second, "payload decoding exercised reentrant settings replacement");
    for (auto sound : notification_played)
      std::cout << "Incoming reload cue: " << NotificationSoundName(sound) << '\n';
    Require(notification_played == std::vector{NotificationSound::kAlarm},
            "incoming native reads must not retain pre-reload settings");
    Require(HandleIncomingPlayerAlert(toast) && notification_played.size() == 1,
            "duplicate incoming payload does not replay sound");
    return;
  }
  notification_first.audio.alert_victory      = NotificationAudioCue(NotificationSound::kSuccess);
  notification_first.ui.disabled_banner_types = {std::to_underlying(ToastState::Victory)};
  int originals                               = 0;
  ToastObserver_EnqueueToast_Hook([&](auto*, auto*) { ++originals; }, nullptr, toast);
  Require(originals == 0 && notification_played == std::vector{NotificationSound::kSuccess},
          "hidden banner still plays its independently configured sound");
  notification_first.ui.disabled_banner_types.clear();
  ToastObserver_EnqueueOrCombineToast_Hook(
      [&](auto*, auto*, uintptr_t comparison) {
        Require(comparison == 42, "combine argument remains intact");
        ++originals;
      },
      nullptr, toast, 42);
  Require(originals == 1 && notification_played.size() == 2, "visible native banner delegates once");
  Require(ShowBanner(nullptr), "missing toast preserves native banner path");

  int  native_audio   = 0;
  auto audio_original = [&](auto*, auto* name, int32_t action, auto*, auto*, auto*, bool queued, auto*) {
    Require(queued && (!name || action >= 0), "native audio arguments remain intact");
    ++native_audio;
    return true;
  };
  auto* sound_name                            = il2cpp::NewString("fixture-audio");
  notification_first.audio.disable_all_events = true;
  for (int action : {0, 3, 16, 33})
    Require(!AudioEventManager_PostEvent_Hook(audio_original, nullptr, sound_name, action, nullptr, nullptr, nullptr,
                                              true, nullptr),
            "all-audio filter suppresses start actions");
  for (int action : {1, 21, 23})
    Require(AudioEventManager_PostEvent_Hook(audio_original, nullptr, sound_name, action, nullptr, nullptr, nullptr,
                                             true, nullptr),
            "all-audio filter retains cleanup");
  notification_first.audio.disable_all_events = false;
  notification_first.audio.disabled_events    = {"fixture-audio"};
  Require(!AudioEventManager_PostEvent_Hook(audio_original, nullptr, sound_name, 21, nullptr, nullptr, nullptr, true,
                                            nullptr),
          "named filter preserves legacy all-action suppression");
  Require(
      AudioEventManager_PostEvent_Hook(audio_original, nullptr, nullptr, 0, nullptr, nullptr, nullptr, true, nullptr)
          && native_audio == 4,
      "null audio name delegates");

  // Independent availability and live activation. No desktop delivery is performed.
  s_available_events                          = fleet_notification_bit(FleetNotificationKind::Docked);
  notification_first.audio.alert_fleet_docked = NotificationAudioCue(NotificationSound::kArrival);
  RefreshFleetNotificationAudio();
  Require(event_enabled(FleetNotificationKind::Docked) && !notification_enabled(FleetNotificationKind::Docked),
          "audio-only fleet event stays active independently of desktop settings");
  const auto before = notification_played.size();
  s_audio_activation.Observe(2);
  play_event_audio(FleetNotificationKind::Docked, 2);
  Require(notification_played.size() == before, "first fresh observation after enable is silent");
  s_audio_activation.Observe(2);
  fleet_watch::Transition transition{{2, 99, FleetState::Impulsing}, {2, 99, FleetState::Docked}, nullptr, 1};
  emit_transition(transition);
  Require(notification_played.size() == before + 1 && notification_played.back() == NotificationSound::kArrival,
          "subsequent real dock transition uses configured sound");
  notification_first.audio.alert_fleet_docked = NotificationAudioCue{};
  RefreshFleetNotificationAudio();
  Require(!event_enabled(FleetNotificationKind::Docked), "live disable removes event interest");
  notification_first.ui.notify_fleet_events = kAllFleetNotifications;
  RefreshFleetNotificationAudio();
  Require(s_enabled_notifications == s_available_events && s_enabled_events == s_available_events,
          "unavailable event producers cannot become ready merely from settings");
  s_available_events |= fleet_notification_bit(FleetNotificationKind::NodeDepleted);
  bool scoped_original = false;
  ToastFleetObserver_HandleMiningDepleted_Hook(
      [&](auto*, int64_t id) {
        Require(id == 0, "native depletion identity is not changed");
        scoped_original = toast_notification_suppression_depth == 1;
      },
      nullptr, 0);
  Require(scoped_original && toast_notification_suppression_depth == 0,
          "dedicated depletion forwarding suppresses generic desktop only during original");
  std::cout << "Production audio, banner and fleet consumer cases passed\n";
}
} // namespace
