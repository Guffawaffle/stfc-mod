#include "settings/image_file_setting.h"
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace mod_settings;
std::promise<std::string> picked;
std::future<std::string>  OpenImageFilePicker()
{
  picked = std::promise<std::string>{};
  return picked.get_future();
}
void Check(bool condition)
{
  if (!condition)
    throw std::runtime_error("image picker fixture failed");
}
void Drain(ImageFileSetting& picker, ActionSetting& action)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!action.Read(0).actionable() && std::chrono::steady_clock::now() < deadline) {
    picker.Poll();
    std::this_thread::yield();
  }
  Check(action.Read(0).actionable());
}
int main(int argc, char** argv)
{
  Check(argc == 2);
  const auto directory = std::filesystem::path(argv[1]);
  std::filesystem::create_directories(directory);
  const auto image = directory / "loading.png";
  std::ofstream(image) << "existing path boundary fixture";
  std::string                  requested, active;
  int                          writes = 0;
  ValueDefinition<std::string> definition{"image", "Loading image path",
                                          [&] { return ValueReadResult<std::string>::Known(requested, 1); },
                                          [&](std::string value, std::uint64_t) {
                                            requested = std::move(value);
                                            return ApplyResult::Applied;
                                          }};
  definition.default_value = "";
  definition.persist       = [&](const auto&) { ++writes; };
  definition.active_value  = [&] { return std::optional(active); };
  auto          setting    = std::make_shared<ValueSetting<std::string>>(std::move(definition));
  ActionSetting action;
  action.read          = [&](auto) { return ActionSetting::Presentation{"Loading image path", "Edit", requested}; };
  action.reset         = [&] { setting->Reset(setting->Observe()); };
  action.prepare_reset = [&] { return PrepareReset(*setting); };
  std::mutex              probe_mutex;
  std::condition_variable probe_wake;
  bool                    block_probe = false, probe_entered = false, release_probe = false, probe_finished = false;
  ImagePathValidator      validation(
      [&](const auto& path) {
        {
          std::unique_lock lock(probe_mutex);
          if (block_probe) {
            probe_entered = true;
            probe_wake.notify_all();
            probe_wake.wait(lock, [&] { return release_probe; });
            probe_finished = true;
            probe_wake.notify_all();
          }
        }
        std::error_code error;
        return std::filesystem::is_regular_file(std::filesystem::path(path), error) && !error;
      },
      std::chrono::milliseconds(0));
  struct Unblock {
    std::mutex&              mutex;
    std::condition_variable& wake;
    bool&                    release;
    ~Unblock()
    {
      {
        std::lock_guard lock(mutex);
        release = true;
      }
      wake.notify_all();
    }
  } unblock{probe_mutex, probe_wake, release_probe};
  ImageFileSetting picker(setting, action, validation);
  action.invoke(0);
  Check(!action.Read(0).actionable());
  Check(picker.Poll()); // Initial choosing status is presented without waiting.
  picked.set_value(image.string());
  picker.Poll();
  Drain(picker, action);
  Check(requested == image.string() && active.empty() && writes == 1);
  Check(action.Read(0).actionable());
  action.invoke(0);
  picked.set_value("");
  picker.Poll();
  Check(requested == image.string() && writes == 1); // Cancel never clears a preference.
  action.invoke(0);
  setting->SetFromUser("other.png", setting->Observe());
  const auto afterEdit = writes;
  picked.set_value(image.string());
  picker.Poll();
  Drain(picker, action);
  Check(requested == "other.png" && writes == afterEdit); // Stale picker cannot overwrite a later edit.
  action.reset();
  action.invoke(0);
  auto reset = action.prepare_reset();
  Check(reset.current() && reset.apply()); // Even an unchanged-default reset cancels pending intent.
  const auto afterReset = writes;
  picked.set_value(image.string());
  picker.Poll();
  Check(requested.empty() && writes == afterReset);
  action.invoke(0);
  picked.set_value((directory / "missing.png").string());
  picker.Poll();
  Drain(picker, action);
  Check(requested.empty() && writes == afterReset);
  action.invoke(0);
  picked.set_exception(std::make_exception_ptr(std::runtime_error("dialog failure")));
  picker.Poll();
  Check(action.Read(0).actionable() && writes == afterReset);
  {
    std::lock_guard lock(probe_mutex);
    block_probe = true;
  }
  action.invoke(0);
  picked.set_value(image.string());
  picker.Poll();
  {
    std::unique_lock lock(probe_mutex);
    Check(probe_wake.wait_for(lock, std::chrono::seconds(3), [&] { return probe_entered; }));
  }
  Check(!action.Read(0).actionable() && requested.empty());
  action.reset(); // Reset while an actual filesystem probe is in flight.
  const auto resetDuringProbe = writes;
  {
    std::lock_guard lock(probe_mutex);
    release_probe = true;
  }
  probe_wake.notify_all();
  {
    std::unique_lock lock(probe_mutex);
    Check(probe_wake.wait_for(lock, std::chrono::seconds(3), [&] { return probe_finished; }));
  }
  picker.Poll();
  Check(requested.empty() && writes == resetDuringProbe && action.Read(0).actionable());
  std::filesystem::remove(image);
  std::cout
      << "Image picker: async admission, cancel, stale edit/reset, missing file, failure and restart timing passed\n";
}
