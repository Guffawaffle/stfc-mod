#pragma once

#include "image_path_validator.h"
#include "text_entry.h"
#include "value_settings.h"
#include <memory>
#include <utility>

namespace mod_settings
{
// PLAY's exact-entry fallback. The host supplies page custody; only owned paths
// cross the same validator used by the picker model.
inline TextEntryRequest ImagePathEntry(const std::shared_ptr<ValueSetting<std::string>>& image,
                                       std::function<bool()>                             current,
                                       ImagePathValidator& validator = LoadingImagePathValidator())
{
  const auto observed = image->Observe();
  validator.Cancel();
  const auto trim = [](std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    return first == text.npos ? std::string_view{} : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
  };
  TextEntryRequest request{
      .title = image->label(),
      .value = observed.state.known() ? *observed.state.value : std::string{},
      .help  = image->metadata().description,
      .validate =
          [&validator, trim](std::string_view text) {
            switch (validator.Check(trim(text)).state) {
              case ImagePathValidator::State::Valid:
                return std::string{};
              case ImagePathValidator::State::Pending:
                return std::string{"Checking the image path..."};
              default:
                return std::string{"Enter a path to an available image, or leave it empty."};
            }
          },
      .commit =
          [image, observed, current = std::move(current), &validator, trim](std::string_view text) {
            if (!observed.state.known() || !current())
              return false;
            const auto path = trim(text);
            if (validator.Check(path).state != ImagePathValidator::State::Valid || !current())
              return false;
            const auto outcome = image->SetFromUser(std::string(path), observed).outcome;
            const bool applied = outcome == Outcome::AppliedVerified || outcome == Outcome::Unchanged;
            if (applied)
              validator.Cancel();
            return applied;
          },
      .cancel = [&validator] { validator.Cancel(); }};
  request.validation_changed = [&validator, sequence = validator.Sequence()]() mutable {
    const auto value = validator.Sequence();
    return std::exchange(sequence, value) != value;
  };
  return request;
}
} // namespace mod_settings
