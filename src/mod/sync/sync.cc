// [sync]: watches the game's server responses and realtime updates, turns them into records on one processing
// thread (records.cc), and keeps the shared state: records, the game session and the targets. Each target uploads
// on its own thread (upload.cc); battle journals are fetched on another (battlelogs.cc).
//
// TODO: sync has no shutdown. Its threads are detached and run until the process exits, which can interrupt an
// HTTP request mid-flight. The shared state is leaked so they never see it destroyed.

#include "sync/sync.h"
#include "sync/input_queue.h"

#include "features/features.h"

#include "game/app.h"
#include "game/server.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>

#include <thread>
#include <utility>

namespace data_sync
{
namespace
{
  // --- Processing thread ---

  struct Job {
    Processor process;
    bool      own_profile;
    Input     input;
  };

  // Budget includes the handler's in-flight lease. Rejection never copies the
  // managed payload, and an earlier slow copy retains its FIFO position.
  auto& pending_jobs = *new sync_queue::BoundedQueue<Job>{{kInputQueueCount, kInputQueueBytes, kInputItemBytes}};

  template <typename Factory>
  void Submit(uint64_t generation, size_t bytes, size_t user_units, bool required, Factory&& factory) noexcept
  {
    if (!OrdinarySessionOpen(generation))
      return;
    const auto charge = InputCharge(bytes, user_units, sizeof(Job) + 2);
    if (!charge
        || pending_jobs.TryEmplace(*charge, std::forward<Factory>(factory)) != sync_queue::Admission::accepted) {
      if (required)
        SealOrdinarySession(generation);
    }
  }

  void RunJobs() noexcept
  {
    auto& state = State();
    ContainWorkerExit(state.health, [&] {
      while (auto lease = pending_jobs.WaitPop()) {
        const auto& job = lease->value;
        ProcessOwnedInput(job.process, job.own_profile, job.input);
      }
    });
    state.ready.notify_all();
  }

  void HandleEntityGroup(EntityGroup* entity_group) noexcept
  {
    const auto generation = SessionGeneration();
    bool       required   = true;
    if (!OrdinarySessionOpen(generation))
      return;
    try {
      if (!entity_group)
        return;
      const auto observed_type = entity_group->TryType();
      if (!observed_type) {
        SealOrdinarySession(generation);
        return;
      }
      const auto type        = observed_type.value;
      const auto process     = ProcessorFor(type);
      const bool own_profile = type == EntityGroup::Kind::UserProfiles;
      if (!process && !own_profile)
        return;
      required              = (process != CachePlayerNames && process != CacheAllianceNames) || own_profile;
      Il2CppString* user_id = nullptr;
      int32_t       level   = 0;
      if (type == EntityGroup::Kind::PlanetaryMapResponse || own_profile) {
        const auto manager = UserProfileManager::TryInstance();
        if (!manager) {
          SealOrdinarySession(generation);
          return;
        }
        const auto profile =
            manager.value ? manager.value->TryLocalUserProfile() : il2cpp::CallResult<UserProfile*>{true, nullptr};
        if (!profile) {
          SealOrdinarySession(generation);
          return;
        }
        if (profile.value) {
          const auto id = profile.value->TryUserId();
          if (!id) {
            SealOrdinarySession(generation);
            return;
          }
          user_id = id.value;
          if (own_profile && user_id && user_id->length > 0) {
            const auto observed_level = profile.value->TryLevel();
            if (!observed_level) {
              SealOrdinarySession(generation);
              return;
            }
            level = observed_level.value;
          }
        }
      }
      if (user_id && user_id->length < 0) {
        SealOrdinarySession(generation);
        return;
      }
      const auto user_units = user_id ? static_cast<size_t>(user_id->length) : 0;
      required = (process != CachePlayerNames && process != CacheAllianceNames) || (own_profile && user_units);
      // A missing local identity deliberately supplies no owner-qualified map.
      if (type == EntityGroup::Kind::PlanetaryMapResponse && !user_units)
        return;
      if (!process && !(own_profile && user_units))
        return;
      const auto group = entity_group->TryGroup();
      if (!group) {
        if (required)
          SealOrdinarySession(generation);
        return;
      }
      if (!group.value)
        return;
      auto payload = group.value->TryPayload();
      if (!payload) {
        if (required)
          SealOrdinarySession(generation);
        return;
      }
      const auto bytes = payload.value.bytes;
      Submit(generation, bytes.size(), user_units, required, [&] {
        return Job{process, own_profile, {generation, {bytes.begin(), bytes.end()}, il2cpp::ToUtf8(user_id), level}};
      });
    } catch (...) {
      if (required)
        SealOrdinarySession(generation);
    }
  }

  // Count and each item are observations, not defaults. Successful null/empty collections retain no-content policy.
  template <typename Group, typename Response, typename Visit> bool VisitEntityGroups(Response* response, Visit&& visit)
  {
    const auto observed = response->TryEntityGroups();
    if (!observed)
      return false;
    if (!observed.value)
      return true;
    auto owner = il2cpp::GcHandle::Strong(observed.value);
    if (!owner)
      return false;
    auto*      list  = owner.template Get<il2cpp::List<Group*>>();
    const auto count = list->template TryCall<"get_Count", int32_t>();
    if (!count || count.value < 0)
      return false;
    for (int32_t index = 0; index < count.value; ++index) {
      const auto item = list->template TryCall<"get_Item", Group*(int32_t)>(index);
      if (!item || (item.value && !item.value->template Is<Group>()))
        return false;
      if (item.value) {
        auto group = il2cpp::GcHandle::Strong(item.value);
        if (!group)
          return false;
        visit(group.template Get<Group>());
      }
    }
    return true;
  }

  void HandlePlatformResponse(PlatformServiceResponse* response) noexcept
  {
    const auto generation = SessionGeneration();
    if (!OrdinarySessionOpen(generation))
      return;
    try {
      if (!VisitEntityGroups<PlatformEntityGroup>(response, [&](PlatformEntityGroup* entity_group) {
            const auto type = entity_group->TryType();
            if (!type) {
              SealOrdinarySession(generation);
              return;
            }
            if (type.value != PlatformEntityGroup::Kind::StoreOffers)
              return;
            const auto group = entity_group->TryGroup();
            if (!group) {
              SealOrdinarySession(generation);
              return;
            }
            if (!group.value)
              return;
            auto payload = group.value->TryPayload();
            if (!payload) {
              SealOrdinarySession(generation);
              return;
            }
            const auto bytes = payload.value.bytes;
            Submit(generation, bytes.size(), 0, true,
                   [&] { return Job{ProcessStoreOffers, false, {generation, {bytes.begin(), bytes.end()}}}; });
          }))
        SealOrdinarySession(generation);
    } catch (...) {
      SealOrdinarySession(generation);
    }
  }

  // --- Hooks ---

  // The parser and game server classes below are only bound as hook targets, so their receivers stay untyped.

  void* RtcParser_ParseFinalPayload_Hook(auto original, void* _this, CentrifugoInfo centrifugo_info,
                                         RealtimeDataPayload* payload, void* final_payload)
  {
    // DataType arrives as a number in a string; JSON is "0". This runs inside the game's parser, so it only copies
    // the payload out.
    const auto generation = SessionGeneration();
    try {
      if (OrdinarySessionOpen(generation) && payload) {
        const auto type = payload->TryDataType();
        if (!type)
          SealOrdinarySession(generation);
        else if (il2cpp::Equals(type.value, "0")) {
          const auto target = payload->TryTarget();
          if (!target)
            SealOrdinarySession(generation);
          else if (target.value) {
            const auto data = payload->TryData();
            if (!data || (data.value && data.value->length < 0))
              SealOrdinarySession(generation);
            else if (data.value)
              Submit(generation, 0, static_cast<size_t>(data.value->length), true,
                     [&] { return Job{ProcessRealtimeSlot, false, {generation, il2cpp::ToUtf8(data.value)}}; });
          }
        }
      }
    } catch (...) {
      SealOrdinarySession(generation);
    }
    return original(_this, centrifugo_info, payload, final_payload);
  }

  void GameServerModelRegistry_ProcessResultInternal_Hook(auto original, void* _this, void* parsing_context,
                                                          ServiceResponse* service_response, MethodInfo* method)
  {
    // The hooked method is shared generic code; PlatformModelRegistry passes its own response type.
    const auto generation = SessionGeneration();
    try {
      if (OrdinarySessionOpen(generation) && service_response && service_response->Is<ServiceResponse>()) {
        if (!VisitEntityGroups<EntityGroup>(service_response, HandleEntityGroup))
          SealOrdinarySession(generation);
      } else if (service_response && Config::Sync::Get()->options.store_offers) {
        if (auto* platform = service_response->As<PlatformServiceResponse>())
          HandlePlatformResponse(platform);
      }
    } catch (...) {
      SealOrdinarySession(generation);
    }
    return original(_this, parsing_context, service_response, method);
  }

  void PrimeApp_InitPrimeServer_Hook(auto original, PrimeApp* _this, Il2CppString* game_server_url,
                                     Il2CppString* gateway_server_url, Il2CppString* session_id,
                                     Il2CppString* server_region)
  {
    // A new session drops every record; targets start over from the next responses. The generation changes
    // before original can deliver responses from the new session.
    auto& state = State();
    if (!TrySessionUpdate(state.health, [&] {
          auto url = il2cpp::ToUtf8(game_server_url);
          auto id  = il2cpp::ToUtf8(session_id);
          {
            std::scoped_lock lock(state.mutex);
            state.records.Reset();
            state.session.generation = state.records.Generation();
            state.session.server_url = std::move(url);
            state.session.session_id = std::move(id);
            state.generation         = state.records.Generation();
          }
          ForgetOldBattleRequests(state.generation.load());
        }))
      state.diagnostics_ready.store(false, std::memory_order_release);
    state.ready.notify_all();
    original(_this, game_server_url, gateway_server_url, session_id, server_region);
  }

  void GameServer_Initialise_Hook(auto original, void* _this, Il2CppString* session_id, Il2CppString* game_version,
                                  bool encrypt_requests, Il2CppString* server_region)
  {
    original(_this, session_id, game_version, encrypt_requests, server_region);
    auto& state = State();
    if (!TrySessionUpdate(state.health, [&] {
          auto             version = il2cpp::ToUtf8(game_version);
          std::scoped_lock lock(state.mutex);
          state.session.version = std::move(version);
        })) {
      state.diagnostics_ready.store(false, std::memory_order_release);
      state.ready.notify_all();
    }
  }

  void GameServer_SetInstanceIdHeader_Hook(auto original, void* _this, int32_t instance_id)
  {
    original(_this, instance_id);
    auto& state = State();
    if (!TrySessionUpdate(state.health, [&] {
          std::scoped_lock lock(state.mutex);
          state.session.instance_id = instance_id;
        })) {
      state.diagnostics_ready.store(false, std::memory_order_release);
      state.ready.notify_all();
    }
  }

  bool InstallSync()
  {
    // Complete lazy shared-state allocation before callbacks can call noexcept gates.
    (void)State();
    il2cpp::HookSet hooks("Sync");
    IL2CPP_STAGE(hooks, GameServerModelRegistry::Method("ProcessResultInternal"),
                 GameServerModelRegistry_ProcessResultInternal_Hook);
    IL2CPP_STAGE(hooks, PrimeApp::Class::Method("InitPrimeServer"), PrimeApp_InitPrimeServer_Hook);

    // Only captures the game version for the X-PRIME-VERSION header, which has a fallback (GameSession). Users
    // whose other mods detour the same method turn it off: a native and a managed detour on one prologue corrupt
    // each other and crash at login. Unlike every other [patches] key, game_version is a switch inside this
    // feature's installer, so it means nothing without syncpatches.
    if (Config::Get().patches.game_version)
      IL2CPP_STAGE(hooks, GameServer::Method("Initialise"), GameServer_Initialise_Hook);
    else
      spdlog::debug("[Sync] not capturing the game version ([patches] game_version = false)");

    IL2CPP_STAGE(hooks, GameServer::Method("SetInstanceIdHeader"), GameServer_SetInstanceIdHeader_Hook);

    // Each parser overrides ParseFinalPayload, so these are two distinct native methods.
    IL2CPP_STAGE(hooks, SlotAssignRtcParser::Method("ParseFinalPayload"), RtcParser_ParseFinalPayload_Hook);
    IL2CPP_STAGE(hooks, SlotClearRtcParser::Method("ParseFinalPayload"), RtcParser_ParseFinalPayload_Hook);
    // Without its hooks sync receives nothing, so its workers would only idle.
    if (!hooks.Commit())
      return false;

    try {
      std::thread(RunJobs).detach();
      std::thread(RunBattleLogs).detach();
      FollowSettings();
      State().diagnostics_ready.store(true, std::memory_order_release);
      return true;
    } catch (...) {
      // Installed hooks cannot imply that an unavailable consumer is ready.
      State().health.Stop();
      State().diagnostics_ready.store(false, std::memory_order_release);
      State().ready.notify_all();
      return false;
    }
  }
} // namespace

// --- Shared state ---

SharedState& State()
{
  static auto& state = *new SharedState;
  return state;
}

bool OrdinarySessionOpen(uint64_t generation) noexcept
{
  const auto& state = State();
  return generation == state.generation.load() && state.health.Allows(generation);
}

void SealOrdinarySession(uint64_t generation) noexcept
{
  auto& state = State();
  if (state.health.Seal(generation))
    state.ready.notify_all();
}

void Publish(Stream stream, RecordSet records, uint64_t generation, Update mode)
{
  auto& state = State();
  {
    std::scoped_lock lock(state.mutex);
    if (!OrdinarySessionOpen(generation) || !state.records.Apply(stream, std::move(records), mode))
      return;
  }
  // Wakes the upload threads only when a stream actually changed.
  state.ready.notify_all();
}

bool HasBattle(uint64_t id, uint64_t generation)
{
  auto&            state = State();
  std::scoped_lock lock(state.mutex);
  return OrdinarySessionOpen(generation) && state.records.Contains(Stream::kBattles, id);
}

// Starts, reconfigures and retires target threads to match [sync] after a settings reload. Runs every frame, so
// it returns at once while the settings object is unchanged. Game thread only.
void FollowSettings()
{
  // Once-only reporting here keeps logging/allocation off native response callbacks.
  static uint64_t reported      = 0;
  static bool     reported_stop = false;
  auto&           health        = State().health;
  const auto      sealed        = health.SealedThrough();
  try {
    if (sealed > reported) {
      reported = sealed;
      spdlog::error("[Sync] ordinary input was lost in generation {}; publication, uploads and acknowledgements "
                    "for that generation are sealed. A new native login must establish a fresh baseline.",
                    sealed);
    }
    if (health.Stopped() && !reported_stop) {
      reported_stop = true;
      spdlog::error("[Sync] ordinary processing cannot safely continue; restart the game to establish a fresh sync.");
    }
  } catch (...) {
    // Reporting must not escape the feature's frame callback.
  }
  static std::shared_ptr<const Config::Sync> applied;
  const auto                                 config = Config::Sync::Get();
  if (applied == config)
    return;
  auto& state = State();
  {
    std::scoped_lock lock(state.mutex);
    // Dropping the registry's reference ends the target's thread after its current request (see UploadTarget).
    std::erase_if(state.targets, [&](const auto& entry) {
      if (config->targets.contains(entry.first))
        return false;
      entry.second->delivery.Retire();
      return true;
    });
    for (const auto& [name, options] : config->targets) {
      if (const auto found = state.targets.find(name); found != state.targets.end()) {
        if (!found->second->unavailable)
          found->second->delivery.Configure(options);
        continue;
      }
      auto target = std::make_shared<Destination>();
      target->delivery.Configure(options);
      std::thread(UploadTarget, std::weak_ptr(target), name).detach();
      state.targets.emplace(name, std::move(target));
    }
  }
  applied = config;
  state.ready.notify_all();
}

uint64_t SessionGeneration()
{ return State().generation.load(); }

GameSession Session()
{
  auto&            state = State();
  std::scoped_lock lock(state.mutex);
  return state.session;
}
} // namespace data_sync

const Feature kSync{"Sync", &Config::Patches::syncpatches, data_sync::InstallSync, data_sync::FollowSettings};
