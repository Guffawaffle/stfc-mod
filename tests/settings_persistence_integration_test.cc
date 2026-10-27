#define CONFIG_RUNTIME_TEST "runtime_config_disk_fixture.h"
#include "../src/mod/runtime_config.cc"
#include "config_save.h"
#include "config_save_fixture.h"
#include "runtime_config.h"
#include "settings/persisted_setting.h"
#include <cassert>
#include <fstream>
#include <iostream>

namespace
{
std::mutex              disk_gate_mutex;
std::condition_variable disk_gate_cv;
bool                    disk_gate_armed = false, disk_gate_entered = false, disk_gate_released = false;
void                    ArmDiskGate()
{
  std::lock_guard lock(disk_gate_mutex);
  disk_gate_armed   = true;
  disk_gate_entered = disk_gate_released = false;
}
void AwaitDiskGate()
{
  std::unique_lock lock(disk_gate_mutex);
  assert(disk_gate_cv.wait_for(lock, std::chrono::seconds(5), [] { return disk_gate_entered; }));
}
void ReleaseDiskGate()
{
  std::lock_guard lock(disk_gate_mutex);
  disk_gate_released = true;
  disk_gate_cv.notify_all();
}
config_edit::Outcome FixtureDiskSave(config_edit::TomlEditor& editor, const std::filesystem::path& path,
                                     const config_edit::Request& request)
{
  {
    std::unique_lock lock(disk_gate_mutex);
    if (disk_gate_armed) {
      disk_gate_armed   = false;
      disk_gate_entered = true;
      disk_gate_cv.notify_all();
      if (!disk_gate_cv.wait_for(lock, std::chrono::seconds(5), [] { return disk_gate_released; }))
        throw std::runtime_error("fixture save gate timed out");
    }
  }
  return editor.Save(path, request); // Actual editor and checked filesystem commit.
}
} // namespace
#define CONFIG_EDIT_SAVE(editor, path, request) FixtureDiskSave(editor, path, request)
#include "../src/mod/runtime_config_writer.cc"

namespace
{
using namespace mod_settings;
unsigned disk_quit_callbacks = 0;
void     Wait(ValueSetting<bool>& setting, SaveState desired)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (setting.save_status().state == SaveState::Pending) {
    assert(std::chrono::steady_clock::now() < deadline);
    std::this_thread::yield();
  }
  assert(setting.save_status().state == desired);
}
ValueDefinition<bool> Definition(bool& live)
{
  return PersistedDefinition<bool>({"preset-toggle", "Reorder officer presets",
                                    [&] { return ValueReadResult<bool>::Known(live, 1); },
                                    [&](bool value, std::uint64_t generation) {
                                      if (generation != 1)
                                        return ApplyResult::Rejected;
                                      live = value;
                                      return ApplyResult::Applied;
                                    }},
                                   {"ui", "allow_officer_preset_reordering", "fixture only", "", ApplyTiming::Live},
                                   false, std::chrono::milliseconds(20));
}
void ResetAdapter(config_edit::RuntimeConfigWriter& disk)
{
  writer    = &disk;
  available = true;
  owner     = CurrentThreadToken();
  requested_settings.clear();
  unavailable_settings.clear();
  draining = stopped = resume = forcing = false;
  vote                                  = 0;
}
} // namespace

int main()
{
  ConfigSaveFixtureDirectory fixture;
  const auto                 path = fixture.Root() / "fixture.toml";
  const std::string          initial =
      "# preserve me\n[ui]\nallow_officer_preset_reordering = false # user comment\nother = 'untouched'\n";
  {
    std::ofstream file(path, std::ios::binary);
    file << initial;
  }
  {
    config_edit::RuntimeConfigWriter disk(path);
    ResetAdapter(disk);
    // CONFIG_RUNTIME_TEST substitutes Config's native startup acquisition.
    // Seed its actual parsed startup value before idempotent UI registration.
    const auto parsed = toml::parse_file(path.string());
    bool       live   = parsed["ui"]["allow_officer_preset_reordering"].value<bool>().value();
    assert(disk.Register("ui", "allow_officer_preset_reordering", live));
    ValueSetting<bool> setting(Definition(live));
    assert(setting.Observe().state.value == false && ReadConfigText(path) == initial);
    ArmDiskGate();
    const auto changed = setting.SetFromUser(true, setting.Observe());
    assert(changed.outcome == Outcome::AppliedVerified && live);
    AwaitDiskGate();
    assert(setting.save_status().state == SaveState::Pending);
    assert(!runtime_config::CaptureReload().empty());
    ReleaseDiskGate();
    Wait(setting, SaveState::Saved);
    const auto saved = ReadConfigText(path);
    assert(saved.find("# preserve me") != std::string::npos && saved.find("# user comment") != std::string::npos);
    assert(saved.find("other = 'untouched'") != std::string::npos);
    assert(toml::parse(saved)["ui"]["allow_officer_preset_reordering"].value<bool>() == true);
    assert(runtime_config::CaptureReload().empty());

    // A conflicting external edit must not be overwritten or turn a verified
    // session change into a false successful disk-save claim.
    ReplaceConfigText(path, "# external\n[ui]\nallow_officer_preset_reordering = 'external'\nother=42\n");
    assert(setting.SetFromUser(false, setting.Observe()).outcome == Outcome::AppliedVerified && !live);
    Wait(setting, SaveState::Conflict);
    assert(toml::parse(ReadConfigText(path))["ui"]["allow_officer_preset_reordering"].value<std::string>()
           == "external");
    assert(!runtime_config::CaptureReload().empty());
    // Restore the last acknowledged disk value explicitly, then retry the
    // same accepted intent. Registration never silently rebases a conflict.
    ReplaceConfigText(path, saved);
    assert(setting.RetrySave(setting.Observe()).outcome == Outcome::Unchanged);
    Wait(setting, SaveState::Saved);
    ArmDiskGate();
    assert(setting.SetFromUser(true, setting.Observe()).outcome == Outcome::AppliedVerified);
    AwaitDiskGate();
    assert(setting.save_status().state == SaveState::Pending);
    request_quit = [](int) { ++disk_quit_callbacks; };
    // Quit admission/drain runs through the actual adapter; the native quit
    // callback and separate state worker alone are controlled by the fixture.
    assert(!WantsQuit([] { return true; }));
    ReleaseDiskGate();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!stopped) {
      assert(std::chrono::steady_clock::now() < deadline);
      Update();
      std::this_thread::yield();
    }
    assert(disk_quit_callbacks == 1 && setting.save_status().state == SaveState::Saved);
    writer = nullptr;
  }
  // Recreate the disk owner and model from the saved file: reopening is not a
  // successful read from the previous owner's in-memory state.
  {
    config_edit::RuntimeConfigWriter disk(path);
    ResetAdapter(disk);
    bool live = toml::parse_file(path.string())["ui"]["allow_officer_preset_reordering"].value<bool>().value();
    assert(live && disk.Register("ui", "allow_officer_preset_reordering", live));
    ValueSetting<bool> reopened(Definition(live));
    assert(reopened.Observe().state.value == true);
    assert(reopened.SetFromUser(false, reopened.Observe()).outcome == Outcome::AppliedVerified);
    Wait(reopened, SaveState::Saved);
    writer = nullptr;
  }
  assert(toml::parse(ReadConfigText(path))["ui"]["allow_officer_preset_reordering"].value<bool>() == false);
  std::cout << "Settings persistence: production value model -> runtime adapter -> background worker -> preserving "
               "TOML edit -> checked replacement -> receipt -> reopen passed; native UI/quit delivery simulated\n";
}
