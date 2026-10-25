// Actual production uploader/CPR over the owned Python fixture listener only.
// No configuration files, game hooks, profile/account stores or game server.
#include "sync_diagnostics.h"
#include "sync_fixture_state.h"

#include <cpr/cpr.h>

#include <algorithm>
#include <charconv>
#include <iostream>
#include <thread>

using namespace data_sync;
using sync_fixture::Check;
using sync_fixture::state;
using namespace std::chrono_literals;

namespace
{
std::string base, key;

json Control(const char* path, bool post = false)
{
  const cpr::Url    url{base + path};
  const cpr::Header headers{{"stfc-sync-token", key}};
  const auto        response =
      post ? cpr::Post(url, headers, cpr::Timeout{2000}, cpr::ConnectTimeout{500}, cpr::Redirect{false})
           : cpr::Get(url, headers, cpr::Timeout{2000}, cpr::ConnectTimeout{500}, cpr::Redirect{false});
  Check(response.status_code == 200, "owned loopback control failed");
  return json::parse(response.text);
}

template <typename Predicate> void Wait(Predicate&& predicate, const char* message)
{
  const auto deadline = Clock::now() + 8s;
  while (!predicate()) {
    Check(Clock::now() < deadline, message);
    std::this_thread::sleep_for(10ms);
  }
}

class Uploader
{
public:
  std::shared_ptr<Destination> destination = std::make_shared<Destination>();

  explicit Uploader(const std::string& path)
  {
    SyncTargetConfig options;
    options.url         = base + '/' + path;
    options.token       = key; // Generated synthetic fixture key, never a user token.
    options.resources   = true;
    options.diagnostics = true;
    destination->delivery.Configure(options);
    {
      std::scoped_lock lock(state.mutex);
      state.targets.emplace(path, destination);
    }
    worker_ = std::thread(UploadTarget, std::weak_ptr(destination), path);
  }

  ~Uploader()
  {
    {
      std::scoped_lock lock(state.mutex);
      destination->unavailable = true;
      destination->delivery.Retire();
      destination->diagnostics.Close();
      std::erase_if(state.targets, [&](const auto& entry) { return entry.second == destination; });
    }
    state.ready.notify_all();
    if (worker_.joinable())
      worker_.join();
  }

private:
  std::thread worker_;
};

void Resource(int amount)
{
  {
    std::scoped_lock lock(state.mutex);
    state.records.Apply(Stream::kResources, {{7, {{"rid", 7}, {"amount", amount}}}}, Update::kFull);
  }
  state.ready.notify_all();
}

bool Accepted(Destination& destination)
{
  std::scoped_lock lock(state.mutex);
  // Test-only planner observation can advance attempted bookkeeping, never ACK.
  // Bypass backoff: a future retry deadline must not masquerade as acceptance.
  // This fixture's sole resource key has already been attempted by the worker.
  return !destination.delivery.Next(state.records, Clock::time_point::max());
}

void RetryAndBestEffort()
{
  sync_fixture::Fresh();
  Uploader upload("retry");
  Resource(10);
  Wait([] { return Control("/control/state").at("retry").at("ordinary").get<int>() >= 2; }, "ordinary retry missing");
  Wait([&] { return Accepted(*upload.destination); }, "successful ordinary response was not acknowledged");
  PublishDiagnosticEvent("fixture", json{{"seq", uint64_t{1}}, {"session", "synthetic"}, {"event", "sample"}});
  Wait([] { return Control("/control/state").at("retry").at("diagnostic").get<int>() == 1; },
       "diagnostic attempt missing");
  Wait([&] { return upload.destination->diagnostics.Stats().count == 0; }, "diagnostic HTTP lease did not release");
  Check(state.diagnostic_drops.load() > 0, "failed best-effort diagnostic was not counted");
  Resource(11);
  Wait([] { return Control("/control/state").at("retry").at("ordinary").get<int>() >= 3; },
       "ordinary delivery stalled");
  Wait([&] { return Accepted(*upload.destination); }, "ordinary delivery after diagnostic did not acknowledge");
  Check(Control("/control/state").at("retry").at("diagnostic") == 1, "diagnostic failure was retried/coalesced");
}

void SealDuringResponse()
{
  sync_fixture::Fresh();
  Uploader upload("blocked");
  Resource(20);
  Wait([] { return Control("/control/state").at("blocked").at("entered").get<bool>(); },
       "response barrier not reached");
  const auto generation = state.generation.load();
  SealOrdinarySession(generation);
  Control("/control/release", true);
  // This tail observation is processed by the same uploader only after the
  // held ordinary response returns. It proves diagnostics remain independent.
  PublishDiagnosticEvent("fixture", json{{"seq", uint64_t{2}}, {"session", "synthetic"}, {"event", "tail"}});
  Wait([] { return Control("/control/state").at("blocked").at("diagnostic").get<int>() == 1; },
       "diagnostic tail missing");
  Wait([&] { return upload.destination->diagnostics.Stats().count == 0; }, "tail HTTP lease did not release");
  Check(!Accepted(*upload.destination), "held response acknowledged after the generation was sealed");
  Check(!OrdinarySessionOpen(generation), "sealed ordinary generation reopened");
}
} // namespace

int main(int argc, char** argv)
{
  try {
    Check(argc == 3, "fixture requires its owned loopback URL and key");
    base                              = argv[1];
    key                               = argv[2];
    constexpr std::string_view prefix = "http://127.0.0.1:";
    Check(base.starts_with(prefix) && key.size() == 32
              && std::all_of(key.begin(), key.end(),
                             [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }),
          "only the generated local fixture selector is accepted");
    const auto port   = std::string_view(base).substr(prefix.size());
    unsigned   number = 0;
    const auto parsed = std::from_chars(port.data(), port.data() + port.size(), number);
    Check(parsed.ec == std::errc{} && parsed.ptr == port.data() + port.size() && number > 0 && number <= 65535,
          "invalid loopback port");
    Check(Control("/control/state").at("fixture") == key, "listener is not this owned synthetic fixture");
    RetryAndBestEffort();
    SealDuringResponse();
    Check(state.targets.empty() && state.diagnostic_budget.Count() == 0, "fixture ownership did not drain");
    std::cout << "Production sync uploader loopback checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
