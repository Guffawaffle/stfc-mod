// Pure overlay over the existing TOML model. No file, worker, game or runtime state is consulted.
#include "runtime_reload.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
void Check(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}
void Refused(toml::table input, const runtime_config::ReloadSnapshot& snapshot)
{
  const auto before   = input;
  bool       rejected = false;
  try {
    runtime_config::ApplyReloadSnapshot(input, snapshot);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  Check(rejected && input == before, "invalid snapshot mutated its candidate or was accepted");
}

void ProtectedValues()
{
  auto                                 input     = toml::parse(R"(
    [ui]
    show_player_cargo = true
    label = 'external'
    [graphics]
    ui_scale = 0.5
    [unrelated]
    value = 'external change should flow'
    [[unrelated.items]]
    name = 'preserved array of tables'
  )");
  const auto                           original  = input;
  const auto                           unrelated = *input.get_as<toml::table>("unrelated");
  const runtime_config::ReloadSnapshot protected_values{
      {"ui", "show_player_cargo", false},
      {"ui", "label", std::string{}},
      {"graphics", "ui_scale", 0.0},
      {"new", "count", std::numeric_limits<std::int64_t>::max()},
      {"new", "minimum", std::numeric_limits<std::int64_t>::min()},
  };
  runtime_config::ApplyReloadSnapshot(input, protected_values);
  Check(input["ui"]["show_player_cargo"].value<bool>() == false, "false was treated as missing");
  Check(input["ui"]["label"].value<std::string>() == "", "empty string was treated as missing");
  Check(input["graphics"]["ui_scale"].value<double>() == 0.0, "zero was treated as missing");
  Check(input["new"]["count"].value<std::int64_t>() == std::numeric_limits<std::int64_t>::max()
            && input["new"]["minimum"].value<std::int64_t>() == std::numeric_limits<std::int64_t>::min(),
        "integer observations lost precision");
  Check(*input.get_as<toml::table>("unrelated") == unrelated, "overlay changed unrelated source values");
  auto unprotected = original;
  runtime_config::ApplyReloadSnapshot(unprotected, {});
  Check(unprotected == original, "empty protection altered external settings");
  runtime_config::ApplyReloadSnapshot(input, protected_values);
  Check(*input.get_as<toml::table>("unrelated") == unrelated, "repeated overlay altered unrelated values");
}

void LiteralSegments()
{
  const std::string section = "ui.\xc3\xa9", key = "e\xcc\x81.mode";
  toml::table       input;
  runtime_config::ApplyReloadSnapshot(input, {{section, key, std::string("owned value")}});
  Check(input[section][key].value<std::string>() == "owned value", "literal Unicode/dotted identity changed");
  Check(!input.contains("ui"), "literal section was incorrectly split into a hierarchy");
  auto next = input;
  runtime_config::ApplyReloadSnapshot(next, {{section, key, std::string("later requested value")}});
  Check(input[section][key].value<std::string>() == "owned value", "an earlier snapshot was not independently owned");
}

void InvalidSnapshots()
{
  const auto input = toml::parse("[ui]\nvalue = 1\nnot_table = false\n");
  Refused(input, {{"ui", "value", std::int64_t{2}}, {"ui", "value", std::int64_t{3}}});
  Refused(input, {{"", "value", true}});
  Refused(input, {{"ui", "", true}});
  Refused(toml::parse("ui = false\n"), {{"ui", "value", true}});
  for (const double value : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()})
    Refused(input, {{"ui", "first", true}, {"ui", "value", value}});
}
} // namespace

int main()
{
  try {
    ProtectedValues();
    LiteralSegments();
    InvalidSnapshots();
    std::cout
        << "Runtime reload overlay passed: owned exact values, literal identities, unrelated data and rejection\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
