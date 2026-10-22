#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace mod_state
{
enum class Group : size_t { OfficerOrder, ShipPins, ConsoleOpacity, Count };
enum class SaveOutcome { None, Pending, Saved, Failed };
struct SaveStatus {
  uint64_t    revision = 0;
  SaveOutcome outcome  = SaveOutcome::None;
};
struct StateEdit {
  Group                    group;
  std::vector<int64_t>     ids;
  std::vector<std::string> legacy_names;
  int                      opacity_percent = 0;
};

// Whole desired values may replace an older pending value in the SAME group.
// The two pin fields travel together. A worker transaction rereads the actual
// file while holding its OS lock; this is never a cached-file replacement.
class StateWriter
{
public:
  using Commit = std::function<bool(const StateEdit&)>;
  explicit StateWriter(Commit commit, std::chrono::milliseconds debounce = std::chrono::milliseconds(75))
      : commit_(std::move(commit))
      , debounce_(debounce)
      , worker_([this] { Run(); })
  {
  }
  ~StateWriter()
  {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
    }
    wake_.notify_one();
    worker_.join();
  }
  StateWriter(const StateWriter&)            = delete;
  StateWriter& operator=(const StateWriter&) = delete;
  uint64_t     Submit(StateEdit edit)
  {
    const auto charge = Charge(edit);
    const auto group  = static_cast<size_t>(edit.group);
    if (!charge || group >= pending_.size())
      return 0;
    std::lock_guard lock(mutex_);
    if (stopping_)
      return 0;
    const auto replaced = pending_[group] ? pending_[group]->charge : 0;
    if (retained_bytes_ - replaced > MaximumRetainedBytes - charge)
      return 0;
    const auto revision = ++revision_;
    pending_[group]     = Work{revision, std::move(edit), charge, Clock::now() + debounce_};
    retained_bytes_     = retained_bytes_ - replaced + charge;
    status_[group]      = {revision, SaveOutcome::Pending};
    wake_.notify_one();
    return revision;
  }
  SaveStatus Status(Group group)
  {
    std::lock_guard lock(mutex_);
    const auto      index = static_cast<size_t>(group);
    return index < status_.size() ? status_[index] : SaveStatus{};
  }
  size_t RetainedBytes()
  {
    std::lock_guard lock(mutex_);
    return retained_bytes_;
  }
  static constexpr size_t MaximumEntries       = 4096;
  static constexpr size_t MaximumEditBytes     = 1024 * 1024;
  static constexpr size_t MaximumRetainedBytes = 4 * MaximumEditBytes;
  static size_t           Charge(const StateEdit& edit)
  { return Charge(edit.ids, edit.legacy_names); }
  static size_t Charge(const std::vector<int64_t>& ids, const std::vector<std::string>& names = {})
  {
    if (ids.size() > MaximumEntries || names.size() > MaximumEntries)
      return 0;
    // Moved containers retain their allocation, including unconstructed vector
    // slots and spare string storage. Check before multiplying/adding so even
    // an unusually large reservation cannot wrap the admission charge.
    size_t charge = sizeof(StateEdit);
    if (ids.capacity() > (MaximumEditBytes - charge) / sizeof(int64_t))
      return 0;
    charge += ids.capacity() * sizeof(int64_t);
    if (names.capacity() > (MaximumEditBytes - charge) / sizeof(std::string))
      return 0;
    charge += names.capacity() * sizeof(std::string);
    for (const auto& name : names) {
      // SSO storage may be counted again conservatively; it must never be
      // omitted for strings whose heap capacity exceeds their current size.
      if (name.capacity() >= MaximumEditBytes || charge > MaximumEditBytes - name.capacity() - 1)
        return 0;
      charge += name.capacity() + 1;
    }
    return charge <= MaximumEditBytes ? charge : 0;
  }

private:
  using Clock = std::chrono::steady_clock;
  struct Work {
    uint64_t          revision;
    StateEdit         edit;
    size_t            charge;
    Clock::time_point ready;
  };
  void Run() noexcept
  {
    for (;;) {
      std::optional<Work> work;
      {
        std::unique_lock lock(mutex_);
        for (;;) {
          std::optional<size_t> first;
          for (size_t index = 0; index < pending_.size(); ++index)
            if (pending_[index] && (!first || pending_[index]->revision < pending_[*first]->revision))
              first = index;
          if (!first) {
            if (stopping_)
              return;
            wake_.wait(lock);
            continue;
          }
          const auto ready = pending_[*first]->ready;
          if (!stopping_ && Clock::now() < ready) {
            wake_.wait_until(lock, ready);
            continue;
          }
          work = std::move(pending_[*first]);
          pending_[*first].reset();
          break;
        }
      }
      bool saved = false;
      try {
        saved = commit_(work->edit);
      } catch (...) {
      }
      const auto charge   = work->charge;
      const auto revision = work->revision;
      const auto group    = static_cast<size_t>(work->edit.group);
      work.reset(); // Destroy owned vectors before releasing their budget.
      {
        std::lock_guard lock(mutex_);
        retained_bytes_ -= charge;
        if (status_[group].revision == revision)
          status_[group].outcome = saved ? SaveOutcome::Saved : SaveOutcome::Failed;
      }
    }
  }
  Commit                                                             commit_;
  std::chrono::milliseconds                                          debounce_;
  std::mutex                                                         mutex_;
  std::condition_variable                                            wake_;
  std::array<std::optional<Work>, static_cast<size_t>(Group::Count)> pending_;
  std::array<SaveStatus, static_cast<size_t>(Group::Count)>          status_{};
  uint64_t                                                           revision_       = 0;
  size_t                                                             retained_bytes_ = 0;
  bool                                                               stopping_       = false;
  std::thread                                                        worker_;
};
} // namespace mod_state
