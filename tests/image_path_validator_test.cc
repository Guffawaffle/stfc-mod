#include "settings/image_path_validator.h"
#include <condition_variable>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;
using Validator = mod_settings::ImagePathValidator;
void Check(bool value)
{
  if (!value)
    throw std::runtime_error("image validation fixture failed");
}
int main()
{
  std::mutex               mutex;
  std::condition_variable  wake;
  bool                     release = false;
  std::vector<std::string> probes;
  const auto               owner = std::this_thread::get_id();
  Validator                validator(
      [&](const std::string& path) {
        Check(std::this_thread::get_id() != owner);
        std::unique_lock lock(mutex);
        probes.push_back(path);
        wake.notify_all();
        if (path == "slow")
          wake.wait(lock, [&] { return release; });
        return path != "missing";
      },
      0ms);
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
  } unblock{mutex, wake, release};
  const auto slow = validator.Check("slow");
  {
    std::unique_lock lock(mutex);
    Check(wake.wait_for(lock, 3s, [&] { return probes.size() == 1; }));
  }
  auto       latest = slow;
  const auto before = std::chrono::steady_clock::now();
  for (int index = 0; index < 1000; ++index)
    latest = validator.Check("pending-" + std::to_string(index));
  Check(std::chrono::steady_clock::now() - before < 1s);
  Check(validator.Get(slow.generation).state == Validator::State::Stale);
  Check(validator.Get(latest.generation).state == Validator::State::Pending);
  {
    std::lock_guard lock(mutex);
    Check(probes.size() == 1);
    release = true;
  }
  wake.notify_all();
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  while (validator.Get(latest.generation).state == Validator::State::Pending
         && std::chrono::steady_clock::now() < deadline)
    std::this_thread::yield();
  Check(validator.Get(latest.generation).state == Validator::State::Valid);
  {
    std::lock_guard lock(mutex);
    Check(probes == std::vector<std::string>({"slow", "pending-999"}));
  }
  validator.Cancel();
  Check(validator.Get(latest.generation).state == Validator::State::Stale);
  Check(validator.Check("").state == Validator::State::Valid);
  Check(validator.Check(std::string(Validator::MaximumPathBytes + 1, 'x')).state == Validator::State::Invalid);
  Check(validator.Check("").state == Validator::State::Valid);

  std::atomic_int calls{0};
  Validator       debounced(
      [&](const auto&) {
        ++calls;
        return true;
      },
      100ms);
  for (int index = 0; index < 100; ++index)
    debounced.Check("changed-" + std::to_string(index));
  Check(calls == 0);
  const auto sequence = debounced.Sequence();
  const auto until    = std::chrono::steady_clock::now() + 3s;
  while (debounced.Sequence() == sequence && std::chrono::steady_clock::now() < until)
    std::this_thread::yield();
  Check(calls == 1 && debounced.Sequence() > sequence);
  Validator  failed([](const auto&) -> bool { throw std::runtime_error("stat failed"); }, 0ms);
  const auto failure          = failed.Check("failure");
  const auto failure_deadline = std::chrono::steady_clock::now() + 3s;
  while (failed.Get(failure.generation).state == Validator::State::Pending
         && std::chrono::steady_clock::now() < failure_deadline)
    std::this_thread::yield();
  Check(failed.Get(failure.generation).state == Validator::State::Failed);
  std::cout << "Image validation: one active/latest pending, debounce, owner isolation, stale cancellation and failure "
               "passed\n";
}
