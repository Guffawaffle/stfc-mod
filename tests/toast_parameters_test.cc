// The native toast consumer uses this same lazy source-selection policy; no game or notification delivery.
#include "features/toast_text.h"

#include <array>
#include <cassert>
#include <iostream>
#include <vector>

namespace
{
using toast_text::ParameterFormat;
using toast_text::ParameterRead;
using toast_text::ParameterSource;
using toast_text::ParameterState;

ParameterFormat Resolve(std::string_view text, ParameterRead primary, ParameterRead context,
                        std::vector<ParameterSource>& reads)
{
  return toast_text::FormatParameters(text, [&](ParameterSource source) {
    reads.push_back(source);
    return source == ParameterSource::kToast ? primary : context;
  });
}
} // namespace

int main()
{
  std::vector<ParameterSource> reads;
  auto                         result = Resolve("{0} Lv.{1:000}", {ParameterState::kValues, {"Enterprise", "7"}},
                                                {ParameterState::kValues, {"wrong", "99"}}, reads);
  assert(result.text == "Enterprise Lv.007" && result.source == ParameterSource::kToast);
  assert(result.toast == ParameterState::kValues && !result.context);
  assert((reads == std::vector{ParameterSource::kToast}));

  for (const auto state : std::array{ParameterState::kUnavailable, ParameterState::kInvalid, ParameterState::kNull,
                                     ParameterState::kEmpty}) {
    reads.clear();
    result = Resolve("{0} {1}", {state, {}}, {ParameterState::kValues, {"Kirk", "7"}}, reads);
    assert(result.text == "Kirk 7" && result.source == ParameterSource::kContext);
    assert(result.toast == state && result.context == ParameterState::kValues);
    assert((reads == std::vector{ParameterSource::kToast, ParameterSource::kContext}));
  }

  // Unknown slots and failed/null element conversions do not cause unrelated context values to be merged.
  reads.clear();
  result = Resolve("{0} {1} {2}", {ParameterState::kValues, {"", "7"}},
                   {ParameterState::kValues, {"context", "99", "unexpected"}}, reads);
  assert(result.text == "{0} 7 {2}" && result.source == ParameterSource::kToast);
  assert(reads.size() == 1 && !result.context);

  // Braces supplied by a parameter remain literal; only the original template is scanned.
  reads.clear();
  result = Resolve("{0} vs {1}", {ParameterState::kNull, {}}, {ParameterState::kValues, {"{1}", "Enterprise"}}, reads);
  assert(result.text == "{1} vs Enterprise" && result.source == ParameterSource::kContext);

  for (const auto state : std::array{ParameterState::kUnavailable, ParameterState::kInvalid, ParameterState::kNull,
                                     ParameterState::kEmpty}) {
    reads.clear();
    result = Resolve("Unknown {0}", {ParameterState::kNull, {}}, {state, {}}, reads);
    assert(result.text == "Unknown {0}" && result.source == ParameterSource::kNone);
    assert(result.toast == ParameterState::kNull && result.context == state && reads.size() == 2);
  }

  reads.clear();
  result = Resolve("Already localized", {ParameterState::kValues, {"ignored"}}, {ParameterState::kValues, {"ignored"}},
                   reads);
  assert(result.text == "Already localized" && result.source == ParameterSource::kNone);
  assert(reads.empty() && !result.toast && !result.context);
  std::cout << "Toast named-parameter precedence, fallback, availability and literal-brace fixtures passed\n";
}
