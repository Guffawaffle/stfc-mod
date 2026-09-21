#include "audio_alerts.h"
#include "config.h"
#include "patches/fleet_notification_settings.h"
#include "patches/runtime_config.h"
#include "patches/screen_update_hook.h"
#include "audio_file_picker.h"
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
  std::unique_ptr<ChoiceSetting> choice;
  ActionSetting preview;
  ActionSetting choose;
  std::string status;
};
std::vector<std::unique_ptr<Alert>> s_alerts;
constexpr int kCustom = static_cast<int>(NotificationSound::Count);
Alert* s_pending = nullptr;
bool s_picker_available = false;
bool s_refresh_requested = false;
std::future<std::string> s_picker;
std::future<NotificationAudioCue> s_loading;

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
      s_loading = std::async(std::launch::async, [path = std::move(path)] {
        return notification_audio_load(path, {});
      });
      alert.status = "Loading sound...";
      RefreshAudioPage();
    }
    if (!s_loading.valid() || s_loading.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    auto cue = s_loading.get();
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
  alert.choice = std::make_unique<ChoiceSetting>(ValueDefinition<int>{page + ".sound", "Sound",
      [&alert] {
        if (!alert.available()) return ValueReadResult<int>{};
        if (s_pending == &alert) return ValueReadResult<int>::Known(kCustom, 1);
        const auto builtin = notification_sound_from_name(alert.cue().source);
        return ValueReadResult<int>::Known(builtin ? static_cast<int>(*builtin) : kCustom, 1);
      },
      [&alert](int value, std::uint64_t generation) {
        if (generation != 1 || !alert.available() || s_pending == &alert || value < 0 || value > kCustom)
          return ApplyResult::Rejected;
        if (value == kCustom && alert.custom.source == "none")
          return ChooseFile(alert) ? ApplyResult::Applied : ApplyResult::Rejected;
        // Reuse the prepared TOML clip. No file loading/decoding on the UI thread.
        alert.cue() = value == kCustom ? alert.custom : NotificationAudioCue(static_cast<NotificationSound>(value));
        alert.status.clear();
        if (alert.fleet) RefreshFleetNotificationAudio();
        runtime_config::SaveSetting("audio", alert.key.c_str(), alert.cue().source);
        return ApplyResult::Applied;
      }}, std::move(labels));
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
