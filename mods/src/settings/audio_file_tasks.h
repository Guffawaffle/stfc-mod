#pragma once
#include "patches/notification_audio.h"
#include <future>
#include <thread>
#include <utility>

namespace mod_settings
{
struct AudioFileTasks {
  std::future<std::string> picker;
  std::future<NotificationAudioCue> loading;
};

template <typename Function> auto RunAudioTask(Function&& function)
{
  // These hooks stay loaded until process exit. Unlike std::async, a packaged
  // task's future does not join its worker when destroyed, and the MSVC async
  // runtime cannot wait for it during CRT shutdown. Workers own their inputs
  // and never access the settings UI or Config.
  using Result = std::invoke_result_t<Function>;
  std::packaged_task<Result()> task(std::forward<Function>(function));
  auto result = task.get_future();
  std::thread(std::move(task)).detach();
  return result;
}

inline AudioFileTasks& FileTasks()
{
  static AudioFileTasks tasks;
  return tasks;
}
}
