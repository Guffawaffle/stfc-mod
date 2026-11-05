#pragma once
#include <future>
#include <thread>
#include <utility>

namespace mod_settings
{
// PLAY's RunAudioTask policy, scoped to file dialogs. The future does not join
// on destruction. Hooks live until process exit; workers own their inputs and
// never access Config, a managed object, or the Settings UI.
template <typename Function> auto RunFilePickerTask(Function&& function)
{
  using Result = std::invoke_result_t<Function>;
  std::packaged_task<Result()> task(std::forward<Function>(function));
  auto                         result = task.get_future();
  std::thread(std::move(task)).detach();
  return result;
}
} // namespace mod_settings
