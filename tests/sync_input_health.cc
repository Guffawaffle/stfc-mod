#include "sync/input_health.h"
#include "sync/input_queue.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

#define CHECK(expression)                                                                                              \
  do {                                                                                                                 \
    if (!(expression)) {                                                                                               \
      std::cerr << __FILE__ << ':' << __LINE__ << ": " #expression " failed\n";                                        \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (false)

using data_sync::InputCharge;
using data_sync::InputHealth;
using sync_queue::Admission;

int main()
{
  InputHealth health;
  CHECK(!health.Allows(0));
  CHECK(health.Allows(1));
  CHECK(health.Seal(1));
  CHECK(!health.Allows(1) && !health.Seal(1));
  CHECK(health.Allows(2));
  CHECK(health.Seal(2));
  CHECK(!health.Seal(1) && health.SealedThrough() == 2);
  CHECK(!health.Allows(1) && !health.Allows(2) && health.Allows(3));
  health.Stop();
  CHECK(health.Stopped() && !health.Allows(3) && !health.Allows(100));

  InputHealth session_health;
  int         original_calls = 0;
  int         metadata_value = 0;
  CHECK(data_sync::TrySessionUpdate(session_health, [&] { metadata_value = 7; }));
  CHECK(metadata_value == 7 && session_health.Allows(1));
  const auto start_hook = [&] {
    const auto updated = data_sync::TrySessionUpdate(session_health, [&] { throw std::bad_alloc(); });
    // Matches the host's source boundary: never catch or retry the game call.
    ++original_calls;
    return updated;
  };
  CHECK(!start_hook() && original_calls == 1);
  CHECK(session_health.Stopped() && !session_health.Allows(1) && !session_health.Allows(100));
  InputHealth game_exception_health;
  bool        game_exception_propagated = false;
  try {
    CHECK(data_sync::TrySessionUpdate(game_exception_health, [] {}));
    ++original_calls;
    throw std::runtime_error("game original exception");
  } catch (const std::runtime_error&) {
    game_exception_propagated = true;
  }
  CHECK(game_exception_propagated && original_calls == 2 && !game_exception_health.Stopped());

  InputHealth failed_worker, exited_worker;
  int         worker_calls = 0;
  data_sync::ContainWorkerExit(failed_worker, [&] {
    ++worker_calls;
    throw std::bad_alloc();
  });
  CHECK(worker_calls == 1 && failed_worker.Stopped() && !failed_worker.Allows(100));
  data_sync::ContainWorkerExit(exited_worker, [&] { ++worker_calls; });
  CHECK(worker_calls == 2 && exited_worker.Stopped() && !exited_worker.Allows(100));

  CHECK(InputCharge(0, 0, 80) == 80); // Empty protobuf can be a valid empty full list.
  CHECK(InputCharge(4, 2, 80) == 90);
  CHECK(InputCharge(data_sync::kInputItemBytes - 80, 0, 80) == data_sync::kInputItemBytes);
  CHECK(!InputCharge(data_sync::kInputItemBytes - 79, 0, 80));
  CHECK(!InputCharge(std::numeric_limits<size_t>::max(), 0, 80));
  CHECK(!InputCharge(0, std::numeric_limits<size_t>::max(), 80));
  CHECK(!InputCharge(0, 0, std::numeric_limits<size_t>::max()));

  // Exercise the exact reservation primitive and health gate together. Native
  // host wiring and real processor paths require the separate sync fixture.
  sync_queue::BoundedQueue<std::string> queue{{1, 8, 8}};
  InputHealth                           generation;
  int                                   copies = 0;
  const auto                            first  = queue.TryEmplace(8, [&] {
    ++copies;
    return std::string("owned");
  });
  CHECK(first == Admission::accepted && copies == 1);
  {
    auto owned = queue.WaitPop();
    CHECK(owned && owned->value == "owned");
    CHECK(queue.Stats().count == 1 && queue.Stats().bytes == 8);
    const auto rejected = queue.TryEmplace(1, [&] {
      ++copies;
      return std::string("lost");
    });
    CHECK(rejected == Admission::full && copies == 1);
    generation.Seal(7);
    CHECK(!generation.Allows(7));
  }
  CHECK(queue.Stats().count == 0 && queue.Stats().bytes == 0);
  CHECK(generation.Allows(8));
  const auto failed = queue.TryEmplace(1, [&]() -> std::string {
    ++copies;
    throw std::bad_alloc();
  });
  CHECK(failed == Admission::failed && copies == 2);
  generation.Seal(8);
  queue.Close();
  CHECK(!queue.WaitPop());
  CHECK(!generation.Allows(8) && generation.Allows(9));
  CHECK(queue.Stats().count == 0 && queue.Stats().failures == 1);
  std::cout << "Sync owned-input admission and generation health tests passed\n";
}
