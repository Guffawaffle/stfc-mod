#include "audio_alerts.h"
#include "config.h"
#include "persisted_setting.h"
#include "defaultconfig.h"
#include "patches/fleet_notification_settings.h"
#include "patches/incoming_player_attack.h"
#if __APPLE__
#include "patches/notification_desktop_mac.h"
#endif
#include "patches/runtime_config.h"
#include "patches/screen_update_hook.h"
#include "audio_file_picker.h"
#include "audio_file_tasks.h"
#include "native/action_widgets.h"
#include <memory>
#include <chrono>
#include <spdlog/spdlog.h>

namespace mod_settings
{
namespace
{
struct Alert {
  std::string key, label;
  std::function<NotificationAudioCue&()> cue;
  std::function<bool()> available;
  bool fleet = false;
  NotificationAudioCue custom;
  std::uint64_t generation = 1;
  std::unique_ptr<ChoiceSetting> choice;
  ActionSetting preview;
  ActionSetting choose;
  std::string status;
};
std::vector<std::unique_ptr<Alert>> s_alerts;
std::unique_ptr<ChoiceSetting> s_coalescing;
constexpr int kCustom = static_cast<int>(NotificationSound::Count);
Alert* s_pending = nullptr;
bool s_picker_available = false;
bool s_refresh_requested = false;
auto& s_picker = FileTasks().picker;
auto& s_loading = FileTasks().loading;

void RefreshAudioPage()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  native::RefreshActions();
#endif
}

bool ChooseFile(Alert& alert)
{
  if (!s_picker_available || s_pending || !alert.available()) return false;
  try {
    s_picker = OpenAudioFilePicker();
    s_pending = &alert;
    ++alert.generation;
    alert.status = "Choosing a file...";
    s_refresh_requested = true;
    return true;
  } catch (...) {
    alert.status = "Could not open the file picker";
    s_refresh_requested = true;
    return false;
  }
}

void PollFilePicker()
{
  if (s_refresh_requested) {
    s_refresh_requested = false;
    RefreshAudioPage();
  }
  if (!s_pending) return;
  auto& alert = *s_pending;
  try {
    if (s_picker.valid()) {
      if (s_picker.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
      auto path = s_picker.get();
      if (path.empty()) {
        alert.status = "Selection canceled";
        s_pending = nullptr;
        RefreshAudioPage();
        return;
      }
      s_loading = RunAudioTask([path = std::move(path)] {
        return notification_audio_load(path, {});
      });
      alert.status = "Loading sound...";
      RefreshAudioPage();
    }
    if (!s_loading.valid() || s_loading.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    auto cue = s_loading.get();
    ++alert.generation;
    s_pending = nullptr;
    if (!cue.enabled()) {
      alert.status = "Could not load sound (see log)";
    } else if (alert.available()) {
      alert.custom = cue;
      alert.cue() = std::move(cue);
      alert.status.clear();
      if (alert.fleet) RefreshFleetNotificationAudio();
      runtime_config::SaveSetting("audio", alert.key.c_str(), alert.cue().source);
    }
  } catch (const std::exception& error) {
    spdlog::warn("[NotifyAudio] File selection failed: {}", error.what());
    alert.status = "Could not load sound (see log)";
    s_pending = nullptr;
  } catch (...) {
    spdlog::warn("[NotifyAudio] File selection failed: unknown error");
    alert.status = "Could not load sound (see log)";
    s_pending = nullptr;
  }
  RefreshAudioPage();
}

void Add(PageCatalog& catalog, std::string key, std::string label,
         std::function<NotificationAudioCue&()> cue, std::function<bool()> available, bool fleet)
{
  auto owner = std::make_unique<Alert>();
  auto& alert = *owner;
  alert.key = std::move(key);
  alert.label = std::move(label);
  alert.cue = std::move(cue);
  alert.available = std::move(available);
  alert.fleet = fleet;
  const bool has_custom = !notification_sound_from_name(alert.cue().source).has_value();
  if (has_custom) alert.custom = alert.cue();
  std::vector<std::string> labels{"Off", "Default", "Info", "Success", "Warning", "Alarm", "Arrival", "Soft", "Ping", "Repair"};
  labels.push_back("Custom...");
  const auto page = "community_mod.audio." + alert.key;
  auto definition = PersistedDefinition<int>(ValueDefinition<int>{page + ".sound", "Sound",
      [&alert] {
        if (!alert.available()) return ValueReadResult<int>{};
        if (s_pending == &alert) return ValueReadResult<int>::Known(kCustom, alert.generation);
        const auto builtin = notification_sound_from_name(alert.cue().source);
        return ValueReadResult<int>::Known(builtin ? static_cast<int>(*builtin) : kCustom, alert.generation);
      },
      [&alert](int value, std::uint64_t generation) {
        if (generation != alert.generation || !alert.available() || s_pending == &alert || value < 0 || value > kCustom)
          return ApplyResult::Rejected;
        if (value == kCustom && alert.custom.source == "none")
          return ChooseFile(alert) ? ApplyResult::Applied : ApplyResult::Rejected;
        // Reuse the prepared TOML clip. No file loading/decoding on the UI thread.
        alert.cue() = value == kCustom ? alert.custom : NotificationAudioCue(static_cast<NotificationSound>(value));
        alert.status.clear();
        if (alert.fleet) RefreshFleetNotificationAudio();
        return ApplyResult::Applied;
      }}, {"audio", alert.key, "Choose a built-in sound or a custom audio file.", "", ApplyTiming::Live}, 0,
      [&alert](const int& value) -> config_edit::Value {
        return value == kCustom ? alert.custom.source : std::string(notification_sound_name(static_cast<NotificationSound>(value)));
      });
  // File selection is asynchronous; the completed source is saved by PollFilePicker.
  auto persist = definition.persist;
  auto status = definition.save_status;
  definition.persist = [&alert, persist](const int& value) { if (s_pending != &alert) persist(value); };
  definition.save_status = [&alert, status](const int& value) { return s_pending == &alert ? SaveStatus{} : status(value); };
  alert.choice = std::make_unique<ChoiceSetting>(std::move(definition), std::move(labels));
  alert.preview = {page + ".preview", "Preview sound",
      [&alert](std::size_t) {
        return ActionSetting::Presentation{"Preview sound", "Play", "",
                                           s_pending != &alert && alert.available() && alert.cue().enabled()};
      },
      [&alert](std::size_t) {
        if (s_pending != &alert && alert.available() && alert.cue().enabled()) notification_audio_play(alert.cue());
      }};
  alert.choose = {page + ".custom", "Custom sound",
      [&alert](std::size_t) {
        std::string value = alert.status;
        if (value.empty() && alert.custom.source != "none") {
          const auto filename = std::filesystem::u8path(alert.custom.source).filename().u8string();
          value.assign(reinterpret_cast<const char*>(filename.data()), filename.size());
        }
        return ActionSetting::Presentation{"Custom sound", "Choose file...", value,
                                           s_picker_available && !s_pending && alert.available()};
      },
      [&alert](std::size_t) { ChooseFile(alert); }};
  catalog.AddPage(page, alert.label, "community_mod.audio");
  catalog.SetSummary(page, [&alert] {
    if (!alert.available()) return std::string("Unavailable");
    const auto builtin = notification_sound_from_name(alert.cue().source);
    return builtin ? (*builtin == NotificationSound::None ? std::string("Off") : std::string(notification_sound_name(*builtin)))
                   : std::string("Custom file");
  });
  catalog.AddAction(page, alert.preview);
  catalog.AddAction(page, alert.choose);
  catalog.AddChoice(page, *alert.choice);
  s_alerts.push_back(std::move(owner));
}
}

void RegisterAudioAlertPages(PageCatalog& catalog)
{
  if (!s_alerts.empty()) return;
  s_picker_available = install_screen_manager_update_hook() && register_screen_manager_update_callback(PollFilePicker);
  catalog.AddPage("community_mod.audio", "Audio Alerts", "community_mod.settings");
  s_coalescing = std::make_unique<ChoiceSetting>(PersistedDefinition<int>(ValueDefinition<int>{
      "community_mod.audio.coalescing", "Audio coalescing",
      [] { return ValueReadResult<int>::Known(static_cast<int>(notification_audio_coalescing()), 1); },
      [](int value, std::uint64_t generation) {
        if (generation != 1 || value < 0 || value > 2) return ApplyResult::Rejected;
        const auto mode = static_cast<AudioCoalescing>(value);
        notification_audio_set_coalescing(mode);
        return ApplyResult::Applied;
      }}, {"audio", "coalescing", "How concurrent audio requests are combined.", "", ApplyTiming::Live},
      static_cast<int>(AudioCoalescing::Same), [](const int& value) -> config_edit::Value { return std::string(audio_coalescing_name(static_cast<AudioCoalescing>(value))); }),
      std::vector<std::string>{"None - play every request", "Same - absorb repeats of the playing sound",
                                  "All - absorb all requests while a sound plays"});
  catalog.AddChoice("community_mod.audio", *s_coalescing);
  Add(catalog, "alert_incoming_player_attack", "Incoming player attack (ships)",
      []() -> NotificationAudioCue& { return Config::Get().alert_incoming_player_attack; },
      IncomingPlayerAttackAvailable, false);
  static BooleanSetting incoming_notification(PersistedDefinition<bool>({"community_mod.ui.notify_incoming_player_attack", "Desktop notification",
      [] {
        return IncomingPlayerAttackAvailable() ? ReadResult::Known(Config::Get().notify_incoming_player_attack, 1)
                                              : ReadResult{};
      },
      [](bool value, std::uint64_t generation) {
        if (generation != 1 || !IncomingPlayerAttackAvailable()) return ApplyResult::Rejected;
        Config::Get().notify_incoming_player_attack = value;
#if __APPLE__
        if (value) notification_desktop_mac_init();
#endif
        return ApplyResult::Applied;
      }}, {"ui", "notify_incoming_player_attack", "Show a desktop notification for incoming player attacks.", "", ApplyTiming::Live}, DefaultConfig::UI::notify_incoming_player_attack));
  catalog.AddBoolean("community_mod.audio.alert_incoming_player_attack", incoming_notification);
  struct ToastEntry { const char* key; const char* label; NotificationAudioCue Config::*member; };
  for (auto entry : {ToastEntry{"alert_victory", "Battle victory", &Config::alert_victory},
                     ToastEntry{"alert_defeat", "Battle defeat", &Config::alert_defeat},
                     ToastEntry{"alert_armada_created", "Armada created", &Config::alert_armada_created},
                     ToastEntry{"alert_armada_battle_won", "Armada victory", &Config::alert_armada_battle_won},
                     ToastEntry{"alert_armada_battle_lost", "Armada defeat", &Config::alert_armada_battle_lost}})
    Add(catalog, entry.key, entry.label, [entry]() -> NotificationAudioCue& { return Config::Get().*(entry.member); },
        ToastAudioAvailable, false);
  constexpr const char* labels[]{"Arrived in system", "Arrived at destination", "Started mining", "Node depleted",
                                 "Docked", "Repair complete", "Miner over protected cargo"};
  for (const auto& entry : kFleetNotificationCatalog) {
    const auto kind = entry.kind;
    const auto index = static_cast<std::size_t>(kind);
    Add(catalog, std::string(entry.audio_config_name), labels[index],
        [index]() -> NotificationAudioCue& { return Config::Get().alert_fleet_events[index]; },
        [kind] { return FleetNotificationAudioAvailable(kind); }, true);
  }
}
}
