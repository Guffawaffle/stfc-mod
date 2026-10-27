#pragma once
#include "action_setting.h"
#include "image_file_picker.h"
#include "image_path_validator.h"
#include "value_settings.h"
#include <chrono>
#include <future>
#include <memory>
#include <utility>

namespace mod_settings
{
// The UI thread owns the setting and polls a picker that owns only its result.
// Reset also cancels pending intent when the current value already is default.
class ImageFileSetting
{
public:
  ImageFileSetting(std::shared_ptr<ValueSetting<std::string>> setting, ActionSetting& action,
                   ImagePathValidator& validator = LoadingImagePathValidator())
      : setting_(std::move(setting))
      , validator_(validator)
  {
    const auto read = action.read;
    action.read     = [this, read](std::size_t index) {
      auto result    = read(index);
      result.button  = "Choose file";
      result.enabled = result.enabled && !picker_.valid() && !validating_;
      if (!status_.empty())
        result.value += "\n" + status_;
      return result;
    };
    action.invoke = [this](std::size_t) {
      if (picker_.valid() || validating_)
        return;
      observed_ = setting_->Observe();
      dirty_    = true;
      if (!observed_.state.known())
        return;
      validator_.Cancel();
      try {
        picker_    = OpenImageFilePicker();
        requested_ = generation_;
        status_    = "Choosing an image...";
      } catch (...) {
        status_ = "Could not open the file picker";
      }
    };
    const auto reset = action.reset;
    action.reset     = [this, reset] {
      CancelValidation();
      reset();
    };
    const auto prepare   = action.prepare_reset;
    action.prepare_reset = [this, prepare] {
      auto       plan  = prepare();
      const auto apply = plan.apply;
      plan.apply       = [this, apply] {
        CancelValidation();
        return apply && apply();
      };
      return plan;
    };
  }
  bool Poll()
  {
    bool changed = std::exchange(dirty_, false);
    try {
      if (picker_.valid() && picker_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto path = picker_.get();
        changed   = true;
        if (path.empty())
          status_ = "Selection canceled";
        else if (requested_ != generation_)
          status_ = "Selection discarded after reset";
        else if (path.size() > ImagePathValidator::MaximumPathBytes)
          status_ = "Selected image path is too long";
        else {
          const auto validation = validator_.Check(path);
          validation_ticket_    = validation.generation;
          validating_path_      = std::move(path);
          validating_           = true;
          status_               = "Checking the selected image...";
        }
      }
      if (!validating_)
        return changed;
      const auto validation = validator_.Get(validation_ticket_);
      if (validation.state == ImagePathValidator::State::Pending)
        return changed;
      validating_ = false;
      if (requested_ != generation_)
        status_ = "Selection discarded after reset";
      else if (validation.state != ImagePathValidator::State::Valid)
        status_ = "Selected image is unavailable";
      else {
        const auto result = setting_->SetFromUser(validating_path_, observed_).outcome;
        status_           = result == Outcome::AppliedVerified || result == Outcome::Unchanged
                                ? std::string{}
                                : "Setting changed; choose the image again";
      }
      validating_path_.clear();
    } catch (...) {
      validating_ = false;
      validating_path_.clear();
      status_ = "Could not select an image";
    }
    return true;
  }

private:
  void CancelValidation()
  {
    ++generation_;
    validator_.Cancel();
    validating_ = false;
    validating_path_.clear();
    status_.clear();
    dirty_ = true;
  }
  std::shared_ptr<ValueSetting<std::string>> setting_;
  ImagePathValidator&                        validator_;
  ValueSnapshot<std::string>                 observed_;
  std::future<std::string>                   picker_;
  std::string                                status_;
  std::string                                validating_path_;
  uint64_t                                   validation_ticket_ = 0;
  bool                                       validating_        = false;
  bool                                       dirty_      = false;
  std::uint64_t                              generation_ = 0, requested_ = 0;
};
} // namespace mod_settings
