#pragma once

// The native menus use different BDA directions and negate their complete
// comparator for Descending. Keep the source chain available to a game-free
// characterization fixture; menu construction and native ownership stay in officers.cc.
namespace officer_sort
{
using Comparer = int (*)(void*, void*, void*);

inline int Compare(void* left, void* right, Comparer unlocked, Comparer below_deck, Comparer index, int index_sign)
{
  if (unlocked)
    if (const int order = unlocked(left, right, nullptr))
      return order;
  if (below_deck)
    if (const int order = below_deck(left, right, nullptr))
      return order;
  return index ? index_sign * index(left, right, nullptr) : 0;
}
} // namespace officer_sort
