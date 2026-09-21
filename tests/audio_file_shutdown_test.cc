#include "settings/audio_file_tasks.h"
#include <memory>
#include <string_view>

int main(int argc, char** argv)
{
  if (argc != 2) return 2;
  // The worker owns the promise it waits on: it cannot finish accidentally
  // through a broken promise when main returns. The harness bounds process exit.
  auto release = std::make_shared<std::promise<void>>();
  auto blocked = release->get_future();
  std::promise<void> started;
  auto entered = started.get_future();
  if (std::string_view(argv[1]) == "picker") {
    mod_settings::FileTasks().picker = mod_settings::RunAudioTask(
        [release, blocked = std::move(blocked), started = std::move(started)]() mutable -> std::string {
          started.set_value();
          blocked.wait();
          return {};
        });
  } else if (std::string_view(argv[1]) == "loading") {
    mod_settings::FileTasks().loading = mod_settings::RunAudioTask(
        [release, blocked = std::move(blocked), started = std::move(started)]() mutable -> NotificationAudioCue {
          started.set_value();
          blocked.wait();
          return {};
        });
  } else return 2;
  entered.wait();
  return 0; // Must not join the deliberately still-pending task at shutdown.
}
