#include "discovery_tow_policy.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
int  checks = 0;
void Check(bool value)
{
  ++checks;
  if (!value) {
    std::cerr << "Discovery tow policy check failed: " << checks << '\n';
    std::abort();
  }
}

using namespace discovery_tow;
constexpr std::int64_t large = 9007199254740993LL;
Request                RequestForTest()
{
  return {.generation       = 7,
          .selected         = large,
          .discovery        = 31,
          .selected_index   = 0,
          .discovery_index  = 5,
          .section          = -747,
          .owner            = 100,
          .selected_object  = 200,
          .discovery_object = 300,
          .baseline         = 400,
          .deadline_ms      = 30'000};
}
Evidence Completed()
{
  return {.generation                 = 7,
          .now_ms                     = 29'999,
          .input_mode                 = true,
          .owner_alive                = true,
          .section                    = -747,
          .selected                   = large,
          .discovery                  = 31,
          .owner                      = 100,
          .selected_object            = 200,
          .discovery_object           = 300,
          .destination_matches        = true,
          .can_tow                    = true,
          .can_be_towed               = true,
          .iterator_state             = -1,
          .course                     = 500,
          .course_fleet               = large,
          .course_valid               = true,
          .course_in_use              = false,
          .course_destination_matches = true};
}

void WaitingAndCompletion()
{
  const auto request  = RequestForTest();
  auto       evidence = Completed();
  Check(Evaluate(request, evidence) == Decision::Submit);
  for (auto state : {0, 1, 2}) {
    evidence.iterator_state = state;
    evidence.course.reset();
    evidence.course_fleet.reset();
    evidence.course_valid.reset();
    evidence.course_in_use.reset();
    evidence.course_destination_matches.reset();
    Check(Evaluate(request, evidence) == Decision::Wait);
  }
  for (auto state : {-2, -100, 3, 99, std::numeric_limits<int32_t>::max()}) {
    evidence.iterator_state = state;
    Check(Evaluate(request, evidence) == Decision::Cancel);
  }
  evidence = Completed();
  evidence.iterator_state.reset();
  Check(SourceMatches(request, evidence)); // Pre-plan ownership can qualify before any iterator exists.
  Check(Evaluate(request, evidence) == Decision::Cancel);
  evidence        = Completed();
  evidence.course = request.baseline;
  Check(Evaluate(request, evidence) == Decision::Cancel); // A rooted old object cannot masquerade as a fresh course.
  evidence.course.reset();
  Check(Evaluate(request, evidence) == Decision::Cancel);
  evidence.course = 0;
  Check(Evaluate(request, evidence) == Decision::Cancel); // Successful null remains no course.
  auto no_baseline     = request;
  no_baseline.baseline = 0;
  Check(Evaluate(no_baseline, Completed()) == Decision::Submit);
}

void IdentityAndLifetime()
{
  const auto request = RequestForTest();
  for (auto field : {&Evidence::selected, &Evidence::discovery, &Evidence::course_fleet}) {
    auto evidence   = Completed();
    evidence.*field = 0;
    Check(Evaluate(request, evidence) == Decision::Cancel);
    evidence.*field = -1;
    Check(Evaluate(request, evidence) == Decision::Cancel);
    (evidence.*field).reset();
    Check(Evaluate(request, evidence) == Decision::Cancel);
  }
  for (auto field : {&Evidence::owner, &Evidence::selected_object, &Evidence::discovery_object}) {
    auto evidence = Completed();
    ++(evidence.*field);
    Check(Evaluate(request, evidence) == Decision::Cancel); // Same ID on a replacement object is a different source.
    evidence.*field = 0;
    Check(Evaluate(request, evidence) == Decision::Cancel);
  }
  auto evidence     = Completed();
  evidence.selected = large - 1;
  Check(Evaluate(request, evidence) == Decision::Cancel); // No conversion through floating-point identities.
  evidence            = Completed();
  evidence.generation = request.generation + 1;
  Check(Evaluate(request, evidence) == Decision::Cancel);
  evidence        = Completed();
  evidence.now_ms = request.deadline_ms;
  Check(Evaluate(request, evidence) == Decision::Cancel);
  evidence.now_ms = request.deadline_ms + 1;
  Check(Evaluate(request, evidence) == Decision::Cancel);
  evidence            = Completed();
  evidence.input_mode = false;
  Check(Evaluate(request, evidence) == Decision::Cancel); // Disable, native hotkey mode and Settings capture.
  evidence             = Completed();
  evidence.owner_alive = false;
  Check(Evaluate(request, evidence) == Decision::Cancel);
  evidence         = Completed();
  evidence.section = request.section + 1;
  Check(Evaluate(request, evidence) == Decision::Cancel);
  evidence.section.reset();
  Check(Evaluate(request, evidence) == Decision::Cancel);
}

void CheckedNativeObservations()
{
  const auto request = RequestForTest();
  for (auto field : {&Evidence::destination_matches, &Evidence::can_tow, &Evidence::can_be_towed,
                     &Evidence::course_valid, &Evidence::course_destination_matches}) {
    auto evidence   = Completed();
    evidence.*field = false;
    Check(Evaluate(request, evidence) == Decision::Cancel);
    (evidence.*field).reset();
    Check(Evaluate(request, evidence) == Decision::Cancel);
  }
  auto evidence          = Completed();
  evidence.course_in_use = true;
  Check(Evaluate(request, evidence) == Decision::Cancel);
  evidence.course_in_use.reset();
  Check(Evaluate(request, evidence) == Decision::Cancel);

  // A terminal iterator alone proves neither success nor ownership. The native
  // scheduler can leave -1 on stop/throw; failed/missing/old/wrong courses refuse.
  evidence              = Completed();
  evidence.course_valid = false;
  Check(evidence.iterator_state == -1 && Evaluate(request, evidence) == Decision::Cancel);
  evidence                            = Completed();
  evidence.course_destination_matches = false;
  Check(Evaluate(request, evidence) == Decision::Cancel);

  for (auto field : {&Request::selected, &Request::discovery}) {
    auto invalid   = request;
    invalid.*field = 0;
    Check(Evaluate(invalid, Completed()) == Decision::Cancel);
    invalid.*field = -1;
    Check(Evaluate(invalid, Completed()) == Decision::Cancel);
  }
  auto invalid     = request;
  invalid.selected = invalid.discovery;
  Check(Evaluate(invalid, Completed()) == Decision::Cancel);
  invalid                = request;
  invalid.selected_index = invalid.discovery_index;
  Check(Evaluate(invalid, Completed()) == Decision::Cancel);
  invalid                 = request;
  invalid.discovery_index = -1;
  Check(Evaluate(invalid, Completed()) == Decision::Cancel);
  invalid                 = request;
  invalid.selected_object = invalid.discovery_object;
  Check(Evaluate(invalid, Completed()) == Decision::Cancel);
  invalid       = request;
  invalid.owner = 0;
  Check(Evaluate(invalid, Completed()) == Decision::Cancel);
}
} // namespace

int main()
{
  WaitingAndCompletion();
  IdentityAndLifetime();
  CheckedNativeObservations();
  std::cout << "Discovery tow policy passed: " << checks << " checks; Unity alone advances the iterator\n";
}
