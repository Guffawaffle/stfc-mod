// Pure synthetic envelope/admission checks. No sync target, HTTP client, game,
// account state or diagnostic producer is started by this fixture.
#include "sync/diagnostic.h"
#include "sync/input_queue.h"

#include <cstdlib>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#define CHECK(expression)                                                                                              \
  do {                                                                                                                 \
    if (!(expression)) {                                                                                               \
      std::cerr << __FILE__ << ':' << __LINE__ << ": " #expression " failed\n";                                        \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (false)

using namespace data_sync;
using nlohmann::json;
using namespace std::chrono_literals;

int main()
{
  json       event    = {{"seq", std::numeric_limits<uint64_t>::max()},
                         {"utc_ms", std::numeric_limits<int64_t>::min()},
                         {"session", "synthetic-session"},
                         {"phase", "synthetic-observation"},
                         {"value", ""}};
  const auto original = event;
  const auto encoded  = DiagnosticEnvelope("synthetic-stream", event, "fixture-source", "fixture-version");
  CHECK(encoded);
  const auto envelope = json::parse(*encoded).at(0);
  CHECK(envelope.at("type") == "diagnostic" && envelope.at("schema") == 1);
  CHECK(envelope.at("source_id") == "fixture-source" && envelope.at("producer_version") == "fixture-version");
  CHECK(envelope.at("stream") == "synthetic-stream" && envelope.at("session") == "synthetic-session");
  CHECK(envelope.at("sequence") == "18446744073709551615");
  CHECK(envelope.at("utc_ms") == "-9223372036854775808");
  CHECK(envelope.at("event") == "synthetic-observation" && envelope.at("payload") == original && event == original);

  event["event"]  = "preferred-event";
  event["utc_ms"] = 1.5;
  auto result     = DiagnosticEnvelope("synthetic-stream", event, "fixture-source", "fixture-version");
  CHECK(result && json::parse(*result).at(0).at("event") == "preferred-event");
  CHECK(json::parse(*result).at(0).at("utc_ms").is_null());
  event.erase("event");
  event.erase("phase");
  event.erase("utc_ms");
  result = DiagnosticEnvelope("synthetic-stream", event, "fixture-source", "fixture-version");
  CHECK(result && json::parse(*result).at(0).at("event") == "observation");
  CHECK(json::parse(*result).at(0).at("utc_ms").is_null());
  event["seq"] = int64_t{1};
  CHECK(!DiagnosticEnvelope("synthetic", event, "fixture", "fixture"));
  event["seq"] = 1.0;
  CHECK(!DiagnosticEnvelope("synthetic", event, "fixture", "fixture"));
  event.erase("seq");
  CHECK(!DiagnosticEnvelope("synthetic", event, "fixture", "fixture"));
  CHECK(!DiagnosticEnvelope("synthetic", json::array(), "fixture", "fixture"));

  event            = {{"seq", uint64_t{1}}, {"value", ""}};
  const auto empty = DiagnosticEnvelope("s", event, "p", "v");
  CHECK(empty && empty->size() < kDiagnosticItemBytes);
  event["value"] = std::string(kDiagnosticItemBytes - empty->size(), 'x');
  result         = DiagnosticEnvelope("s", event, "p", "v");
  CHECK(result && result->size() == kDiagnosticItemBytes);
  event["value"].get_ref<std::string&>().push_back('x');
  CHECK(!DiagnosticEnvelope("s", event, "p", "v"));
  event["value"] = std::string(1024 * 1024, 'x');
  CHECK(!DiagnosticEnvelope("s", event, "p", "v"));
  event["value"] = std::string(1, static_cast<char>(0xff));
  CHECK(!DiagnosticEnvelope("s", event, "p", "v"));

  DiagnosticBudget budget;
  for (size_t i = 0; i < kDiagnosticCount; ++i)
    CHECK(budget.Reserve(1));
  CHECK(!budget.Reserve(1) && budget.Count() == kDiagnosticCount && budget.Bytes() == kDiagnosticCount);
  for (size_t i = 0; i < kDiagnosticCount; ++i)
    budget.Release(1);
  CHECK(budget.Count() == 0 && budget.Bytes() == 0);
  CHECK(budget.Reserve(kDiagnosticBytes));
  CHECK(!budget.Reserve(1) && budget.Count() == 1 && budget.Bytes() == kDiagnosticBytes);
  budget.Release(kDiagnosticBytes);
  try {
    CHECK(budget.Reserve(64));
    DiagnosticCharge reservation{budget, 64};
    throw std::runtime_error("synthetic dispatch failure");
  } catch (const std::runtime_error&) {
  }
  CHECK(budget.Count() == 0 && budget.Bytes() == 0);
  CHECK(!budget.Reserve(0) && !budget.Reserve(std::numeric_limits<size_t>::max()));

  sync_queue::BoundedQueue<int> queue{{2, 8, 4}};
  std::promise<void>            entered, release;
  auto                          permission = release.get_future().share();
  auto                          producer   = std::async(std::launch::async, [&] {
    return queue.TryEmplace(4, [&] {
      entered.set_value();
      permission.wait();
      return 7;
    });
  });
  CHECK(entered.get_future().wait_for(2s) == std::future_status::ready);
  CHECK(!queue.TryPop());
  CHECK(queue.TryEmplace(4, [] { return 8; }) == sync_queue::Admission::accepted);
  CHECK(!queue.TryPop()); // Ready later entry cannot pass the earlier reservation.
  release.set_value();
  CHECK(producer.wait_for(2s) == std::future_status::ready && producer.get() == sync_queue::Admission::accepted);
  {
    auto first = queue.TryPop();
    CHECK(first && first->value == 7 && !queue.TryPop());
    CHECK(queue.Stats().count == 2 && queue.Stats().bytes == 8);
  }
  {
    auto second = queue.TryPop();
    CHECK(second && second->value == 8);
  }
  CHECK(!queue.TryPop() && queue.Stats().count == 0 && queue.Stats().bytes == 0);
  CHECK(queue.TryEmplace(4, []() -> int { throw std::bad_alloc(); }) == sync_queue::Admission::failed);
  CHECK(!queue.TryPop() && queue.Stats().count == 0 && queue.Stats().bytes == 0);
  std::cout << "Pure diagnostic envelope, budget and nonblocking queue checks passed\n";
}
