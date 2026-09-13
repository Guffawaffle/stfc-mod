#pragma once
#include "value_settings.h"
#include <vector>

namespace mod_settings
{
using ShortcutList = std::vector<std::string>;
// A draft edits exactly one alternative against the snapshot the user saw.
// The existing ValueSetting owner provides reentry/stale-read protection.
class ShortcutDraft
{
public:
  explicit ShortcutDraft(ValueSetting<ShortcutList>& owner)
      : owner_(owner)
  {
  }
  void Begin()
  {
    Cancel();
    observed_ = owner_.Observe();
  }
  void Stage(std::size_t index, std::optional<std::string> replacement)
  {
    if (!observed_.state.known()) {
      Cancel();
      return;
    }
    auto value = *observed_.state.value;
    if (index > value.size() || (!replacement && index == value.size())) {
      Cancel();
      return;
    }
    if (!replacement)
      value.erase(value.begin() + index);
    else if (index == value.size())
      value.push_back(*replacement);
    else
      value[index] = *replacement;
    desired_ = std::move(value);
  }
  bool pending() const
  { return desired_.has_value(); }
  const std::optional<ShortcutList>& desired() const
  { return desired_; }
  Outcome Apply()
  {
    if (!desired_)
      return Outcome::Suppressed;
    const auto result = owner_.SetFromUser(*desired_, observed_).outcome;
    Cancel();
    return result;
  }
  void Cancel()
  {
    desired_.reset();
    observed_ = {};
  }

private:
  ValueSetting<ShortcutList>& owner_;
  ValueSnapshot<ShortcutList> observed_;
  std::optional<ShortcutList> desired_;
};
} // namespace mod_settings
