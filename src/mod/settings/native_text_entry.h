#pragma once

#include "text_entry.h"
#include <cstdint>
#include <memory>
#include <utility>

namespace mod_settings
{
// Game-free request/session ownership. Draft validation never writes; each explicit Apply invokes commit once.
class TextEntrySession
{
public:
  using Token = std::uint64_t;
  enum class Result { Applied, Invalid, Rejected, Stale, Busy };
  enum class Recording { Unchanged, Changed, Unavailable, Stale, Busy };
  Token Open(TextEntryRequest request, std::function<bool()> current)
  {
    if (state_ || !request.commit || !current || (request.record_frame && !request.read_only))
      return 0;
    auto state     = std::make_shared<State>();
    state->request = std::move(request);
    state->current = std::move(current);
    state->value   = state->request.value;
    state->token   = ++serial_;
    if (!state->token)
      state->token = ++serial_;
    state_ = std::move(state);
    return state_->token;
  }
  bool Active() const
  { return bool(state_); }
  bool Current(Token token) const
  { return state_ && token != 0 && state_->token == token; }
  const std::string& Value() const
  { return state_ ? state_->value : empty_; }
  const std::string& Error() const
  { return state_ ? state_->error : empty_; }
  const TextEntryRequest* Request() const
  { return state_ ? &state_->request : nullptr; }
  void Draft(std::string value)
  {
    if (state_ && !state_->request.read_only && !state_->applying && !state_->validating)
      state_->value = std::move(value);
  }
  bool Validate()
  {
    auto state = state_;
    if (!state || state->validating)
      return false;
    state->validating = true;
    struct Guard {
      bool& value;
      ~Guard()
      { value = false; }
    } guard{state->validating};
    std::string error;
    try {
      if (!state->current())
        error = "Settings page changed. Cancel and reopen to try again.";
      else if (state->request.validate)
        error = state->request.validate(state->value);
      if (error.empty() && !state->current())
        error = "Settings page changed. Cancel and reopen to try again.";
    } catch (...) {
      error = "Validation unavailable. Cancel and reopen to try again.";
    }
    if (state_ != state)
      return false;
    state->error = std::move(error);
    return state->error.empty();
  }
  Result Apply()
  {
    auto state = state_;
    if (!state)
      return Result::Stale;
    if (state->applying || state->validating || state->recording)
      return Result::Busy;
    state->applying = true;
    struct Guard {
      bool& value;
      ~Guard()
      { value = false; }
    } guard{state->applying};
    if (!Validate())
      return state_ == state ? Result::Invalid : Result::Stale;
    if (state_ != state)
      return Result::Stale;
    const auto value   = state->value;
    bool       applied = false;
    try {
      applied = state->current() && state->request.commit(value);
    } catch (...) {
    }
    if (applied) {
      if (state_ == state)
        state_.reset();
      return Result::Applied;
    }
    if (state_ != state)
      return Result::Stale;
    state->error = "Setting changed or could not be applied. Cancel and reopen to try again.";
    return Result::Rejected;
  }
  void Cancel()
  {
    auto state = std::exchange(state_, {});
    if (state && state->request.cancel)
      try {
        state->request.cancel();
      } catch (...) {
      }
  }
  Recording SampleRecording()
  {
    auto state = state_;
    if (!state || !state->request.record_frame)
      return Recording::Stale;
    if (state->applying || state->validating || state->recording)
      return Recording::Busy;
    state->recording = true;
    struct Guard {
      bool& value;
      ~Guard()
      { value = false; }
    } guard{state->recording};
    try {
      if (!state->current())
        return Recording::Stale;
      const auto value = state->request.record_frame();
      if (state_ != state || !state->current())
        return Recording::Stale;
      if (!value || *value == state->value)
        return Recording::Unchanged;
      state->value = *value;
      return Recording::Changed;
    } catch (...) {
      if (state_ == state)
        state->error = "Shortcut recording unavailable. Cancel and reopen to try again.";
      return Recording::Unavailable;
    }
  }
  bool RecordAgain()
  {
    auto state = state_;
    if (!state || !state->request.record_frame || !state->request.record_again || state->applying || state->validating
        || state->recording)
      return false;
    state->recording = true;
    struct Guard {
      bool& value;
      ~Guard()
      { value = false; }
    } guard{state->recording};
    try {
      if (!state->current())
        return false;
      state->request.record_again();
      if (state_ != state || !state->current())
        return false;
      state->value = state->request.value;
      state->error.clear();
      return true;
    } catch (...) {
      if (state_ == state)
        state->error = "Shortcut recording unavailable. Cancel and reopen to try again.";
      return false;
    }
  }

private:
  struct State {
    TextEntryRequest      request;
    std::function<bool()> current;
    std::string           value, error;
    Token                 token    = 0;
    bool                  applying = false, validating = false, recording = false;
  };
  std::shared_ptr<State>          state_;
  Token                           serial_ = 0;
  inline static const std::string empty_;
};
} // namespace mod_settings
