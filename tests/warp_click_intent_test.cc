#include "warp_click_intent.h"

#include <initializer_list>
#include <stdexcept>

void Check(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}

int main()
{
  constexpr int64_t fleet  = 9007199254740993LL;
  constexpr int64_t target = 9007199254740995LL;
  WarpClickIntent   intent;
  Check(!intent.Consume(true, fleet, target), "ordinary prompt uses configured policy");
  intent.Begin(true, fleet, target);
  // Ctrl may be released during asynchronous route planning. Consumption has
  // no keyboard-state argument and exact 64-bit identities remain authoritative.
  Check(intent.Consume(true, fleet, target), "modified trip preserves the manual choice");
  Check(!intent.Consume(true, fleet, target), "first popup consumes the request once");
  intent.Begin(true, fleet, target);
  Check(!intent.Consume(true, fleet + 1, target), "different fleet does not inherit choice");
  Check(!intent.Consume(true, fleet, target), "mismatched popup also clears request");
  intent.Begin(true, fleet, target);
  Check(!intent.Consume(true, fleet, target + 1), "different target does not inherit choice");
  intent.Begin(true, fleet, target);
  intent.Begin(false, fleet, target);
  Check(!intent.Consume(true, fleet, target), "ordinary next move replaces modified intent");
  intent.Begin(true, fleet, target);
  intent.Begin(true, fleet + 1, target + 1);
  Check(intent.Consume(true, fleet + 1, target + 1), "latest modified move owns request");
  intent.Begin(true, fleet, target);
  Check(intent.Consume(false, 0, 0), "unavailable popup identity cannot auto-select a paid option");
  Check(!intent.Consume(true, fleet, target), "unavailable popup consumes once");
  for (const auto invalid : {int64_t{0}, int64_t{-1}}) {
    intent.Begin(true, invalid, target);
    Check(intent.Consume(true, fleet, target), "unresolved modified fleet keeps native choice");
    intent.Begin(true, fleet, invalid);
    Check(intent.Consume(false, 0, 0), "unresolved modified target keeps native choice");
    intent.Begin(false, invalid, invalid);
    Check(!intent.Consume(false, 0, 0), "ordinary invalid request cannot invent override");
  }
}
