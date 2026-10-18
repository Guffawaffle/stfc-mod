#include "config.h"
#include "settings/audio_alerts.h"
#include "settings/audio_file_picker.h"
#include "patches/runtime_config.h"
#include "patches/screen_update_hook.h"
#include "patches/fleet_notification_settings.h"
#include <map>
#include <set>
#include <stdexcept>
#include <thread>
#include <iostream>

namespace {
using namespace mod_settings;
std::set<std::string> registered;
std::map<std::string, config_edit::Value> saved;
ScreenManagerUpdateCallback tick;
std::string nextPath;
AudioCoalescing coalescing = AudioCoalescing::Same;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Drain(const std::function<bool()>& done) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!done() && std::chrono::steady_clock::now() < deadline) { tick(); std::this_thread::yield(); }
  Check(done(), "asynchronous file completion timed out");
}
}
Config::Config() { notify_incoming_player_attack = false; }
Config& Config::Get() { static Config config; return config; }
std::string_view notification_sound_name(NotificationSound sound) {
  static constexpr std::string_view names[]{"none","default","info","success","warning","alarm","arrival","soft","ping","repair"};
  return names[static_cast<int>(sound)];
}
std::optional<NotificationSound> notification_sound_from_name(std::string_view text) {
  for (int i=0;i<static_cast<int>(NotificationSound::Count);++i)
    if (notification_sound_name(static_cast<NotificationSound>(i)) == text) return static_cast<NotificationSound>(i);
  return {};
}
NotificationAudioCue notification_audio_load(std::string_view path, const std::filesystem::path&) {
  NotificationAudioCue cue; cue.source=path;
  cue.data=std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{1}); return cue;
}
void notification_audio_play(const NotificationAudioCue&) {}
AudioCoalescing notification_audio_coalescing() { return coalescing; }
void notification_audio_set_coalescing(AudioCoalescing value) { coalescing=value; }
bool IncomingPlayerAttackAvailable() { return true; }
bool ToastAudioAvailable() { return true; }
bool FleetNotificationAudioAvailable(FleetNotificationKind) { return true; }
void RefreshFleetNotificationAudio() {}
bool install_screen_manager_update_hook() { return true; }
bool register_screen_manager_update_callback(ScreenManagerUpdateCallback callback) { tick=callback; return true; }
std::future<std::string> OpenAudioFilePicker() { std::promise<std::string> promise; promise.set_value(nextPath); return promise.get_future(); }
namespace mod_settings::native { void RefreshActions() {} }
namespace runtime_config {
bool RegisterSetting(const char* section,const char* key) noexcept { registered.insert(std::string(section)+"."+key); return true; }
void SaveSetting(const char* section,const char* key,config_edit::Value value,std::chrono::milliseconds) noexcept {
  const auto full=std::string(section)+"."+key; Check(registered.contains(full),"unregistered save"); saved[full]=std::move(value);
}
mod_settings::SaveStatus GetSaveStatus(const char* section,const char* key,const config_edit::Value& value) noexcept {
  const auto found=saved.find(std::string(section)+"."+key);
  return {found==saved.end() ? mod_settings::SaveState::None : found->second==value ? mod_settings::SaveState::Saved : mod_settings::SaveState::SessionOnly};
}
}
int main() {
  PageCatalog catalog("community_mod.settings","Settings"); RegisterAudioAlertPages(catalog);
  auto pages=catalog.Build(); Check(registered.size()==15,"audio keys/defaults not registered"); Check(saved.empty(),"browsing saved audio");
  for (auto& page:pages) for (auto& item:page.items) {
    if (auto model=std::get_if<ChoiceSetting*>(&item)) {
      auto& state=(*model)->state(); Check(state.metadata().persisted() && state.default_value().has_value(),"missing audio metadata/default");
      auto before=state.Observe(); Check(state.SetFromUser((*before.state.value + 1) % 3,before).outcome==Outcome::AppliedVerified,"built-in choice rejected");
      Check(state.save_status().state==SaveState::Saved,"choice save status absent");
      Check(state.Reset(state.Observe()).outcome==Outcome::AppliedVerified || *state.Observe().state.value==1,"default reset failed");
    }
  }
  const auto page=std::find_if(pages.begin(),pages.end(),[](const auto& p){return p.id=="community_mod.audio.alert_victory";});
  ChoiceSetting* choice=nullptr; ActionSetting* custom=nullptr;
  for (auto& item:page->items) { if (auto p=std::get_if<ChoiceSetting*>(&item)) choice=*p; if (auto p=std::get_if<ActionSetting*>(&item); p && (*p)->identity.ends_with(".custom")) custom=*p; }
  nextPath="first.wav"; custom->invoke(0); Drain([]{return Config::Get().alert_victory.source=="first.wav";});
  auto reset=PrepareReset(choice->state()); Check(reset.current(),"prepared reset unavailable");
  nextPath="second.wav"; custom->invoke(0); Drain([]{return Config::Get().alert_victory.source=="second.wav";});
  Check(!reset.current(),"same-index file replacement failed to invalidate stale reset");
  Check(std::get<std::string>(saved.at("audio.alert_victory"))=="second.wav","completed custom source not saved");
  Check(choice->state().save_status().state==SaveState::Saved,"custom source save status stale");
  choice->state().Reset(choice->state().Observe()); Check(Config::Get().alert_victory.source=="none","custom reset failed");
  std::cout << "Audio settings passed: built-ins, defaults, save status, asynchronous files and stale reset guards\n";
}
