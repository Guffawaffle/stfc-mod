#include "audio_alerts.h"
#include "config.h"
#include "features/notification_audio.h"
#include "features/notifications.h"
#include "game/notifications.h"
#include "persisted_setting.h"

#include "features/features.h"
#include "features/fleet_notification_settings.h"

#include "audio_file_picker.h"
#include "file_picker_task.h"
#include "frame.h"
#include "native_controls.h"
#include "runtime_config.h"
#include <chrono>
#include <memory>
#include <spdlog/spdlog.h>

namespace mod_settings
{
namespace
{
  struct Alert {
    std::string                            key, label;
    std::function<NotificationAudioCue&()> cue;
    std::function<bool()>                  available;
    bool                                   fleet = false;
    NotificationAudioCue                   custom;
    std::uint64_t                          generation = 1;
    std::unique_ptr<ChoiceSetting>         choice;
    ActionSetting                          preview;
    ActionSetting                          choose;
    std::string                            status;
    NotificationAudioCue                   observed;
    bool                                   observed_known = false;
  };
  std::vector<std::unique_ptr<Alert>> s_alerts;
  std::unique_ptr<ChoiceSetting>      s_coalescing;
  constexpr int                       kCustom             = static_cast<int>(NotificationSound::kCount);
  Alert*                              s_pending           = nullptr;
  bool                                s_picker_available  = false;
  bool                                s_refresh_requested = false;
  std::future<std::string>            s_picker;
  std::future<NotificationAudioCue>   s_loading;
  NotificationAudioCue                s_pending_observation;
  std::uint64_t                       s_pending_generation = 0;
  bool                                s_discard_pending    = false;
  void                                PollFilePicker();

  bool SameCue(const NotificationAudioCue& a, const NotificationAudioCue& b)
  { return a.sound == b.sound && a.source == b.source && a.data == b.data; }

  bool IncomingAudioAvailable()
  { return ToastAudioAvailable() && IncomingPlayerAttackAvailable(); }

  void ObserveCue(Alert& alert)
  {
    const auto& cue = alert.cue();
    if (alert.observed_known && !SameCue(alert.observed, cue))
      ++alert.generation;
    alert.observed       = cue;
    alert.observed_known = true;
    if (!NotificationSoundFromName(cue.source))
      alert.custom = cue;
  }

  void RefreshAudioPage()
  {
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
    native::RequestPageRefresh();
#endif
  }

  bool ChooseFile(Alert& alert)
  {
    if (s_pending || !alert.available())
      return false;
    if (!s_picker_available && FrameDeliveryAvailable())
      s_picker_available = RegisterFrameCallback(PollFilePicker, "AudioFilePicker");
    if (!s_picker_available || !FrameDeliveryAvailable()) {
      alert.status = "File picker unavailable";
      RefreshAudioPage();
      return false;
    }
    try {
      ObserveCue(alert);
      s_pending_observation = alert.cue();
      s_pending_generation  = alert.generation;
      s_picker              = OpenAudioFilePicker();
      s_pending             = &alert;
      s_discard_pending     = false;
      alert.status          = "Choosing a file...";
      s_refresh_requested   = true;
      return true;
    } catch (...) {
      s_pending_observation = {};
      alert.status          = "Could not open the file picker";
      s_refresh_requested   = true;
      return false;
    }
  }

  void PollFilePicker()
  {
    if (s_refresh_requested) {
      s_refresh_requested = false;
      RefreshAudioPage();
    }
    if (!s_pending)
      return;
    auto& alert = *s_pending;
    try {
      if (s_picker.valid()) {
        if (s_picker.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
          return;
        auto path = s_picker.get();
        if (path.empty()) {
          alert.status          = "Selection canceled";
          s_pending             = nullptr;
          s_pending_observation = {};
          RefreshAudioPage();
          return;
        }
        s_loading    = RunFilePickerTask([path = std::move(path)] { return LoadNotificationAudio(path, {}); });
        alert.status = "Loading sound...";
        RefreshAudioPage();
      }
      if (!s_loading.valid() || s_loading.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
      auto cue = s_loading.get();
      if (alert.available())
        ObserveCue(alert);
      const bool stale = s_discard_pending || alert.generation != s_pending_generation;
      ++alert.generation;
      s_pending = nullptr;
      if (!cue.enabled()) {
        alert.status = "Could not load sound (see log)";
      } else if (alert.available() && !stale && SameCue(alert.cue(), s_pending_observation)) {
        alert.custom         = cue;
        alert.cue()          = std::move(cue);
        alert.observed       = alert.cue();
        alert.observed_known = true;
        alert.status.clear();
        if (alert.fleet)
          RefreshFleetNotificationAudio();
        runtime_config::SaveSetting("audio", alert.key.c_str(), alert.cue().source);
      } else {
        alert.status = "Selection discarded; the alert changed or became unavailable";
      }
    } catch (const std::exception& error) {
      spdlog::warn("[NotifyAudio] File selection failed: {}", error.what());
      alert.status = "Could not load sound (see log)";
      s_pending    = nullptr;
    } catch (...) {
      spdlog::warn("[NotifyAudio] File selection failed: unknown error");
      alert.status = "Could not load sound (see log)";
      s_pending    = nullptr;
    }
    if (!s_pending)
      s_pending_observation = {};
    RefreshAudioPage();
  }

  void Add(PageCatalog& catalog, std::string key, std::string label, std::function<NotificationAudioCue&()> cue,
           std::function<bool()> available, bool fleet)
  {
    auto  owner     = std::make_unique<Alert>();
    auto& alert     = *owner;
    alert.key       = std::move(key);
    alert.label     = std::move(label);
    alert.cue       = std::move(cue);
    alert.available = std::move(available);
    alert.fleet     = fleet;
    std::vector<std::string> labels{"Off",   "Default", "Info", "Success", "Warning",
                                    "Alarm", "Arrival", "Soft", "Ping",    "Repair"};
    labels.push_back("Custom...");
    const auto page       = "community_mod.audio." + alert.key;
    auto       definition = PersistedDefinition<int>(
        ValueDefinition<int>{page + ".sound", "Sound",
                             [&alert] {
                               if (!alert.available())
                                 return ValueReadResult<int>{};
                               ObserveCue(alert);
                               const auto builtin = NotificationSoundFromName(alert.cue().source);
                               return ValueReadResult<int>::Known(builtin ? static_cast<int>(*builtin) : kCustom,
                                                                  alert.generation);
                             },
                             [&alert](int value, std::uint64_t generation) {
                               if (generation != alert.generation || !alert.available() || value < 0 || value > kCustom)
                                 return ApplyResult::Rejected;
                               if (value == kCustom && alert.custom.source == "none") {
                                 ChooseFile(alert);
                                 // Admission is not a verified cue change. Keep the actual current cue
                                 // selected until owned decoding finishes; never persist phantom custom.
                                 return ApplyResult::Rejected;
                               }
                               if (s_pending == &alert)
                                 s_discard_pending = true;
                               // Reuse the prepared TOML clip. No file loading/decoding on the UI thread.
                               alert.cue()          = value == kCustom
                                                          ? alert.custom
                                                          : NotificationAudioCue(static_cast<NotificationSound>(value));
                               alert.observed       = alert.cue();
                               alert.observed_known = true;
                               alert.status.clear();
                               if (alert.fleet)
                                 RefreshFleetNotificationAudio();
                               return ApplyResult::Applied;
                             }},
        {"audio", alert.key, "Choose a built-in sound or a custom audio file.", "", ApplyTiming::Live}, 0,
        [&alert](const int& value) -> config_edit::Value {
          return value == kCustom ? alert.custom.source
                                  : std::string(NotificationSoundName(static_cast<NotificationSound>(value)));
        });
    // File selection is asynchronous; the completed source is saved by PollFilePicker.
    auto persist       = definition.persist;
    definition.persist = [&alert, persist](const int& value) {
      // An explicit reset/retry to an unchanged default is still newer user intent.
      if (s_pending == &alert)
        s_discard_pending = true;
      persist(value);
    };
    alert.choice  = std::make_unique<ChoiceSetting>(std::move(definition), std::move(labels));
    alert.preview = {page + ".preview", "Preview sound",
                     [&alert](std::size_t) {
                       return ActionSetting::Presentation{"Preview sound", "Play", "",
                                                          s_pending != &alert && alert.available()
                                                              && alert.cue().enabled()};
                     },
                     [&alert](std::size_t) {
                       if (s_pending != &alert && alert.available() && alert.cue().enabled())
                         PlayNotificationSound(alert.cue());
                     }};
    alert.choose  = {page + ".custom", "Custom sound",
                     [&alert](std::size_t) {
                      std::string value = alert.status;
                      if (value.empty() && alert.custom.source != "none") {
                        const auto utf8 = std::u8string_view(
                            reinterpret_cast<const char8_t*>(alert.custom.source.data()), alert.custom.source.size());
                        const auto filename = std::filesystem::path(utf8).filename().u8string();
                        value.assign(reinterpret_cast<const char*>(filename.data()), filename.size());
                      }
                      return ActionSetting::Presentation{"Custom sound", "Choose file...", value,
                                                         FrameDeliveryAvailable() && !s_pending && alert.available()};
                     },
                     [&alert](std::size_t) { ChooseFile(alert); }};
    catalog.AddPage(page, alert.label, "community_mod.audio");
    catalog.SetSummary(page, [&alert] {
      if (!alert.available())
        return std::string("Unavailable");
      const auto builtin = NotificationSoundFromName(alert.cue().source);
      return builtin ? (*builtin == NotificationSound::kNone ? std::string("Off")
                                                             : std::string(NotificationSoundName(*builtin)))
                     : std::string("Custom file");
    });
    catalog.AddAction(page, alert.preview);
    catalog.AddAction(page, alert.choose);
    catalog.AddChoice(page, *alert.choice);
    s_alerts.push_back(std::move(owner));
  }
} // namespace

void RegisterAudioAlertPages(PageCatalog& catalog)
{
  if (!s_alerts.empty())
    return;
  // Shared polling is admitted lazily by explicit ChooseFile intent.
  catalog.AddPage("community_mod.audio", "Audio Alerts", "community_mod.settings");
  s_coalescing = std::make_unique<ChoiceSetting>(
      PersistedDefinition<int>(
          ValueDefinition<int>{
              "community_mod.audio.coalescing", "Audio coalescing",
              [] { return ValueReadResult<int>::Known(static_cast<int>(NotificationAudioCoalescing()), 1); },
              [](int value, std::uint64_t generation) {
                if (generation != 1 || value < 0 || value > 2)
                  return ApplyResult::Rejected;
                const auto mode                = static_cast<AudioCoalescing>(value);
                Config::Get().audio.coalescing = mode;
                SetNotificationAudioCoalescing(mode);
                return ApplyResult::Applied;
              }},
          {"audio", "coalescing", "How concurrent audio requests are combined.", "", ApplyTiming::Live},
          static_cast<int>(AudioCoalescing::Same),
          [](const int& value) -> config_edit::Value {
            return std::string(audio_coalescing_name(static_cast<AudioCoalescing>(value)));
          }),
      std::vector<std::string>{"None - play every request", "Same - absorb repeats of the playing sound",
                               "All - absorb all requests while a sound plays"});
  catalog.AddChoice("community_mod.audio", *s_coalescing);
  Add(
      catalog, "alert_incoming_player_attack", "Incoming player attack (ships)",
      []() -> NotificationAudioCue& { return Config::Get().audio.alert_incoming_player_attack; },
      IncomingAudioAvailable, false);
  static BooleanSetting incoming_notification(PersistedDefinition<bool>(
      {"community_mod.ui.notify_incoming_player_attack", "Desktop notification",
       [] {
         return IncomingAudioAvailable() ? ReadResult::Known(Config::Get().ui.notify_incoming_player_attack, 1)
                                         : ReadResult{};
       },
       [](bool value, std::uint64_t generation) {
         if (generation != 1 || !IncomingAudioAvailable())
           return ApplyResult::Rejected;
         Config::Get().ui.notify_incoming_player_attack = value;
#if __APPLE__
         if (value)
           InitializeDesktopNotifications();
#endif
         return ApplyResult::Applied;
       }},
      {"ui", "notify_incoming_player_attack", "Show a desktop notification for incoming player attacks.", "",
       ApplyTiming::Live},
      false));
  catalog.AddBoolean("community_mod.audio.alert_incoming_player_attack", incoming_notification);
  struct ToastEntry {
    const char*          key;
    const char*          label;
    NotificationAudioCue Config::Audio::* member;
  };
  for (auto entry : {ToastEntry{"alert_victory", "Battle victory", &Config::Audio::alert_victory},
                     ToastEntry{"alert_defeat", "Battle defeat", &Config::Audio::alert_defeat},
                     ToastEntry{"alert_armada_created", "Armada created", &Config::Audio::alert_armada_created},
                     ToastEntry{"alert_armada_battle_won", "Armada victory", &Config::Audio::alert_armada_battle_won},
                     ToastEntry{"alert_armada_battle_lost", "Armada defeat", &Config::Audio::alert_armada_battle_lost}})
    Add(
        catalog, entry.key, entry.label,
        [entry]() -> NotificationAudioCue& { return Config::Get().audio.*(entry.member); }, ToastAudioAvailable, false);
  constexpr const char* labels[]{
      "Arrived in system", "Arrived at destination",    "Started mining", "Node depleted", "Docked",
      "Repair complete",   "Miner over protected cargo"};
  constexpr std::array<NotificationAudioCue Config::Audio::*, 7> fleet_cues{
      &Config::Audio::alert_fleet_arrived_in_system,
      &Config::Audio::alert_fleet_arrived_at_destination,
      &Config::Audio::alert_fleet_started_mining,
      &Config::Audio::alert_fleet_node_depleted,
      &Config::Audio::alert_fleet_docked,
      &Config::Audio::alert_fleet_repair_complete,
      &Config::Audio::alert_fleet_miner_opc};
  for (const auto& entry : kFleetNotificationCatalog) {
    const auto kind  = entry.kind;
    const auto index = static_cast<std::size_t>(kind);
    Add(
        catalog, std::string(entry.audio_config_name), labels[index],
        [member = fleet_cues[index]]() -> NotificationAudioCue& { return Config::Get().audio.*member; },
        [kind] { return FleetNotificationAudioAvailable(kind); }, true);
  }
}
} // namespace mod_settings
