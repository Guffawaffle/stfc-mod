#include "features/fleet_opc_sample.h"
#include <cassert>

int main()
{
  using namespace fleet_opc;
  SampleCache cache;
  int         reads  = 0;
  auto        reader = [&] {
    ++reads;
    return Cargo{true, false, 99, 100};
  };
  cache.Read(0, 1, 2, 4, 1000, reader);
  cache.Read(0, 1, 2, 4, 1999, reader);
  assert(reads == 1);
  cache.Read(0, 1, 2, 4, 2000, reader);
  assert(reads == 2);
  cache.Read(0, 1, 3, 4, 2001, reader);
  assert(reads == 3);
  cache.Invalidate(0);
  cache.Read(0, 1, 3, 4, 2002, reader);
  assert(reads == 4);
  assert(!cache.Read(-1, 1, 2, 4, 2003, reader).known);
  assert(reads == 4);

  const Cargo protected_cargo{true, false, 99, 100};
  assert(Display(Evaluate(true, protected_cargo, true, 1, true, 0, 1)) == "SAFE");
  assert(Display(Evaluate(true, protected_cargo, true, 1, true, 0, 2)) == "<1m");
  assert(Display(Evaluate(true, {true, false, 100, 100}, true, 1, true, 0, 2)) == "<1m");
  assert(Display(Evaluate(true, {true, true, 101, 100}, true, 1, true, 0, 2)) == "OPC");
  assert(Display(Evaluate(false, protected_cargo, true, 1, true, 0, 2)).empty());
  assert(Display(Evaluate(true, {}, true, 1, true, 0, 2)).empty());
  assert(Display(Evaluate(true, protected_cargo, true, 0, true, 0, 2)).empty());
  assert(Display(Evaluate(true, protected_cargo, false, 1, true, 0, 2)).empty());
  assert(CardDisplay(Evaluate(true, protected_cargo, false, 1, true, 0, 2)) == "OPC: Unknown");
  assert(!Evaluate(true, protected_cargo, true, 1, true, 3, 2).node_known);
  assert(Duration(60) == "1m");
  assert(Duration(61) == "2m");
  assert(Duration(3601) == "1h 1m");
  assert(Duration(86400) == "1d");
}
