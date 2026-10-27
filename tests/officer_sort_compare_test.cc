#include "officer_sort_compare.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
struct Officer {
  int unlocked, below_deck, index;
};
int CompareValue(int a, int b)
{ return (a > b) - (a < b); }
int Unlocked(void* a, void* b, void*)
{ return -CompareValue(static_cast<Officer*>(a)->unlocked, static_cast<Officer*>(b)->unlocked); }
int BelowAscending(void* a, void* b, void*)
{ return CompareValue(static_cast<Officer*>(a)->below_deck, static_cast<Officer*>(b)->below_deck); }
int BelowDescending(void* a, void* b, void*)
{ return -BelowAscending(a, b, nullptr); }
int Index(void* a, void* b, void*)
{ return CompareValue(static_cast<Officer*>(a)->index, static_cast<Officer*>(b)->index); }
officer_sort::Comparer unlocked = nullptr, index = nullptr;

// The four source wrapper bodies are independent oracles for the shared chain.
// Frozen officer_sort.cc supplies the roster reversal and assignment direction;
// native SortComparer then optionally negates the complete selected comparator.
int RosterAscending(void* a, void* b)
{
  if (unlocked) {
    const int r = unlocked(a, b, nullptr);
    if (r)
      return r;
  }
  if (const int r = BelowDescending(a, b, nullptr))
    return r;
  return index ? -index(a, b, nullptr) : 0;
}
int RosterDescending(void* a, void* b)
{
  if (unlocked) {
    const int r = unlocked(a, b, nullptr);
    if (r)
      return r;
  }
  if (const int r = BelowAscending(a, b, nullptr))
    return r;
  return index ? -index(a, b, nullptr) : 0;
}
int AssignmentAscending(void* a, void* b)
{
  if (unlocked) {
    const int r = unlocked(a, b, nullptr);
    if (r)
      return r;
  }
  if (const int r = BelowAscending(a, b, nullptr))
    return r;
  return index ? index(a, b, nullptr) : 0;
}
int AssignmentDescending(void* a, void* b)
{
  if (unlocked) {
    const int r = unlocked(a, b, nullptr);
    if (r)
      return r;
  }
  if (const int r = BelowDescending(a, b, nullptr))
    return r;
  return index ? index(a, b, nullptr) : 0;
}
} // namespace

int main()
{
  std::array<Officer, 6> officers{{{1, 1, 1}, {1, 0, 2}, {0, 1, 3}, {0, 0, 4}, {1, 1, 5}, {1, 1, 1}}};
  unsigned               comparisons = 0;
  for (int optional = 0; optional < 4; ++optional) {
    unlocked = (optional & 1) ? Unlocked : nullptr;
    index    = (optional & 2) ? Index : nullptr;
    for (const bool roster : {false, true})
      for (const bool ascending_delegate : {false, true})
        for (auto& a : officers)
          for (auto& b : officers) {
            const auto reference = roster ? (ascending_delegate ? RosterAscending : RosterDescending)
                                          : (ascending_delegate ? AssignmentAscending : AssignmentDescending);
            const auto bda       = roster == ascending_delegate ? BelowDescending : BelowAscending;
            const int  actual    = officer_sort::Compare(&a, &b, unlocked, bda, index, roster ? -1 : 1);
            for (const int direction : {1, -1}) {
              if (direction * actual != direction * reference(&a, &b))
                throw std::runtime_error("source Below Deck comparator parity failed");
              ++comparisons;
            }
          }
  }
  if (officer_sort::Compare(nullptr, nullptr, nullptr, nullptr, nullptr, 1) != 0)
    throw std::runtime_error("unavailable optional comparers must not dereference observations");
  std::cout << "Below Deck source comparator parity passed: " << comparisons
            << " roster/assignment, direction, locked, tie and optional-comparer cases\n";
}
