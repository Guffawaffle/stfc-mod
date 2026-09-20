#include "audio_alerts.h"
#include "config.h"
#include "patches/fleet_notification_settings.h"
#include "patches/runtime_config.h"
#include <memory>

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
};
std::vector<std::unique_ptr<Alert>> s_alerts;
constexpr int kCustom = static_cast<int>(NotificationSound::Count);

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
  if (has_custom) labels.push_back(alert.custom.enabled() ? "Custom file (TOML)" : "Custom file (unavailable)");
  const auto page = "community_mod.audio." + alert.key;
  alert.choice = std::make_unique<ChoiceSetting>(ValueDefinition<int>{page + ".sound", "Sound",
      [&alert] {
        if (!alert.available()) return ValueReadResult<int>{};
        const auto builtin = notification_sound_from_name(alert.cue().source);
        return ValueReadResult<int>::Known(builtin ? static_cast<int>(*builtin) : kCustom, 1);
      },
      [&alert, has_custom](int value, std::uint64_t generation) {
        if (generation != 1 || !alert.available() || value < 0 || value > kCustom || (value == kCustom && !has_custom))
          return ApplyResult::Rejected;
        // Reuse the prepared TOML clip. No file loading/decoding on the UI thread.
        alert.cue() = value == kCustom ? alert.custom : NotificationAudioCue(static_cast<NotificationSound>(value));
        if (alert.fleet) RefreshFleetNotificationAudio();
        runtime_config::SaveSetting("audio", alert.key.c_str(), alert.cue().source);
        return ApplyResult::Applied;
      }}, std::move(labels));
  alert.preview = {page + ".preview", "Preview sound",
      [&alert](std::size_t) {
        return ActionSetting::Presentation{"Preview sound", "Play", "", alert.available() && alert.cue().enabled()};
      },
      [&alert](std::size_t) {
        if (alert.available() && alert.cue().enabled()) notification_audio_play(alert.cue());
      }};
  catalog.AddPage(page, alert.label, "community_mod.audio");
  catalog.SetSummary(page, [&alert] {
    if (!alert.available()) return std::string("Unavailable");
    const auto builtin = notification_sound_from_name(alert.cue().source);
    return builtin ? (*builtin == NotificationSound::None ? std::string("Off") : std::string(notification_sound_name(*builtin)))
                   : std::string("Custom file");
  });
  catalog.AddAction(page, alert.preview);
  catalog.AddChoice(page, *alert.choice);
  s_alerts.push_back(std::move(owner));
}
}

void RegisterAudioAlertPages(PageCatalog& catalog)
{
  if (!s_alerts.empty()) return;
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
