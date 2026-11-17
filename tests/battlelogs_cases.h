#pragma once

// The actual request/retry/resolver implementation, over this executable's owned listener only.
#include "../src/mod/sync/battlelogs.cc"

namespace data_sync
{
uint64_t SessionGeneration()
{ return State().generation.load(); }
GameSession Session()
{
  std::scoped_lock lock(State().mutex);
  return State().session;
}
bool HasBattle(uint64_t id, uint64_t generation)
{
  std::scoped_lock lock(State().mutex);
  return OrdinarySessionOpen(generation) && State().records.Contains(Stream::kBattles, id);
}
void Publish(Stream stream, RecordSet records, uint64_t generation, Update mode)
{
  std::scoped_lock lock(State().mutex);
  if (OrdinarySessionOpen(generation))
    State().records.Apply(stream, std::move(records), mode);
}
} // namespace data_sync

namespace
{
void BattleInterest(bool enabled, bool keep_resource = true)
{
  auto config                = std::make_shared<Config::Sync>();
  config->options.resources  = keep_resource;
  config->options.battlelogs = enabled;
  config->resolver_cache_ttl = 0; // Every re-enabled request must exercise the real resolver transport.
  SyncTargetConfig resource;
  resource.url       = base + "/unused-resource";
  resource.token     = key;
  resource.resources = true;
  if (keep_resource)
    config->targets.emplace("resource", resource);
  if (enabled) {
    SyncTargetConfig battle;
    battle.url        = base + "/unused-battle";
    battle.token      = key;
    battle.battlelogs = true;
    config->targets.emplace("battle", battle);
  }
  std::atomic_store_explicit(&sync_fixture::settings, std::shared_ptr<const Config::Sync>{std::move(config)},
                             std::memory_order_release);
  state.ready.notify_all();
}
void FreshBattle()
{
  sync_fixture::Fresh();
  {
    std::scoped_lock lock(state.mutex);
    state.session.server_url  = base;
    state.session.session_id  = key; // Generated local-only identity, never game credentials.
    state.session.version     = "synthetic";
    state.session.instance_id = 7;
  }
  BattleInterest(true);
}
json BattleCounts()
{ return Control("/control/state").at("battle").at("counts"); }
void CheckBattleCounts(const json& expected, const char* message)
{ Check(BattleCounts() == expected, message); }

json BattleWire()
{
  SyncTargetConfig options;
  options.url        = base + "/unused-battle";
  options.token      = key;
  options.battlelogs = true;
  Delivery target;
  target.Configure(options);
  std::scoped_lock lock(state.mutex);
  auto             upload = target.Next(state.records, Clock::now());
  Check(upload && upload->stream == Stream::kBattles, "retained journal is not deliverable to a re-added target");
  const auto records = upload->Records();
  Check(records.size() == 1 && records[0].at("type") == "battlelog", "cached journal wire shape changed");
  return records[0];
}

class BattleThread
{
public:
  template <typename Work>
  explicit BattleThread(Work work)
      : worker_([this, work = std::move(work)] {
        try {
          work();
        } catch (...) {
          error_ = std::current_exception();
        }
      })
  {
  }
  void Join()
  {
    worker_.join();
    if (error_)
      std::rethrow_exception(error_);
  }
  ~BattleThread()
  {
    if (worker_.joinable())
      worker_.join();
  }

private:
  std::exception_ptr error_;
  std::thread        worker_;
};

void BattleRetryRemoval()
{
  FreshBattle();
  const auto generation = SessionGeneration();
  const auto before     = BattleCounts();
  QueueNewBattles({7001}, generation);
  ProcessBattle(battles.Pop()); // Listener deliberately rejects this first journal attempt.
  auto retry = battles.Pop();
  Check(retry.id == 7001 && retry.attempts == 1 && retry.retry_at > Clock::now(),
        "actual failed journal did not schedule its native retry");
  std::mutex              mutex;
  std::condition_variable ready;
  bool                    entered = false, released = false, before_deadline = false;
  retry.retry_at = Clock::now() + 3s;
  BattleThread worker([&] {
    int observations               = 0;
    sync_fixture::session_observer = [&] {
      if (++observations != 2)
        return; // Entry observation, then the actual retry-wait predicate.
      std::unique_lock lock(mutex);
      before_deadline = Clock::now() < retry.retry_at;
      entered         = true;
      ready.notify_all();
      ready.wait_for(lock, 2s, [&] { return released; });
    };
    ProcessBattle(retry);
    sync_fixture::session_observer = {};
  });
  {
    std::unique_lock lock(mutex);
    Check(ready.wait_for(lock, 2s, [&] { return entered; }) && before_deadline,
          "actual retry wait predicate was not observed before its deadline");
    BattleInterest(false); // Atomic config replacement; does not take state.mutex held by wait's predicate.
    released = true;
  }
  ready.notify_all();
  state.ready.notify_all();
  worker.Join();
  auto expected       = before;
  expected["journal"] = expected.at("journal").get<int>() + 1;
  CheckBattleCounts(expected, "removed battle target admitted a later retry/profile/alliance request");
  Check(OrdinarySessionOpen(generation) && !HasBattle(7001, generation),
        "canceled retry must neither seal unrelated sync nor fabricate a journal");
  {
    std::scoped_lock lock(battle_mutex);
    Check(!pending.contains({generation, 7001}), "canceled retry retained a wedged pending battle");
  }
  const auto settings = Config::Sync::Get();
  Check(settings->targets.contains("resource") && !settings->targets.contains("battle"),
        "unrelated resource destination must survive removal of the last battle destination");
  BattleInterest(true);
  QueueNewBattles({7001}, generation);
  ProcessBattle(battles.Pop());
  Check(HasBattle(7001, generation), "re-adding interest did not recover the same pending battle id");
  expected["journal"]  = expected.at("journal").get<int>() + 1;
  expected["profile"]  = expected.at("profile").get<int>() + 1;
  expected["alliance"] = expected.at("alliance").get<int>() + 1;
  CheckBattleCounts(expected, "re-enabled battle did not exercise journal/player/alliance requests");
  Check(BattleWire().at("names").at("player7001")
            == json{{"name", "Synthetic Player"}, {"alliance_name", "Synthetic Alliance"}, {"alliance_tag", "FX"}},
        "re-enabled journal did not retain resolved player/alliance wire names");
}

void BattleRequestAdmission(bool remove_during_setup)
{
  FreshBattle();
  const auto before  = BattleCounts();
  bool       removed = false;
  if (remove_during_setup) {
    sync_fixture::settings_observer = [&] {
      // The getter captured its old snapshot before invoking this callback.
      if (!std::exchange(removed, true))
        BattleInterest(false);
    };
  } else {
    BattleInterest(false);
  }
  const auto reply                = RequestGameData(Session(), "/journals/get", json{{"journal_id", 7301}}.dump());
  sync_fixture::settings_observer = {};
  Check(reply.empty(), "uninterested direct request was dispatched");
  CheckBattleCounts(before, "interest removal before transport admitted a journal request");
  Check(OrdinarySessionOpen(SessionGeneration()), "uninterested request sealed unrelated sync");
  BattleInterest(false, false);
  Check(Config::Sync::Get()->targets.empty(), "empty-target admission control retained a destination");
  Check(RequestGameData(Session(), "/journals/get", json{{"journal_id", 7301}}.dump()).empty(),
        "empty target registry admitted a direct journal request");
  CheckBattleCounts(before, "empty target registry issued a journal/profile/alliance request");
}

void BattleResponseRemoval(bool profile_response)
{
  FreshBattle();
  const auto generation = SessionGeneration();
  const auto before     = BattleCounts();
  const auto id         = profile_response ? 7201ULL : 7101ULL;
  QueueNewBattles({id}, generation);
  auto         battle = battles.Pop();
  BattleThread worker([&] { ProcessBattle(battle); });
  const auto   field = profile_response ? "profile_entered" : "journal_entered";
  Wait([&] { return Control("/control/state").at("battle").at(field).get<bool>(); },
       "synthetic battle response barrier was not reached");
  const auto at_removal = BattleCounts();
  BattleInterest(false);
  Control(profile_response ? "/control/release/battle-profile" : "/control/release/battle-journal", true);
  worker.Join();
  CheckBattleCounts(at_removal, "held battle response admitted a later player/alliance request after removal");
  Check(HasBattle(id, generation) && OrdinarySessionOpen(generation),
        "already-acquired successful journal must remain cached without sealing unrelated sync");
  const auto record = BattleWire();
  Check(record.at("journal") == json{{"initiator_id", "player" + std::to_string(id)}, {"target_id", "npc_fixture"}},
        "already-acquired journal changed before delivery");
  const auto names = profile_response ? json{{"player7201",
                                              {{"name", "Synthetic Player"},
                                               {"alliance_name", nullptr},
                                               {"alliance_tag", nullptr},
                                               {"alliance_id", 42}}}}
                                      : json::object();
  Check(record.at("names") == names,
        "retained journal lost empty-name or known-player/unresolved-alliance wire fallback");
  Check(at_removal.at("journal") == before.at("journal").get<int>() + 1,
        "held response case must actually issue its journal");
  Check(at_removal.at("profile") == before.at("profile").get<int>() + (profile_response ? 1 : 0),
        "profile barrier must follow exactly the intended synthetic requests");
}

void BattleLogsCases(std::string_view mode)
{
  if (mode == "battle-request")
    BattleRequestAdmission(false);
  else if (mode == "battle-prepost")
    BattleRequestAdmission(true);
  else if (mode == "battle-retry")
    BattleRetryRemoval();
  else if (mode == "battle-journal")
    BattleResponseRemoval(false);
  else if (mode == "battle-profile")
    BattleResponseRemoval(true);
  else {
    Check(mode == "battle-native", "unknown local battle fixture case");
    BattleRequestAdmission(false);
    BattleRequestAdmission(true);
    BattleRetryRemoval();
    BattleResponseRemoval(false);
    BattleResponseRemoval(true);
  }
  sync_fixture::session_observer  = {};
  sync_fixture::settings_observer = {};
  std::cout << "Production battle requests/retry/target removal/name suppression passed: " << mode << '\n';
}
} // namespace
