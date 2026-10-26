#include "settings/setting_metadata.h"
#include <stdexcept>
#ifdef CONFIG_RUNTIME_TEST
#include CONFIG_RUNTIME_TEST // Isolated fixture substitutes Unity/worker boundaries only.
#else
#include "config.h"
#include "file.h"
#include "mod_state.h"
#include "runtime_config.h"
#include "runtime_config_writer.h"
#include "settings/basic_gameplay_settings.h"
#include <spdlog/spdlog.h>

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
#include "features/features.h"
#include "frame.h"
#include "installation.h"
#include <unity/application.h>
#if _WIN32
#include <Windows.h>
#endif
#include <il2cpp/hook.h>
#endif
#endif

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)

namespace
{
// Installed hooks live until process exit. Retain this one control object rather
// than joining a worker from DLL teardown. No worker is started during Configure.
config_edit::RuntimeConfigWriter* writer    = nullptr;
bool                              available = false;
std::atomic<std::uintptr_t>       owner{0};
std::atomic_bool                  forcing{false};
std::mutex                        lifecycle;
bool                              draining = false, stopped = false, resume = false;
std::uint64_t                     vote       = 0;
thread_local unsigned             quit_depth = 0;
void (*request_quit)(int)                    = nullptr;
void (*save_status_changed)()                = nullptr;
std::atomic_bool           persistence_unavailable{false};
std::atomic_bool           protection_incomplete{false};
bool                       reported_save_failure    = false;
std::uint64_t              reported_status_sequence = 0;
std::atomic<std::uint64_t> adapter_status_sequence{0};
using SettingKey = std::pair<std::string, std::string>;
std::map<SettingKey, config_edit::Value> unavailable_settings;
// Owner-thread intents are owned values, not pointers into the replaceable Config.
struct RequestedSetting {
  config_edit::Value value;
  std::uint64_t      revision     = 0;
  bool               session_only = false;
};
std::map<SettingKey, RequestedSetting> requested_settings;
#ifndef CONFIG_RUNTIME_TEST
std::optional<toml::table> startup_values;
#endif

// Stable identity for the lifetime of the Unity owner thread on either platform.
std::uintptr_t CurrentThreadToken()
{
  static thread_local char token;
  return reinterpret_cast<std::uintptr_t>(&token);
}

void Report(std::string_view section, std::string_view key, config_edit::Outcome result)
{
  const char* reason = "write failed";
  switch (result) {
    case config_edit::Outcome::Conflict:
      reason = "file changed externally";
      break;
    case config_edit::Outcome::InvalidDocument:
      reason = "invalid TOML";
      break;
    case config_edit::Outcome::Unsupported:
      reason = "unsupported setting representation";
      break;
    default:
      break;
  }
  spdlog::warn("Could not persist {}.{}: {}; live setting is unchanged", section, key, reason);
}

bool WantsQuit(auto original)
{
  struct Depth {
    Depth()
    { ++quit_depth; }
    ~Depth()
    { --quit_depth; }
  } depth;
  std::uint64_t this_vote;
  {
    std::lock_guard lock(lifecycle);
    this_vote = ++vote;
  }
  const bool      allows = original();
  std::lock_guard lock(lifecycle);
  if (!writer)
    return allows;
  if (stopped) {
    if (!allows) {
      // Unity may veto the resumed request. The surviving session must regain
      // saving after the old worker has fully terminated.
      stopped  = false;
      draining = true;
      resume   = false;
    }
    return allows;
  }
  // A later/nested game veto must not be overwritten by an older returning vote.
  if (this_vote == vote)
    resume = allows;
  if (allows) {
    mod_state::BeginQuitDrain();
    writer->Stop(false);
    // Close admission before checking: Submit shares lifecycle, so no later
    // request can race an idle exit. An already-deferred quit still observes
    // native thread exit through Update before resuming.
    if (!draining && !writer->HasWork() && mod_state::QuitDrained() && resume) {
      stopped = true;
      resume  = false;
      return true;
    }
    draining = true;
  }
  return false; // Resume only after observing native worker exit, even after failure.
}

void Update()
{
  std::uintptr_t unset = 0;
  owner.compare_exchange_strong(unset, CurrentThreadToken());
  if (forcing || owner != CurrentThreadToken() || quit_depth)
    return;
  const bool failed   = persistence_unavailable.load() || (writer && writer->HasFailures());
  const auto sequence = adapter_status_sequence.load() + (writer ? writer->StatusSequence() : 0);
  if (failed != reported_save_failure || sequence != reported_status_sequence) {
    reported_save_failure    = failed;
    reported_status_sequence = sequence;
    if (save_status_changed) {
      try {
        save_status_changed();
      } catch (...) { /* Presentation cannot interrupt shutdown. */
      }
    }
  }
  bool quit = false;
  {
    std::lock_guard lock(lifecycle);
    if (!draining || stopped || !writer || !writer->PollStopped() || !mod_state::QuitDrained())
      return;
    quit   = resume;
    resume = false; // Consume before Unity callbacks; never retry a genuine veto.
    if (!quit) {
      mod_state::ResumeAfterQuitVeto();
      draining = false;
      stopped  = false;
      if (!writer->Resume()) {
        available = false;
        persistence_unavailable.store(true);
        ++adapter_status_sequence;
      }
      return;
    }
    stopped = true;
  }
  if (quit)
    request_quit(0);
}

bool EnsureDelivery()
{
#ifdef CONFIG_RUNTIME_TEST
  return install_screen_manager_update_hook() && register_screen_manager_update_callback(Update);
#else
  // Physical installation and registration are testable during initialization;
  // dispatch readiness intentionally stays false until the host finishes.
  return EnsureFrameTick().Installed() && RegisterFrameCallback(Update, "RuntimeConfig");
#endif
}

#if _WIN32
DWORD WINAPI FinishForceClose(void* handle)
{
  WaitForSingleObject(handle, 500);
  CloseHandle(handle);
  TerminateProcess(GetCurrentProcess(), 1);
  return 0;
}
#endif
} // namespace
#elif _WIN32
#include <Windows.h>
#endif

namespace runtime_config
{
bool PrepareFrame()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  return EnsureDelivery();
#else
  return false;
#endif
}

bool SetSaveStatusObserver(void (*observer)())
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  if (save_status_changed && save_status_changed != observer)
    return false;
  // Status must also update if persistence/quit-hook validation failed, or no
  // writer was configured. Registration uses the existing idempotent dispatcher.
  if (!observer || !EnsureDelivery())
    return false;
  save_status_changed = observer;
  return true;
#else
  (void)observer;
  return false;
#endif
}
bool HasSaveFailures() noexcept
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  return persistence_unavailable.load() || (writer && writer->HasFailures());
#else
  return false;
#endif
}
std::uint64_t StatusSequence() noexcept
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  return adapter_status_sequence.load() + (writer ? writer->StatusSequence() : 0);
#else
  return 0;
#endif
}

ReloadSnapshot CaptureReload()
{
  ReloadSnapshot snapshot;
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  if (owner && owner != CurrentThreadToken())
    throw std::logic_error("Configuration reload requires the owner thread.");
  if (protection_incomplete.load())
    throw std::runtime_error("A live setting could not be retained for reload protection.");
  std::lock_guard lock(lifecycle);
  snapshot.reserve(requested_settings.size());
  for (const auto& [key, intent] : requested_settings) {
    bool protect = !writer || !intent.revision || unavailable_settings.contains(key);
    if (!protect) {
      const auto status = writer->GetStatus(key.first, key.second);
      protect =
          status.pending || status.revision != intent.revision || status.subject != intent.value
          || (status.outcome != config_edit::Outcome::Saved && status.outcome != config_edit::Outcome::AlreadySaved);
    }
    if (protect)
      snapshot.push_back({key.first, key.second, intent.value});
  }
#endif
  return snapshot;
}

bool RegisterSetting(const char* section, const char* key) noexcept
{
  try {
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
    if (!section || !key || !*section || !*key)
      return false;
    std::lock_guard lock(lifecycle);
    if (!writer || draining)
      return false;
    std::optional<config_edit::Value> initial;
#ifndef CONFIG_RUNTIME_TEST
    if (!startup_values)
      return false;
    const auto node = (*startup_values)[section][key];
    if (node.is_boolean())
      initial = *node.value<bool>();
    else if (node.is_string())
      initial = *node.value<std::string>();
    else if (node.is_integer())
      initial = *node.value<std::int64_t>();
    else if (node.is_floating_point())
      initial = *node.value<double>();
#endif
    return writer->Register(section, key, std::move(initial));
#else
    (void)section;
    (void)key;
#endif
  } catch (...) {
  }
  return false;
}
mod_settings::SaveStatus GetSaveStatus(const char* section, const char* key, const config_edit::Value& current) noexcept
{
  using namespace mod_settings;
  try {
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
    if (!section || !key)
      return {SaveState::Unavailable};
    std::lock_guard lock(lifecycle);
    if (const auto found = requested_settings.find(SettingKey{section, key});
        found != requested_settings.end() && found->second.session_only)
      return {SaveState::SessionOnly};
    if (const auto found = unavailable_settings.find(SettingKey{section, key}); found != unavailable_settings.end())
      return {current != found->second ? SaveState::SessionOnly : SaveState::Unavailable};
    if (!writer)
      return {};
    const auto status = writer->GetStatus(section, key);
    if (!status.revision)
      return {};
    if (status.subject != current)
      return {SaveState::SessionOnly, status.revision};
    if (status.pending)
      return {SaveState::Pending, status.revision};
    if (status.outcome == config_edit::Outcome::Saved || status.outcome == config_edit::Outcome::AlreadySaved)
      return {SaveState::Saved, status.revision};
    if (status.outcome == config_edit::Outcome::Conflict)
      return {SaveState::Conflict, status.revision};
    return {SaveState::Failed, status.revision};
#else
    (void)section;
    (void)key;
#endif
  } catch (...) {
    return {SaveState::Unavailable};
  }
  return {};
}
#ifndef CONFIG_RUNTIME_TEST
void Configure(const toml::table& loaded)
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  if (writer)
    return;
  try {
    startup_values = loaded;
    writer         = new config_edit::RuntimeConfigWriter(File::Path(File::Config()), Report);
    // Startup registration is independent of which optional feature hooks are
    // enabled. Never learn a comparison value when the first shortcut saves.
    if (!gameplay_settings::RegisterKeys(
            [](const gameplay_settings::Key& key) { return RegisterSetting(key.section, key.name); }))
      spdlog::warn("Some gameplay setting keys could not register for persistence");
  } catch (...) {
    persistence_unavailable.store(true);
    try {
      spdlog::warn("Runtime config persistence unavailable");
    } catch (...) {
    }
  }
#else
  (void)loaded;
#endif
}

bool Install()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  static InstallGuard installation;
  installation.Run(true, [] {
    if (!writer)
      return false;
    const auto wants = unity::Application::Class::StaticMethod<bool()>("Internal_ApplicationWantsToQuit");
    const auto quit  = unity::Application::Class::StaticMethod<void(std::int32_t)>("Quit");
    if (!wants || !quit || !EnsureDelivery())
      return false;
    request_quit = [](int code) { unity::Application::Quit(code); };
    il2cpp::HookSet hooks("RuntimeConfig");
    IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(wants), WantsQuit);
    available = hooks.Commit(); // Unsafe state propagates to M2's fatal host boundary.
    return available;
  });
  return installation.State() == InstallState::kInstalled;
#else
  return false;
#endif
}
#endif

void RetainSessionIntent(const char* section, const char* key, config_edit::Value value) noexcept
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  if (owner != CurrentThreadToken() || quit_depth || forcing || !section || !key || !*section || !*key)
    return;
  try {
    std::lock_guard lock(lifecycle);
    requested_settings.insert_or_assign(SettingKey{section, key}, RequestedSetting{std::move(value), 0, true});
    ++adapter_status_sequence;
  } catch (...) {
    // Losing the latest live intent makes a subsequent reload unsafe. This is
    // not a failed disk write: no persistence was requested by this operation.
    protection_incomplete.store(true);
  }
#else
  (void)section;
  (void)key;
  (void)value;
#endif
}

void SaveSetting(const char* section, const char* key, config_edit::Value value,
                 std::chrono::milliseconds delay) noexcept
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
  // Rejected callers must not manufacture a live intent that a later reload
  // would apply. Only the established owner can mutate either receipt map.
  if (owner != CurrentThreadToken() || quit_depth || forcing || !section || !key || !*section || !*key)
    return;
#endif
  bool retained = false;
  try {
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
    if (section && key && available && !forcing && owner == CurrentThreadToken() && !quit_depth) {
      std::lock_guard lock(lifecycle);
      if (!draining) {
        auto& intent =
            requested_settings.insert_or_assign(SettingKey{section, key}, RequestedSetting{value}).first->second;
        retained = true;
        if (const auto revision = writer->Submit(section, key, value, delay)) {
          intent.revision = revision;
          if (unavailable_settings.erase(SettingKey{section, key})) {
            persistence_unavailable.store(!unavailable_settings.empty());
            ++adapter_status_sequence;
          }
          return;
        }
        if (writer->HasFailure(section, key)) {
          if (unavailable_settings.erase(SettingKey{section, key})) {
            persistence_unavailable.store(!unavailable_settings.empty());
            ++adapter_status_sequence;
          }
          spdlog::warn("{}.{} changed for this session; runtime save submission failed", section, key);
          return; // The writer owns this failure and its eventual same-key recovery.
        }
      }
    }
    {
      std::lock_guard lock(lifecycle);
      if (section && key && *section && *key) {
        requested_settings.insert_or_assign(SettingKey{section, key}, RequestedSetting{value});
        retained = true;
      }
      unavailable_settings.insert_or_assign(SettingKey{section ? section : "", key ? key : ""}, std::move(value));
      ++adapter_status_sequence;
    }
    persistence_unavailable.store(true);
#else
    (void)value;
#endif
    static bool reported = false;
    if (!reported) {
      reported = true;
      spdlog::warn("{}.{} changed for this session; runtime persistence unavailable", section, key);
    }
  } catch (...) { /* Persistence must not interrupt the shortcut's live effect. */
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__) || defined(CONFIG_RUNTIME_TEST)
    persistence_unavailable.store(true);
    // An allocation/reporting failure may have prevented retention of the
    // newest live value. Refuse future reload rather than lose unknown intent.
    if (!retained)
      protection_incomplete.store(true);
#endif
  }
}

#if _WIN32
void ForceClose() noexcept
{
#if defined(_M_X64)
  if (writer && owner == CurrentThreadToken() && !quit_depth && writer->HasWork()) {
    forcing = true;
    writer->RequestCancelPending();
    HANDLE duplicate = nullptr;
    if (auto handle = writer->NativeHandle();
        handle
        && DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicate, SYNCHRONIZE, FALSE, 0)) {
      // Arm the independent deadline before taking any writer lock. A stalled
      // filesystem operation or Unity callback cannot prolong this best effort.
      if (auto closer = CreateThread(nullptr, 0, FinishForceClose, duplicate, 0, nullptr)) {
        CloseHandle(closer);
        writer->Stop(true);
        return;
      }
      CloseHandle(duplicate);
    }
  }
#endif
  TerminateProcess(GetCurrentProcess(), 1);
}
#endif
} // namespace runtime_config

#ifndef CONFIG_RUNTIME_TEST
// One named M2 installer; its shared callback uses mod.cc's sole frame anchor.
const Feature kRuntimeConfig{"RuntimeConfig", &Config::Patches::runtimeconfighooks, runtime_config::Install};
#endif
