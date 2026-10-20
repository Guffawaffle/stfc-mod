#include "patches/parts/battle_log_summary_data.h"

#include <cassert>
#include <iostream>
#include <limits>

int main()
{
  using battle_summary::Summary;
  Summary s;
  assert(s.outgoing.average() == 0 && s.per_round() == 0 && s.incoming.critical_percent() == 0);
  assert(s.attack(true, false, true, 100, 50));
  assert(s.attack(true, false, false, 30, 20));
  assert(s.attack(false, true, false, 70, 10));
  for (int participant = 0; participant < 6; ++participant) {
    assert(s.attack(false, false, true, 9000000000000000000LL, 9000000000000000000LL));
    assert(s.repair(false, 1e12f));
  }
  s.rounds = 2;
  assert(s.outgoing.shots == 2 && s.outgoing.critical == 1);
  assert(s.outgoing.landed() == 200 && s.outgoing.average() == 100);
  assert(s.outgoing.critical_percent() == 50 && s.per_round() == 100);
  assert(s.incoming.shots == 1 && s.incoming.landed() == 80);
  assert(s.repair(true, 45) && s.repairs == 45);
  assert(!s.attack(true, false, false, -1, 2) && s.outgoing.shots == 2);
  assert(!s.repair(true, std::numeric_limits<float>::quiet_NaN()));
  assert(!s.repair(true, -1) && s.repairs == 45);
  assert(s.rejected == 3);

  Summary large;
  for (int i = 0; i < 4; ++i)
    assert(large.attack(true, false, false, 9000000000000000000LL, 9000000000000000000LL));
  assert(large.outgoing.landed() > static_cast<long double>(std::numeric_limits<std::int64_t>::max()));
  assert(large.attack(true, true, false, 1, 2));
  assert(large.incoming.landed() == 3);

  // Independently summed from the user's public reference CSV: the chart's
  // 229.89T raw damage is NOT the 36.66T hull+shield total we label Landed.
  Summary sample;
  sample.rounds            = 2;
  sample.outgoing.shots    = 16;
  sample.outgoing.critical = 16;
  sample.outgoing.hull     = 22619345911808.0L;
  sample.outgoing.shield   = 14041720946688.0L;
  sample.incoming.hull     = 54429619456.0L;
  sample.incoming.shots    = 15;
  assert(sample.outgoing.landed() == 36661066858496.0L);
  assert(sample.outgoing.average() == 2291316678656.0L);
  assert(sample.per_round() == 18330533429248.0L);
  assert(sample.outgoing.critical_percent() == 100);
  std::cout
      << "PASS: local-player filter, six Armada allies, damage/crit averages, repairs, invalid input, wide totals\n";
}
