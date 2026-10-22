#include "discovery_tow.h"

#include "config.h"
#include "il2cpp/method_contract.h"
#include "il2cpp/runtime.h"
#include "prime/DeploymentManager.h"
#include "prime/FleetsManager.h"
#include "prime/Hub.h"
#include "prime/ScreenManager.h"

#include <array>
#include <chrono>
#include <cstring>
#include <spdlog/spdlog.h>
#include <string_view>
#include <utility>

namespace discovery_tow
{
namespace
{
  using Clock = std::chrono::steady_clock;
  struct Methods {
    const MethodInfo *start{}, *stop{}, *get_course{}, *fleet_id{}, *target{}, *address{}, *same_address{}, *path{},
        *valid{}, *in_use{};
    Il2CppClass* behaviour{};
    Methods()
    {
      auto mono       = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "MonoBehaviour");
      auto deployment = il2cpp_get_class_helper("Assembly-CSharp", "", "DeploymentManager");
      auto course = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CourseData");
      auto node   = il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "CourseNode");
      auto node_address =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "NodeAddress");
      auto result =
          il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "PathFinderResultData");
      behaviour = mono.get_cls();
      start     = method_contract::Resolve(behaviour, "StartCoroutine", false, "UnityEngine.Coroutine",
                                           {"System.Collections.IEnumerator"});
      stop      = method_contract::Resolve(behaviour, "StopCoroutine", false, "System.Void", {"UnityEngine.Coroutine"});
      get_course = method_contract::Resolve(deployment.get_cls(), "GetPlannedCourse", false,
                                            "Digit.PrimeServer.Models.CourseData", {"System.Int64"});
      fleet_id   = method_contract::Resolve(course.get_cls(), "get_FleetID", false, "System.Int64", {});
      target     = method_contract::Resolve(course.get_cls(), "get_TargetNode", false,
                                            "Digit.PrimeServer.Models.CourseNode", {});
      address =
          method_contract::Resolve(node.get_cls(), "get_Address", false, "Digit.PrimeServer.Models.NodeAddress", {});
      same_address = method_contract::Resolve(node_address.get_cls(), "Equals", false, "System.Boolean",
                                              {"Digit.PrimeServer.Models.NodeAddress"});
      path         = method_contract::Resolve(course.get_cls(), "get_PathFinderResult", false,
                                              "Digit.PrimeServer.Models.PathFinderResultData", {});
      valid        = method_contract::Resolve(result.get_cls(), "get_Valid", false, "System.Boolean", {});
      in_use       = method_contract::Resolve(course.get_cls(), "get_IsInUse", false, "System.Boolean", {});
    }
    bool Ready() const
    { return start && stop && get_course && fleet_id && target && address && same_address && path && valid && in_use; }
  };
  Methods& Native()
  {
    static Methods methods;
    return methods;
  }

  // Raw handles have no static destructor: cleanup occurs only on the game's
  // update/cancel path, never through Unity calls during DLL teardown.
  enum Root { Owner, Iterator, Coroutine, Destination, Baseline, RootCount };
  struct Pending {
    std::array<Il2CppGCHandle, RootCount> roots{};
    ptrdiff_t                             state_offset{};
    uint64_t                              selected{}, discovery{};
    int                                   selected_index{}, discovery_index{};
    SectionID                             section{};
    Clock::time_point                     deadline{};
    Clock::time_point                     next_poll{};
    uint64_t                              generation{};
  };
  Pending  pending;
  uint64_t next_generation = 0;

  struct LocalRoot {
    Il2CppGCHandle handle{};
    explicit LocalRoot(void* value)
        : handle(value ? il2cpp_gchandle_new(static_cast<Il2CppObject*>(value), false) : nullptr)
    {
    }
    LocalRoot(const LocalRoot&)            = delete;
    LocalRoot& operator=(const LocalRoot&) = delete;
    ~LocalRoot()
    {
      if (handle)
        il2cpp_gchandle_free(handle);
    }
  };

  Il2CppObject* Target(Root root)
  { return pending.roots[root] ? il2cpp_gchandle_get_target(pending.roots[root]) : nullptr; }

  bool Keep(Root root, void* value)
  {
    if (!value)
      return root == Baseline;
    pending.roots[root] = il2cpp_gchandle_new(static_cast<Il2CppObject*>(value), false);
    return pending.roots[root] != nullptr;
  }

  bool OwnerAlive(Il2CppObject* owner)
  {
    return owner && owner->klass && !(reinterpret_cast<uintptr_t>(owner->klass) & 1)
           && TrackedObjectSnapshot<DeploymentManger>::Alive(
               owner, TrackedObjectSnapshot<DeploymentManger>::LayoutOffset(owner->klass));
  }

  Il2CppObject* Invoke(const MethodInfo* method, void* owner)
  {
    Il2CppObject* result = nullptr;
    return owner && Il2CppRuntime::TryInvoke(method, owner, nullptr, &result) ? result : nullptr;
  }

  bool Boolean(const MethodInfo* method, void* owner, bool expected)
  {
    bool value = false;
    return Il2CppRuntime::TryBoolean(Invoke(method, owner), value) && value == expected;
  }

  bool Course(void* owner, Il2CppObject*& result)
  {
    auto  id = static_cast<int64_t>(pending.selected);
    void* args[]{&id};
    return Il2CppRuntime::TryInvoke(Native().get_course, owner, args, &result);
  }

  bool SameAddress(Il2CppObject* first, void* second)
  {
    if (!first || !second)
      return false;
    void*         args[]{second};
    Il2CppObject* result = nullptr;
    bool          same   = false;
    return Il2CppRuntime::TryInvoke(Native().same_address, first, args, &result)
           && Il2CppRuntime::TryBoolean(result, same) && same;
  }

  bool IteratorShape(Il2CppObject* iterator)
  {
    auto* klass     = iterator ? iterator->klass : nullptr;
    auto* declaring = klass ? il2cpp_class_get_declaring_type(klass) : nullptr;
    if (!klass || !declaring || std::strcmp(declaring->name, "DeploymentService") != 0
        || std::strcmp(declaring->namespaze, "Digit.PrimeServer.Services") != 0
        || !std::string_view(klass->name).starts_with("<PlanCourse>d__"))
      return false;
    auto* state = il2cpp_class_get_field_from_name(klass, "<>1__state");
    auto* fleet = il2cpp_class_get_field_from_name(klass, "fleet");
    if (!state || !state->type || state->type->byref || state->type->type != IL2CPP_TYPE_I4 || !fleet
        || !method_contract::Type(fleet->type, "Digit.PrimeServer.Models.FleetPlayerData"))
      return false;
    const auto offset = il2cpp_field_get_offset(state);
    if (offset < sizeof(Il2CppObject) || offset + sizeof(int32_t) > klass->instance_size
        || !method_contract::Resolve(klass, "MoveNext", false, "System.Boolean", {}))
      return false;
    pending.state_offset = offset;
    return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(iterator) + offset) == 0;
  }

  bool FreshCourse(Il2CppObject* owner)
  {
    Il2CppObject* course = nullptr;
    if (!Course(owner, course) || !course || course == Target(Baseline))
      return false;
    // Root the result through subsequent native property calls. Baseline's root
    // also prevents address reuse from making an old object appear fresh.
    auto root = il2cpp_gchandle_new(course, false);
    if (!root)
      return false;
    struct Release {
      Il2CppGCHandle handle;
      ~Release()
      { il2cpp_gchandle_free(handle); }
    } release{root};
    auto*      fleet        = Invoke(Native().fleet_id, course);
    auto*      id           = fleet ? static_cast<int64_t*>(il2cpp_object_unbox(fleet)) : nullptr;
    const auto course_fleet = id ? static_cast<uint64_t>(*id) : 0;
    auto*      node         = Invoke(Native().target, course);
    auto*      address      = Invoke(Native().address, node);
    return course_fleet == pending.selected && SameAddress(Target(Destination), address)
           && Boolean(Native().in_use, course, false) && Boolean(Native().valid, Invoke(Native().path, course), true);
  }
} // namespace

void Cancel()
{
  auto  old       = std::exchange(pending, {});
  auto* owner     = old.roots[Owner] ? il2cpp_gchandle_get_target(old.roots[Owner]) : nullptr;
  auto* coroutine = old.roots[Coroutine] ? il2cpp_gchandle_get_target(old.roots[Coroutine]) : nullptr;
  // Clear ownership before invoking Unity: StopCoroutine can reenter UI work.
  if (OwnerAlive(owner) && coroutine && Native().stop) {
    void* args[]{coroutine};
    Il2CppRuntime::TryInvoke(Native().stop, owner, args);
  }
  for (auto root : old.roots)
    if (root)
      il2cpp_gchandle_free(root);
}

void Begin(FleetPlayerData* selected, FleetPlayerData* discovery, int selected_index, int discovery_index)
{
  Cancel();
  auto* owner = DeploymentManger::Instance();
  if (!selected || !discovery || !owner)
    return;
  if (!Native().Ready()
      || !il2cpp_class_is_assignable_from(Native().behaviour, reinterpret_cast<Il2CppObject*>(owner)->klass)) {
    spdlog::warn("Discovery tow: native coroutine/course contracts unavailable; tow request skipped");
    return;
  }
  LocalRoot owner_root(owner), selected_root(selected), discovery_root(discovery);
  if (!owner_root.handle || !selected_root.handle || !discovery_root.handle
      || !OwnerAlive(reinterpret_cast<Il2CppObject*>(owner)))
    return;
  pending.generation      = ++next_generation;
  pending.selected        = selected->Id;
  pending.discovery       = discovery->Id;
  pending.selected_index  = selected_index;
  pending.discovery_index = discovery_index;
  auto* sections          = Hub::get_SectionManager();
  if (!sections) {
    Cancel();
    return;
  }
  pending.section        = sections->CurrentSection;
  pending.deadline       = Clock::now() + std::chrono::seconds(30);
  Il2CppObject* baseline = nullptr;
  if (!pending.selected || !pending.discovery || !Keep(Owner, owner) || !Keep(Destination, discovery->Address)
      || !Course(owner, baseline) || !Keep(Baseline, baseline)) {
    Cancel();
    return;
  }
  auto* iterator = owner->PlanCourse(selected, Target(Destination), Vector3::zero(), nullptr, nullptr, nullptr);
  if (!Keep(Iterator, iterator) || !IteratorShape(Target(Iterator))) {
    spdlog::warn("Discovery tow: unsupported course iterator; tow request skipped");
    Cancel();
    return;
  }
  LocalRoot iterator_root(iterator);
  if (!iterator_root.handle) {
    Cancel();
    return;
  }
  // StartCoroutine executes its first MoveNext reentrantly. All operation roots
  // and cancellation identity are established before handing ownership to Unity.
  void*         args[]{Target(Iterator)};
  Il2CppObject* coroutine  = nullptr;
  const auto    generation = pending.generation;
  const bool    started    = Il2CppRuntime::TryInvoke(Native().start, owner, args, &coroutine);
  if (pending.generation != generation) {
    if (coroutine && OwnerAlive(reinterpret_cast<Il2CppObject*>(owner))) {
      void* stop_args[]{coroutine};
      Il2CppRuntime::TryInvoke(Native().stop, owner, stop_args);
    }
    return; // A reentrant replacement/cancel already owns the current state.
  }
  if (!started || !coroutine || !Keep(Coroutine, coroutine)) {
    if (coroutine) {
      void* stop_args[]{coroutine};
      Il2CppRuntime::TryInvoke(Native().stop, owner, stop_args);
    }
    spdlog::warn("Discovery tow: native coroutine could not start; tow request skipped");
    Cancel();
  }
}

void Update()
{
  if (!pending.roots[Owner])
    return;
  auto*       owner  = Target(Owner);
  const auto& config = Config::Get();
  const auto  now    = Clock::now();
  if (!config.installHotkeyHooks || !config.hotkeys_enabled || config.use_scopely_hotkeys || !OwnerAlive(owner)
      || now >= pending.deadline) {
    Cancel();
    return;
  }
  if (now < pending.next_poll)
    return;
  pending.next_poll = now + std::chrono::milliseconds(50);
  auto* fleets      = FleetsManager::Instance();
  auto* sections    = Hub::get_SectionManager();
  auto* selected    = fleets ? fleets->GetFleetPlayerData(pending.selected_index) : nullptr;
  auto* discovery   = fleets ? fleets->GetFleetPlayerData(pending.discovery_index) : nullptr;
  if (!sections || sections->CurrentSection != pending.section
      || DeploymentManger::Instance() != reinterpret_cast<DeploymentManger*>(owner) || !selected || !discovery
      || selected->Id != pending.selected || discovery->Id != pending.discovery
      || !SameAddress(Target(Destination), discovery->Address)) {
    Cancel();
    return;
  }
  auto* iterator = Target(Iterator);
  if (!iterator) {
    Cancel();
    return;
  }
  // Client272 MoveNext returns false with state=-1; all true/yield exits store
  // positive states. Poll only between Unity resumes, never advance it here.
  const auto state = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(iterator) + pending.state_offset);
  if (state != -1)
    return;
  const auto selected_id = pending.selected, discovery_id = pending.discovery;
  const bool fresh = FreshCourse(owner);
  LocalRoot  owner_root(owner);
  const auto generation = next_generation;
  Cancel();
  // A throwing/stopped native iterator can also leave state=-1. Require a new,
  // successful course for the exact fleet/destination before requesting tow.
  if (fresh && owner_root.handle && generation == next_generation && OwnerAlive(owner))
    reinterpret_cast<DeploymentManger*>(owner)->SetTowRequest(selected_id, discovery_id);
  else
    spdlog::warn("Discovery tow: course did not finish with a fresh valid destination; tow request skipped");
}
} // namespace discovery_tow
