#pragma once

#include <toml++/toml.h>

#include <string>
#include <string_view>
#include <vector>

namespace config_release_validation
{
struct ValidationIssue {
  std::string path;
  std::string message;
};

struct ExampleConfigValidationResult {
  std::vector<ValidationIssue> errors;

  [[nodiscard]] bool ok() const
  { return errors.empty(); }
};

[[nodiscard]] ExampleConfigValidationResult ValidateExampleConfig(const toml::table& config);
[[nodiscard]] std::string RenderGeneratedInputBindingCompatibilitySection();
[[nodiscard]] std::string ExtractGeneratedInputBindingCompatibilitySection(std::string_view markdown);
[[nodiscard]] std::string NormalizeMarkdownNewlines(std::string_view text);

} // namespace config_release_validation
