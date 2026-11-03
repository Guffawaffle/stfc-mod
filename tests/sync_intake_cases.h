#pragma once

// Actual native intake, admission and owned queue. No installer, worker or HTTP body runs.
#include "../src/mod/sync/sync.cc"

namespace
{
auto sync_intake_settings = std::make_shared<Config::Sync>();
void SyncIntakeProcess(const data_sync::Input&) {}
} // namespace
std::shared_ptr<const Config::Sync> Config::Sync::Get()
{ return sync_intake_settings; }
namespace data_sync
{
Processor ProcessorFor(EntityGroup::Kind kind)
{
  if (kind == EntityGroup::Kind::Resources)
    return SyncIntakeProcess;
  if (kind == EntityGroup::Kind::AllianceProfiles)
    return CacheAllianceNames;
  if (kind == EntityGroup::Kind::UserProfiles)
    return CachePlayerNames;
  return nullptr;
}
void ProcessRealtimeSlot(const Input&) {}
void ProcessStoreOffers(const Input&) {}
void ProcessOwnProfile(const Input&) {}
void CachePlayerNames(const Input&) {}
void CacheAllianceNames(const Input&) {}
void RunBattleLogs() noexcept {}
void UploadTarget(std::weak_ptr<Destination>, std::string) {}
void ForgetOldBattleRequests(uint64_t) {}
void RecordStore::Reset()
{ ++generation_; }
bool RecordStore::Apply(Stream, RecordSet, Update)
{ return true; }
bool RecordStore::Contains(Stream, const json&) const
{ return false; }
void Delivery::Configure(const SyncTargetConfig&) {}
void Delivery::Retire() {}
} // namespace data_sync

namespace
{
struct SyncIntakeBytes {
  Il2CppObject        header{};
  Il2CppArrayBounds*  bounds = nullptr;
  il2cpp_array_size_t length = 3;
  uint8_t             data[8]{1, 2, 3};
};
struct SyncIntakeByteString {
  Il2CppObject     header{};
  SyncIntakeBytes* bytes = nullptr;
};
struct SyncIntakeGroup {
  Il2CppObject          header{};
  int32_t               kind  = static_cast<int32_t>(EntityGroup::Kind::Resources);
  SyncIntakeByteString* bytes = nullptr;
};
struct SyncIntakeList {
  Il2CppObject               header{};
  std::vector<Il2CppObject*> groups;
};
struct SyncIntakeResponse {
  Il2CppObject    header{};
  SyncIntakeList* groups = nullptr;
};
struct SyncIntakeString {
  Il2CppObject header{};
  int32_t      length = 0;
  char16_t     chars[32]{};
};
int              sync_intake_fault = 0, sync_intake_group_reads = 0, sync_intake_collection_reads = 0;
SyncIntakeString sync_intake_type{}, sync_intake_target{}, sync_intake_data{};
SyncIntakeString sync_intake_user{};
Il2CppObject     sync_intake_manager{}, sync_intake_profile{};
bool             sync_intake_missing_profile = false;
void             SyncIntakeFault(int fault)
{
  if (sync_intake_fault == fault)
    throw Il2CppExceptionWrapper(&thrown);
}
SyncIntakeList* SyncIntakeGroups(SyncIntakeResponse* response, const MethodInfo*)
{
  ++sync_intake_collection_reads;
  SyncIntakeFault(1);
  return response->groups;
}
int32_t SyncIntakeCount(SyncIntakeList* list, const MethodInfo*)
{
  SyncIntakeFault(2);
  return static_cast<int32_t>(list->groups.size());
}
Il2CppObject* SyncIntakeItem(SyncIntakeList* list, int32_t index, const MethodInfo*)
{
  SyncIntakeFault(3);
  return list->groups.at(index);
}
int32_t SyncIntakeKind(SyncIntakeGroup* group, const MethodInfo*)
{
  SyncIntakeFault(4);
  return group->kind;
}
SyncIntakeByteString* SyncIntakePayload(SyncIntakeGroup* group, const MethodInfo*)
{
  ++sync_intake_group_reads;
  SyncIntakeFault(5);
  return group->bytes;
}
Il2CppString* SyncIntakeRtcType(RealtimeDataPayload*, const MethodInfo*)
{
  SyncIntakeFault(6);
  return reinterpret_cast<Il2CppString*>(&sync_intake_type);
}
Il2CppString* SyncIntakeRtcTarget(RealtimeDataPayload*, const MethodInfo*)
{
  SyncIntakeFault(7);
  return reinterpret_cast<Il2CppString*>(&sync_intake_target);
}
Il2CppString* SyncIntakeRtcData(RealtimeDataPayload*, const MethodInfo*)
{
  SyncIntakeFault(8);
  return reinterpret_cast<Il2CppString*>(&sync_intake_data);
}
Il2CppObject* SyncIntakeManager(const MethodInfo*)
{
  SyncIntakeFault(9);
  return &sync_intake_manager;
}
Il2CppObject* SyncIntakeProfile(Il2CppObject*, const MethodInfo*)
{
  SyncIntakeFault(10);
  return sync_intake_missing_profile ? nullptr : &sync_intake_profile;
}
Il2CppString* SyncIntakeUser(Il2CppObject*, const MethodInfo*)
{
  SyncIntakeFault(11);
  return reinterpret_cast<Il2CppString*>(&sync_intake_user);
}
int32_t SyncIntakeLevel(Il2CppObject*, const MethodInfo*)
{
  SyncIntakeFault(12);
  return 55;
}

void SyncIntakeCases(std::string_view mode)
{
  using namespace data_sync;
  auto& byte                       = Define("System", "Byte", IL2CPP_TYPE_U1, nullptr, 1);
  auto& vector                     = Define("System", "Byte[]", IL2CPP_TYPE_SZARRAY, &array_class);
  vector.klass.byval_arg.data.type = T(byte);
  auto& bytes_type                 = Define("Google.Protobuf", "ByteString", IL2CPP_TYPE_CLASS, &object_class);
  bytes_type.klass.instance_size   = sizeof(SyncIntakeByteString);
  AddField(bytes_type, "bytes", mode == "sync-intake-byte-pointer" ? intptr_class : vector,
           offsetof(SyncIntakeByteString, bytes));
  auto& group_type  = Define("Digit.PrimeServer.Models", "EntityGroup", IL2CPP_TYPE_CLASS, &object_class);
  auto& kind_type   = Define("Digit.PrimeServer.Models", "EntityGroupType", IL2CPP_TYPE_VALUETYPE, nullptr, 4);
  kind_type.is_enum = true;
  const auto& kind  = AddMethod(group_type, "get_Type", reinterpret_cast<void*>(&SyncIntakeKind), T(kind_type), {});
  const auto& payload =
      AddMethod(group_type, "get_Group", reinterpret_cast<void*>(&SyncIntakePayload), T(bytes_type), {});
  AddProperty(group_type, "Type", &kind);
  AddProperty(group_type, "Group", &payload);
  auto& raw_pointer_type = Define("Fixture", "SyncNativePointer", IL2CPP_TYPE_PTR);
  auto& list_type        = Define("Fixture", "SyncGroups", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(list_type, "get_Count", reinterpret_cast<void*>(&SyncIntakeCount), T(int_class), {});
  AddMethod(list_type, "get_Item", reinterpret_cast<void*>(&SyncIntakeItem), T(group_type), {T(int_class)});
  auto&       response_type = Define("Digit.PrimeServer.Models", "ServiceResponse", IL2CPP_TYPE_CLASS, &object_class);
  const auto& groups        = AddMethod(response_type, "get_EntityGroups", reinterpret_cast<void*>(&SyncIntakeGroups),
                                        T(mode == "sync-intake-list-pointer"     ? intptr_class
                                          : mode == "sync-intake-list-unmanaged" ? raw_pointer_type
                                                                                 : list_type),
                                        {});
  AddProperty(response_type, "EntityGroups", &groups);
  auto&       platform_group_type = Define("Digit.Platform.Models", "EntityGroup", IL2CPP_TYPE_CLASS, &object_class);
  const auto& platform_kind =
      AddMethod(platform_group_type, "get_Type", reinterpret_cast<void*>(&SyncIntakeKind), T(kind_type), {});
  const auto& platform_payload =
      AddMethod(platform_group_type, "get_Group", reinterpret_cast<void*>(&SyncIntakePayload), T(bytes_type), {});
  AddProperty(platform_group_type, "Type", &platform_kind);
  AddProperty(platform_group_type, "Group", &platform_payload);
  auto& platform_list_type = Define("Fixture", "PlatformSyncGroups", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(platform_list_type, "get_Count", reinterpret_cast<void*>(&SyncIntakeCount), T(int_class), {});
  AddMethod(platform_list_type, "get_Item", reinterpret_cast<void*>(&SyncIntakeItem), T(platform_group_type),
            {T(int_class)});
  auto& platform_response_type = Define("Digit.Platform.Models", "ServiceResponse", IL2CPP_TYPE_CLASS, &object_class);
  const auto& platform_groups =
      AddMethod(platform_response_type, "get_EntityGroups", reinterpret_cast<void*>(&SyncIntakeGroups),
                T(mode == "sync-intake-platform-unmanaged" ? raw_pointer_type : platform_list_type), {});
  AddProperty(platform_response_type, "EntityGroups", &platform_groups);
  auto&       profile_type = Define("Digit.PrimeServer.Models", "UserProfile", IL2CPP_TYPE_CLASS, &object_class);
  const auto& user =
      AddMethod(profile_type, "get_UserId", reinterpret_cast<void*>(&SyncIntakeUser), T(string_class), {});
  const auto& level = AddMethod(profile_type, "get_Level", reinterpret_cast<void*>(&SyncIntakeLevel), T(int_class), {});
  AddProperty(profile_type, "UserId", &user);
  AddProperty(profile_type, "Level", &level);
  auto& manager_type   = Define("Digit.Prime.PlayerProfile", "UserProfileManager", IL2CPP_TYPE_CLASS, &object_class);
  const auto& instance = AddMethod(manager_type, "get_Instance", reinterpret_cast<void*>(&SyncIntakeManager),
                                   T(manager_type), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(manager_type, "Instance", &instance);
  AddMethod(manager_type, "GetLocalUserProfile", reinterpret_cast<void*>(&SyncIntakeProfile), T(profile_type), {});
  sync_intake_manager.klass = &manager_type.klass;
  sync_intake_profile.klass = &profile_type.klass;
  auto& rtc_type            = Define("Digit.Networking.RTC", "RealtimeDataPayload", IL2CPP_TYPE_CLASS, &object_class);
  for (auto entry : {std::pair{"DataType", reinterpret_cast<void*>(&SyncIntakeRtcType)},
                     std::pair{"Target", reinterpret_cast<void*>(&SyncIntakeRtcTarget)},
                     std::pair{"Data", reinterpret_cast<void*>(&SyncIntakeRtcData)}}) {
    auto name = std::string("get_") + entry.first;
    // Metadata names must outlive all cached lookups in this controlled process.
    auto*       stable = new std::string(name);
    const auto& getter = AddMethod(rtc_type, stable->c_str(), entry.second, T(string_class), {});
    AddProperty(rtc_type, entry.first, &getter);
  }
  for (auto* value : {&sync_intake_type, &sync_intake_target, &sync_intake_data, &sync_intake_user})
    value->header.klass = &string_class.klass;
  sync_intake_type.length     = 1;
  sync_intake_type.chars[0]   = u'0';
  sync_intake_target.length   = 1;
  sync_intake_target.chars[0] = u't';
  sync_intake_data.length     = 2;
  sync_intake_data.chars[0]   = u'{';
  sync_intake_data.chars[1]   = u'}';
  sync_intake_user.length     = 5;
  std::copy_n(u"owner", 5, sync_intake_user.chars);
  SyncIntakeBytes      bytes{{&vector.klass}};
  SyncIntakeByteString payload_value{{&bytes_type.klass}, &bytes};
  SyncIntakeGroup      group{{&group_type.klass}, static_cast<int32_t>(EntityGroup::Kind::Resources), &payload_value};
  SyncIntakeList       list{{&list_type.klass}, {&group.header}};
  SyncIntakeResponse   response{{&response_type.klass}, &list};
  RealtimeDataPayload  rtc;
  rtc.klass  = &rtc_type.klass;
  auto fresh = [&] {
    while (auto job = pending_jobs.TryPop()) {}
    data_sync::State().generation.fetch_add(1);
    sync_intake_fault = 0;
  };
  auto registry = [&](bool mutate = false) {
    int originals = 0;
    GameServerModelRegistry_ProcessResultInternal_Hook(
        [&](auto*, auto*, auto*, auto*) {
          ++originals;
          if (mutate)
            bytes.data[0] = 9;
        },
        nullptr, nullptr, reinterpret_cast<ServiceResponse*>(&response), nullptr);
    Require(originals == 1, "native registry original is delivered exactly once");
  };
  auto failed = [&](int fault) {
    fresh();
    sync_intake_fault = fault;
    registry();
    Require(!OrdinarySessionOpen(SessionGeneration()) && !pending_jobs.TryPop(),
            "unavailable required native intake seals without synthesizing empty input");
  };
  if (mode == "sync-intake-count") {
    failed(2);
    return;
  }
  if (mode == "sync-intake-group") {
    failed(5);
    return;
  }
  if (mode == "sync-intake-type") {
    failed(4);
    return;
  }
  if (mode == "sync-intake-list-unmanaged" || mode == "sync-intake-platform-unmanaged") {
    const bool platform = mode == "sync-intake-platform-unmanaged";
    if (platform) {
      sync_intake_settings->options.store_offers = true;
      group.header.klass                         = &platform_group_type.klass;
      group.kind                                 = static_cast<int32_t>(PlatformEntityGroup::Kind::StoreOffers);
      list.header.klass                          = &platform_list_type.klass;
      response.header.klass                      = &platform_response_type.klass;
    }
    fresh();
    sync_intake_collection_reads = 0;
    registry();
    Require(!OrdinarySessionOpen(SessionGeneration()) && !pending_jobs.TryPop() && sync_intake_collection_reads == 0,
            "native-pointer collection metadata is refused before getter invocation under real Object assignability");
    const auto raw = platform
                         ? reinterpret_cast<PlatformServiceResponse*>(&response)->TryCall<"get_EntityGroups", void*>()
                         : reinterpret_cast<ServiceResponse*>(&response)->TryCall<"get_EntityGroups", void*>();
    Require(raw && raw.value == &list && sync_intake_collection_reads == 1,
            "intentional untyped-pointer bindings still accept the native-pointer signature");
    return;
  }
  if (mode == "sync-intake-list-pointer" || mode == "sync-intake-byte-pointer") {
    failed(0);
    return;
  }
  if (mode == "sync-intake-byte-shape") {
    bytes.header.klass = &other_class.klass;
    failed(0);
    return;
  }
  if (mode == "sync-intake-identity") {
    group.kind = static_cast<int32_t>(EntityGroup::Kind::UserProfiles);
    failed(12);
    return;
  }
  if (mode == "sync-intake-bytes") {
    fresh();
    payload_value.bytes = nullptr;
    registry();
    Require(!OrdinarySessionOpen(SessionGeneration()) && !pending_jobs.TryPop(),
            "null ByteString storage cannot become a valid observed empty protobuf");
    return;
  }
  if (mode == "sync-intake-rtc") {
    fresh();
    sync_intake_fault = 8;
    int originals     = 0;
    RtcParser_ParseFinalPayload_Hook(
        [&](auto*, auto, auto*, auto*) -> void* {
          ++originals;
          return &rtc;
        },
        nullptr, CentrifugoInfo{}, &rtc, nullptr);
    Require(originals == 1 && !OrdinarySessionOpen(SessionGeneration()) && !pending_jobs.TryPop(),
            "unavailable required realtime data seals while delivering original once");
    return;
  }
  for (int fault : {1, 2, 3, 4, 5})
    failed(fault);
  fresh();
  registry(true);
  {
    auto job = pending_jobs.TryPop();
    Require(job && job->value.input.bytes == std::string("\1\2\3", 3),
            "owned bytes are copied before the native original can mutate managed storage");
  }
  fresh();
  bytes.length = 0;
  registry();
  {
    auto job = pending_jobs.TryPop();
    Require(job && job->value.input.bytes.empty() && OrdinarySessionOpen(SessionGeneration()),
            "a real observed empty byte vector remains a valid empty protobuf");
  }
  fresh();
  list.groups.clear();
  registry();
  Require(OrdinarySessionOpen(SessionGeneration()) && !pending_jobs.TryPop(), "observed empty collection is healthy");
  list.groups = {&group.header};
  fresh();
  group.kind        = static_cast<int32_t>(EntityGroup::Kind::HullSpecs);
  sync_intake_fault = 5;
  const auto reads  = sync_intake_group_reads;
  registry();
  Require(OrdinarySessionOpen(SessionGeneration()) && !pending_jobs.TryPop() && sync_intake_group_reads == reads,
          "unsupported type is filtered before reading unavailable payload");
  fresh();
  group.kind        = static_cast<int32_t>(EntityGroup::Kind::AllianceProfiles);
  sync_intake_fault = 5;
  registry();
  Require(OrdinarySessionOpen(SessionGeneration()) && !pending_jobs.TryPop(),
          "optional enrichment failure remains best effort");
  group.kind = static_cast<int32_t>(EntityGroup::Kind::UserProfiles);
  for (int fault : {9, 10, 11, 12})
    failed(fault);
  fresh();
  sync_intake_missing_profile = true;
  registry();
  {
    auto job = pending_jobs.TryPop();
    Require(job && job->value.input.user_id.empty() && OrdinarySessionOpen(SessionGeneration()),
            "observed missing local profile preserves optional name enrichment");
  }
  sync_intake_missing_profile = false;
  fresh();
  registry();
  {
    auto job = pending_jobs.TryPop();
    Require(job && job->value.input.user_id == "owner" && job->value.input.level == 55,
            "qualified local identity and level are captured in owned input");
  }
  fresh();
  group.kind           = static_cast<int32_t>(EntityGroup::Kind::HullSpecs);
  int thrown_originals = 0;
  try {
    GameServerModelRegistry_ProcessResultInternal_Hook(
        [&](auto*, auto*, auto*, auto*) {
          ++thrown_originals;
          throw Il2CppExceptionWrapper(&thrown);
        },
        nullptr, nullptr, reinterpret_cast<ServiceResponse*>(&response), nullptr);
    Require(false, "native original exception must propagate");
  } catch (const Il2CppExceptionWrapper&) {
    Require(thrown_originals == 1 && OrdinarySessionOpen(SessionGeneration()),
            "native original throws once outside the mod loss catch");
  }
  sync_intake_settings->options.store_offers = true;
  group.header.klass                         = &platform_group_type.klass;
  group.kind                                 = static_cast<int32_t>(PlatformEntityGroup::Kind::StoreOffers);
  list.header.klass                          = &platform_list_type.klass;
  response.header.klass                      = &platform_response_type.klass;
  fresh();
  registry();
  {
    auto job = pending_jobs.TryPop();
    Require(job && job->value.process == ProcessStoreOffers && job->value.input.bytes.empty(),
            "actual platform registry branch admits observed empty offers payload");
  }
  for (int fault : {1, 2, 3, 4, 5})
    failed(fault);
  for (int fault : {6, 7, 8}) {
    fresh();
    sync_intake_fault = fault;
    int originals     = 0;
    RtcParser_ParseFinalPayload_Hook(
        [&](auto*, auto, auto*, auto*) -> void* {
          ++originals;
          return &rtc;
        },
        nullptr, CentrifugoInfo{}, &rtc, nullptr);
    Require(originals == 1 && !OrdinarySessionOpen(SessionGeneration()) && !pending_jobs.TryPop(),
            "each unavailable realtime observation seals and preserves native original");
  }
  fresh();
  int        rtc_originals = 0;
  const auto result        = RtcParser_ParseFinalPayload_Hook(
      [&](auto*, auto, auto*, auto*) -> void* {
        ++rtc_originals;
        sync_intake_data.chars[0] = u'x';
        return &rtc;
      },
      nullptr, CentrifugoInfo{}, &rtc, nullptr);
  {
    auto job = pending_jobs.TryPop();
    Require(result == &rtc && rtc_originals == 1 && job && job->value.input.bytes == "{}",
            "realtime owns its copy before native mutation and preserves original return");
  }
  Require(warp_roots.empty(), "native intake releases every temporary managed root");
  std::cout << "Sync native intake observations, custody, copy timing and filters passed\n";
}
} // namespace
