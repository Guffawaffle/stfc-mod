#include "settings/native_text_entry.h"
#include "settings/value_settings.h"
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void Check(bool value, const char* message)
{
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
bool Integer(std::string_view text, int& value)
{
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
} // namespace

int main()
{
  using namespace mod_settings;
  TextEntrySession     session;
  int                  live = 7, writes = 0, saves = 0, cancels = 0, commits = 0;
  bool                 page_current = true;
  ValueDefinition<int> definition{"integer", "Exact integer", [&] { return ValueReadResult<int>::Known(live, 1); },
                                  [&](int desired, uint64_t) {
                                    ++writes;
                                    live = desired;
                                    return ApplyResult::Applied;
                                  }};
  definition.persist = [&](int) { ++saves; };
  ValueSetting<int> setting(std::move(definition));
  const auto        request = [&] {
    const auto observed = setting.Observe();
    return TextEntryRequest{.title = "Exact integer",
                            .value = std::to_string(live),
                            .validate =
                                [](std::string_view text) {
                                  int value = 0;
                                  return Integer(text, value) ? std::string{} : "Enter a whole integer in range.";
                                },
                            .commit =
                                [&, observed](std::string_view text) {
                                  ++commits;
                                  int value = 0;
                                  if (!Integer(text, value))
                                    return false;
                                  const auto result = setting.SetFromUser(value, observed).outcome;
                                  return result == Outcome::AppliedVerified || result == Outcome::Unchanged;
                                },
                            .cancel = [&] { ++cancels; }};
  };
  const auto first = session.Open(request(), [&] { return page_current; });
  Check(first && session.Current(first) && session.Value() == "7", "opening captures the exact current display value");
  Check(session.Validate() && writes == 0 && saves == 0 && commits == 0, "validation/rendering does not write or save");
  for (const auto* invalid : {"4.5", "2147483648", "5 trailing", ""}) {
    session.Draft(invalid);
    Check(session.Apply() == TextEntrySession::Result::Invalid && session.Value() == invalid,
          "invalid exact input stays unchanged and open");
  }
  Check(live == 7 && writes == 0 && saves == 0 && commits == 0, "invalid drafts never reach the canonical setter");
  session.Draft(std::to_string(std::numeric_limits<int>::max()));
  Check(session.Apply() == TextEntrySession::Result::Applied && live == std::numeric_limits<int>::max() && writes == 1
            && saves == 1 && commits == 1 && !session.Active(),
        "one explicit commit preserves the full integer range and saves the canonical model exactly once");
  Check(session.Apply() == TextEntrySession::Result::Stale && commits == 1,
        "duplicate Apply cannot commit a closed request");

  const auto stale = session.Open(request(), [&] { return page_current; });
  session.Draft("8");
  live = 9;
  Check(session.Apply() == TextEntrySession::Result::Rejected && session.Current(stale) && session.Value() == "8"
            && live == 9 && saves == 1,
        "an external setting change rejects a stale snapshot without changing the field or retrying a write");
  page_current                = false;
  const auto previous_commits = commits;
  Check(session.Apply() == TextEntrySession::Result::Invalid && commits == previous_commits,
        "a departed native page cannot invoke the old request's commit");
  session.Cancel();
  session.Cancel();
  Check(cancels == 1 && !session.Active(), "Back/Escape/owner cleanup cancels once");
  page_current = true;

  const auto readonly = session.Open({.title     = "Details",
                                      .value     = "kept",
                                      .commit    = [](std::string_view text) { return text == "kept"; },
                                      .read_only = true},
                                     [] { return true; });
  session.Draft("changed");
  Check(session.Current(readonly) && session.Value() == "kept" && session.Apply() == TextEntrySession::Result::Applied,
        "read-only confirmation fields do not acquire a replacement draft");

  int reentrant_commits = 0;
  session.Open({.title = "Reentry",
                .value = "x",
                .commit =
                    [&](std::string_view) {
                      ++reentrant_commits;
                      Check(session.Apply() == TextEntrySession::Result::Busy,
                            "a commit cannot recursively submit itself");
                      return true;
                    }},
               [] { return true; });
  Check(session.Apply() == TextEntrySession::Result::Applied && reentrant_commits == 1,
        "reentrant callbacks retain exactly one explicit commit");

  TextEntrySession::Token replacement = 0;
  session.Open({.title = "Old",
                .value = "old",
                .commit =
                    [&](std::string_view) {
                      session.Cancel();
                      replacement = session.Open(
                          {.title = "New", .value = "new", .commit = [](std::string_view) { return false; }},
                          [] { return true; });
                      return true;
                    }},
               [] { return true; });
  Check(session.Apply() == TextEntrySession::Result::Applied && session.Current(replacement)
            && session.Value() == "new",
        "an old successful callback cannot close a replacement request opened during reentry");
  session.Cancel();
  std::optional<std::string> recorded;
  int                        recording_commits = 0, rerecords = 0;
  const auto                 recording = session.Open(
      {.title    = "Record shortcut",
       .value    = "",
       .validate = [](std::string_view text) { return text.empty() ? std::string("Press a shortcut") : std::string{}; },
       .commit =
           [&](std::string_view text) {
             ++recording_commits;
             return text == "RETURN";
           },
       .read_only    = true,
       .apply_label  = "Confirm",
       .record_frame = [&] { return recorded; },
       .record_again =
           [&] {
             ++rerecords;
             recorded.reset();
           }},
      [] { return true; });
  Check(recording && session.SampleRecording() == TextEntrySession::Recording::Unchanged,
        "recording without a candidate does not manufacture a draft");
  recorded = "RETURN";
  Check(session.SampleRecording() == TextEntrySession::Recording::Changed && session.Value() == "RETURN"
            && recording_commits == 0,
        "raw Return becomes a read-only candidate without implicitly applying");
  session.Draft("MOUSE0");
  Check(session.Value() == "RETURN", "ordinary text/pointer input cannot overwrite a recording candidate");
  Check(session.RecordAgain() && session.Current(recording) && session.Value().empty() && rerecords == 1
            && !session.Validate() && recording_commits == 0,
        "Record again clears only the candidate and waits for another explicit recording");
  recorded = "RETURN";
  Check(session.SampleRecording() == TextEntrySession::Recording::Changed
            && session.Apply() == TextEntrySession::Result::Applied && recording_commits == 1,
        "the explicit Confirm commits the recorded Return exactly once");
  std::cout << "PASS native text-entry request ownership, exact validation, canonical commit and cancellation\n";
}
