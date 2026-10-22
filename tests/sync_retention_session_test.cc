#include "patches/parts/sync_resolver_cache.h"
#include "patches/parts/sync_session_headers.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#define CHECK(expression)                                                                                              \
  do {                                                                                                                 \
    if (!(expression)) {                                                                                               \
      std::cerr << __FILE__ << ':' << __LINE__ << ": " #expression " failed\n";                                        \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (false)

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
struct Profile {
  std::string       name;
  Clock::time_point expires_at;
};

static void Retention()
{
  const auto                                 now = Clock::time_point{} + 1h;
  sync_resolver::Cache<std::string, Profile> small(3);
  small.Put("old", {"old", now + 10s}, now);
  small.Put("hot", {"hot", now + 10s}, now);
  small.Put("new", {"new", now + 10s}, now);
  CHECK(small.Find("hot", now));
  small.Put("fourth", {"fourth", now + 10s}, now);
  CHECK(small.Size() == 3 && !small.Find("old", now));
  small.Put("hot", {"renamed", now + 30s}, now);
  CHECK(small.Size() == 3 && small.Find("hot", now)->name == "renamed");
  // Forgotten identities must retire on unrelated background maintenance.
  small.Prune(now + 10s);
  CHECK(small.Size() == 1 && small.Find("hot", now + 10s));
  small.Prune(now + 30s);
  CHECK(small.Size() == 0);
  small.Put("expired", {"expired", now}, now);
  CHECK(small.Size() == 0);
  sync_resolver::Cache<int, Profile> disabled(0);
  disabled.Put(1, {"unused", now + 10s}, now);
  CHECK(disabled.Size() == 0);

  sync_resolver::Cache<std::string, Profile> population;
  for (int id = 0; id < 30000; ++id) {
    population.Put(std::to_string(id), {std::string(256, 'n'), now + 10s}, now);
    CHECK(population.Size() <= sync_resolver::kEntryLimit);
  }
  CHECK(population.Size() == 4096 && !population.Find("0", now));
  population.Prune(now + 10s);
  CHECK(population.Size() == 0);
  CHECK(sync_resolver::BoundedField(std::string(256, 'x')));
  CHECK(!sync_resolver::BoundedField(std::string(257, 'x')));
}

struct FallibleProfile {
  Clock::time_point  expires_at;
  static inline bool fail = false;
  FallibleProfile(Clock::time_point expiry)
      : expires_at(expiry)
  {
  }
  FallibleProfile(const FallibleProfile&) = default;
  FallibleProfile(FallibleProfile&& other)
      : expires_at(other.expires_at)
  {
    if (fail)
      throw std::runtime_error("fixture move failure");
  }
  FallibleProfile& operator=(FallibleProfile&&) = default;
};

static void AdmissionFailure()
{
  const auto                                 now = Clock::time_point{};
  sync_resolver::Cache<int, FallibleProfile> cache(1);
  FallibleProfile::fail = true;
  bool threw            = false;
  try {
    cache.Put(1, FallibleProfile(now + 1h), now);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  CHECK(threw && cache.Size() == 0);
  FallibleProfile::fail = false;
  cache.Put(2, FallibleProfile(now + 1h), now);
  cache.Put(3, FallibleProfile(now + 1h), now);
  CHECK(cache.Size() == 1 && cache.Find(3, now) && !cache.Find(2, now));
}

static void SessionPublication()
{
  sync_session::Headers headers;
  headers.SetServer("server-a", "session-a");
  headers.SetVersion("version-a");
  headers.SetInstance(7);
  const auto request = headers.Read();
  headers.SetServer("server-a", "session-a");
  CHECK(headers.IsCurrent(request.generation));
  headers.SetServer("server-b", "session-b");
  CHECK(!headers.IsCurrent(request.generation));
  CHECK(request.gameServerUrl == "server-a" && request.instanceSessionId == "session-a");
  CHECK(request.primeVersion == "version-a" && request.instanceId == 7);
  // Capturing/copying a request snapshot must not retain the publication lock
  // through a blocked network request; setters continue while request is held.
  headers.SetVersion("version-b");
  headers.SetInstance(8);
  CHECK(headers.Read().primeVersion == "version-b" && headers.Read().instanceId == 8);

  std::atomic_bool finished{false};
  std::atomic_bool coherent{true};
  std::thread      publisher([&] {
    for (int i = 0; i < 30000; ++i) {
      const auto identity = std::to_string(i);
      headers.SetServer(identity, identity);
    }
    finished.store(true, std::memory_order_release);
  });
  while (!finished.load(std::memory_order_acquire)) {
    const auto snapshot = headers.Read();
    if (snapshot.gameServerUrl != snapshot.instanceSessionId && snapshot.gameServerUrl != "server-b")
      coherent.store(false);
  }
  publisher.join();
  CHECK(coherent.load());
  const auto final = headers.Read();
  CHECK(final.gameServerUrl == "29999" && final.instanceSessionId == "29999");
  CHECK(headers.Generation() == final.generation);
}

static void OrdinaryGenerationPolicy()
{
  sync_session::OrdinarySessionBinding stream;
  CHECK(!stream.SessionChanged(2)); // Initial login while unbound is harmless.
  CHECK(stream.Generation() == 0 && !stream.Quarantined());
  CHECK(stream.Admit(2, 2) && stream.Admit(2, 2));
  CHECK(stream.Generation() == 2 && !stream.SessionChanged(2));
  CHECK(stream.SessionChanged(3));
  CHECK(!stream.Admit(2, 2) && !stream.Admit(3, 3));
  CHECK(stream.Quarantined() && stream.Generation() == 2);

  sync_session::OrdinarySessionBinding staleFirst;
  CHECK(!staleFirst.Admit(4, 5));
  CHECK(staleFirst.Generation() == 4 && staleFirst.Quarantined());
  CHECK(!staleFirst.Admit(4, 4) && !staleFirst.Admit(5, 5));

  sync_session::OrdinarySessionBinding delayedFirst;
  CHECK(!delayedFirst.SessionChanged(7));
  CHECK(!delayedFirst.Admit(6, 6)); // A delayed caller cannot undo observed login.
  CHECK(delayedFirst.Generation() == 6 && delayedFirst.Quarantined());

  for (int run = 0; run < 64; ++run) {
    sync_session::OrdinarySessionBinding concurrent;
    std::atomic_bool                     start{false};
    std::thread                          old([&] {
      while (!start.load(std::memory_order_acquire)) {}
      concurrent.Admit(10, 10);
    });
    std::thread                          next([&] {
      while (!start.load(std::memory_order_acquire)) {}
      concurrent.Admit(11, 11);
    });
    start.store(true, std::memory_order_release);
    old.join();
    next.join();
    CHECK(concurrent.Quarantined());
    CHECK(!concurrent.Admit(10, 10) && !concurrent.Admit(11, 11));
  }
}

int main()
{
  Retention();
  AdmissionFailure();
  SessionPublication();
  OrdinaryGenerationPolicy();
  std::cout << "sync resolver retention and session publication fixtures passed\n";
}
