#include "config.h"
#include "features/fleet_notification_settings.h"
#include "frame.h"
#include "runtime_config.h"
#include "settings/audio_alerts.h"
#include "settings/audio_file_picker.h"
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

namespace
{
using namespace mod_settings;
bool                      toast_ready = true, incoming_ready = true, fleet_ready = true, delivery_ready = true;
bool                      defer_picker = false, throw_picker = false;
std::promise<std::string> deferred_picker;
int                       config_reads = 0, registrations = 0, opens = 0, previews = 0;
const auto                owner_thread = std::this_thread::get_id();
std::atomic_int           loads{0};
std::mutex                load_mutex;
std::condition_variable   load_wake;
bool                      block_load = false, load_entered = false, release_load = false;
std::set<std::string>     registered;
std::map<std::string, config_edit::Value> saved;
ScreenManagerUpdateCallback               tick;
std::string                               nextPath;
AudioCoalescing                           coalescing = AudioCoalescing::Same;
void                                      Check(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}
void Drain(const std::function<bool()>& done)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!done() && std::chrono::steady_clock::now() < deadline) {
    tick();
    std::this_thread::yield();
  }
  Check(done(), "asynchronous file completion timed out");
}
} // namespace
Config& Config::Get()
{
  ++config_reads;
  static Config config;
  return config;
}
NotificationAudioCue LoadNotificationAudio(std::string_view path, const std::filesystem::path&)
{
  Check(std::this_thread::get_id() != owner_thread, "decoding ran on the UI owner");
  ++loads;
  {
    std::unique_lock lock(load_mutex);
    if (block_load) {
      load_entered = true;
      load_wake.notify_all();
      Check(load_wake.wait_for(lock, std::chrono::seconds(3), [] { return release_load; }),
            "bounded load release expired");
    }
  }
  if (path == "throw.wav")
    throw std::runtime_error("synthetic decode failure");
  NotificationAudioCue cue;
  cue.source = path;
  if (path == "bad.wav")
    return cue;
  cue.data = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{1});
  return cue;
}
void PlayNotificationSound(const NotificationAudioCue&)
{
  Check(std::this_thread::get_id() == owner_thread, "preview owner");
  ++previews;
}
AudioCoalescing NotificationAudioCoalescing()
{ return coalescing; }
void SetNotificationAudioCoalescing(AudioCoalescing value)
{ coalescing = value; }
bool IncomingPlayerAttackAvailable()
{ return incoming_ready; }
bool ToastAudioAvailable()
{ return toast_ready; }
bool FleetNotificationAudioAvailable(FleetNotificationKind)
{ return fleet_ready; }
void RefreshFleetNotificationAudio() {}
bool FrameDeliveryAvailable() noexcept
{ return delivery_ready; }
bool RegisterFrameCallback(ScreenManagerUpdateCallback callback, const char*) noexcept
{
  ++registrations;
  tick = callback;
  return true;
}
std::future<std::string> OpenAudioFilePicker()
{
  ++opens;
  if (std::exchange(throw_picker, false))
    throw std::runtime_error("synthetic dialog failure");
  if (defer_picker) {
    deferred_picker = {};
    return deferred_picker.get_future();
  }
  std::promise<std::string> promise;
  promise.set_value(nextPath);
  return promise.get_future();
}
namespace mod_settings::native
{
void RequestPageRefresh() {}
} // namespace mod_settings::native
namespace runtime_config
{
bool RegisterSetting(const char* section, const char* key) noexcept
{
  registered.insert(std::string(section) + "." + key);
  return true;
}
void SaveSetting(const char* section, const char* key, config_edit::Value value, std::chrono::milliseconds) noexcept
{
  const auto full = std::string(section) + "." + key;
  Check(registered.contains(full), "unregistered save");
  saved[full] = std::move(value);
}
mod_settings::SaveStatus GetSaveStatus(const char* section, const char* key, const config_edit::Value& value) noexcept
{
  const auto found = saved.find(std::string(section) + "." + key);
  return {found == saved.end()     ? mod_settings::SaveState::None
          : found->second == value ? mod_settings::SaveState::Saved
                                   : mod_settings::SaveState::SessionOnly};
}
} // namespace runtime_config
int main()
{
  PageCatalog catalog("community_mod.settings", "Settings");
  RegisterAudioAlertPages(catalog);
  Check(config_reads == 0 && registrations == 0 && opens == 0, "registration acquired config/native/file services");
  auto pages = catalog.Build();
  Check(registered.size() == 15, "audio keys/defaults not registered");
  Check(saved.empty(), "browsing saved audio");
  for (auto& page : pages)
    for (auto& item : page.items) {
      if (auto model = std::get_if<ChoiceSetting*>(&item)) {
        auto& state = (*model)->state();
        Check(state.metadata().persisted() && state.default_value().has_value(), "missing audio metadata/default");
        auto before = state.Observe();
        Check(state.SetFromUser((*before.state.value + 1) % 3, before).outcome == Outcome::AppliedVerified,
              "built-in choice rejected");
        Check(state.save_status().state == SaveState::Saved, "choice save status absent");
        Check(state.Reset(state.Observe()).outcome == Outcome::AppliedVerified || *state.Observe().state.value == 1,
              "default reset failed");
      }
    }
  const auto     page   = std::find_if(pages.begin(), pages.end(),
                                       [](const auto& p) { return p.id == "community_mod.audio.alert_victory"; });
  ChoiceSetting* choice = nullptr;
  ActionSetting* custom = nullptr;
  for (auto& item : page->items) {
    if (auto p = std::get_if<ChoiceSetting*>(&item))
      choice = *p;
    if (auto p = std::get_if<ActionSetting*>(&item); p && (*p)->identity.ends_with(".custom"))
      custom = *p;
  }
  nextPath = "first.wav";
  custom->invoke(0);
  Drain([] { return Config::Get().audio.alert_victory.source == "first.wav"; });
  auto reset = PrepareReset(choice->state());
  Check(reset.current(), "prepared reset unavailable");
  nextPath = "second.wav";
  custom->invoke(0);
  Drain([] { return Config::Get().audio.alert_victory.source == "second.wav"; });
  Check(!reset.current(), "same-index file replacement failed to invalidate stale reset");
  Check(std::get<std::string>(saved.at("audio.alert_victory")) == "second.wav", "completed custom source not saved");
  Check(choice->state().save_status().state == SaveState::Saved, "custom source save status stale");
  choice->state().Reset(choice->state().Observe());
  Check(Config::Get().audio.alert_victory.source == "none", "custom reset failed");
  auto            incoming_page   = std::find_if(pages.begin(), pages.end(), [](const auto& p) {
    return p.id == "community_mod.audio.alert_incoming_player_attack";
  });
  ChoiceSetting*  incoming_choice = nullptr;
  BooleanSetting* incoming_toggle = nullptr;
  for (auto& item : incoming_page->items) {
    if (auto p = std::get_if<ChoiceSetting*>(&item))
      incoming_choice = *p;
    if (auto p = std::get_if<BooleanSetting*>(&item))
      incoming_toggle = *p;
  }
  toast_ready = false;
  Check(incoming_ready && incoming_choice && incoming_toggle && !incoming_choice->state().Observe().state.known()
            && !incoming_toggle->Observe().state.known(),
        "incoming controls lost their actual Toasts owner gate");
  toast_ready                     = true;
  const auto before_preview_loads = loads.load();
  choice->state().SetFromUser(int(NotificationSound::kCount), choice->state().Observe());
  ActionSetting* preview = nullptr;
  for (auto& item : page->items)
    if (auto p = std::get_if<ActionSetting*>(&item); p && (*p)->identity.ends_with(".preview"))
      preview = *p;
  Check(preview && preview->Read(0).actionable(), "prepared cue preview unavailable");
  preview->invoke(0);
  Check(previews == 1 && loads.load() == before_preview_loads, "preview decoded/reopened a file");
  choice->state().Reset(choice->state().Observe());
  const auto before_cancel = saved;
  defer_picker             = true;
  custom->invoke(0);
  const auto once = opens;
  custom->invoke(0);
  Check(opens == once && !custom->Read(0).actionable(), "pending admission was not bounded");
  deferred_picker.set_value("");
  Drain([&] { return custom->Read(0).actionable(); });
  Check(saved == before_cancel && Config::Get().audio.alert_victory.source == "none",
        "cancel changed the cue or saved a preference");
  custom->invoke(0);
  Config::Get().audio.alert_victory = NotificationAudioCue(NotificationSound::kSuccess);
  choice->state().Observe();
  Config::Get().audio.alert_victory = NotificationAudioCue(NotificationSound::kNone);
  choice->state().Observe();
  deferred_picker.set_value("stale.wav");
  Drain([&] { return custom->Read(0).actionable(); });
  Check(Config::Get().audio.alert_victory.source == "none" && saved == before_cancel,
        "observed ABA generation admitted stale publication");
  custom->invoke(0);
  const auto reset_default = choice->state().Reset(choice->state().Observe());
  Check(reset_default.outcome == Outcome::Unchanged, "unchanged default reset fixture not prepared");
  const auto after_unchanged_reset = saved;
  deferred_picker.set_value("discard-after-reset.wav");
  Drain([&] { return custom->Read(0).actionable(); });
  Check(Config::Get().audio.alert_victory.source == "none" && saved == after_unchanged_reset,
        "unchanged reset did not revoke pending publication");
  {
    std::lock_guard lock(load_mutex);
    block_load   = true;
    load_entered = release_load = false;
  }
  custom->invoke(0);
  deferred_picker.set_value("slow.wav");
  tick();
  {
    std::unique_lock lock(load_mutex);
    Check(load_wake.wait_for(lock, std::chrono::seconds(2), [] { return load_entered; }), "load never entered");
  }
  choice->state().Reset(choice->state().Observe());
  const auto after_load_reset = saved;
  {
    std::lock_guard lock(load_mutex);
    release_load = true;
    block_load   = false;
  }
  load_wake.notify_all();
  Drain([&] { return custom->Read(0).actionable(); });
  Check(Config::Get().audio.alert_victory.source == "none" && saved == after_load_reset,
        "in-flight decoding overwrote reset");
  defer_picker = false;
  for (const auto path : {"bad.wav", "throw.wav"}) {
    nextPath       = path;
    const auto old = saved;
    custom->invoke(0);
    Drain([&] { return custom->Read(0).actionable(); });
    Check(Config::Get().audio.alert_victory.source == "none" && saved == old, "failed decode changed active cue");
  }
  throw_picker = true;
  custom->invoke(0);
  Check(custom->Read(0).actionable() && Config::Get().audio.alert_victory.source == "none",
        "dialog failure stranded the action");
  delivery_ready               = false;
  const auto no_delivery_opens = opens;
  custom->invoke(0);
  Check(opens == no_delivery_opens && !custom->Read(0).actionable(), "unavailable host admitted a file dialog");
  delivery_ready = true;
  Check(registrations == 1, "each file choice re-registered the shared frame callback");
  std::cout << "Audio settings passed: built-ins, defaults, save status, asynchronous files and stale reset guards\n";
}
