// The selected fleet and the target viewers in space: selecting ships with the number keys, Discovery towing
// with Shift+ship, engaging, scanning,
// mining, queueing, warping, recalling and repairing from the keyboard, dismissing viewers with Escape, opening a
// target's cargo when it is scanned, the action queue switch, and re-enabling ability buttons whose cooldown ended.

#include "config.h"
#include "discovery_tow_policy.h"
#include "features/base_card.h"
#include "features/features.h"
#include "features/mining_preview.h"
#include "planned_system_warp_policy.h"
#include "runtime_config.h"
#include "settings/basic_gameplay_settings.h"
#include "shortcuts.h"

#include "game/app.h"
#include "game/fleet.h"
#include "game/navigation.h"
#include "game/queues.h"
#include "game/ui.h"
#include "game/viewers.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <unity/input.h>
#include <unity/tracker.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace
{
// --- Fleet actions ---

struct FleetPress {
  bool primary       = false;
  bool secondary     = false;
  bool queue         = false;
  bool queue_clear   = false;
  bool recall        = false;
  bool recall_cancel = false;
  bool repair        = false;
  // A replay of last frame's press, see pending_replay.
  bool retry = false;

  bool Any() const
  { return primary || secondary || queue || queue_clear || recall || recall_cancel || repair; }
};

// Target data may arrive one frame after its card. Keep a bounded owned request, never a borrowed widget/fleet
// across frames, and never replay a press into a different participant, card or rebound target context.
struct DeferredFleetPress {
  FleetPress       press;
  int32_t          frame    = -1;
  int64_t          fleet_id = 0, target_id = 0;
  il2cpp::GcHandle fleet, viewer, context;
};
// Retired on the game thread. Static destruction must not free managed roots after runtime shutdown.
auto& pending_replay = *new std::optional<DeferredFleetPress>;

void DeferFleetPress(const FleetPress& press, FleetPlayerData* fleet, PreScanTargetWidget* viewer,
                     BattleTargetData* context)
{
  pending_replay.reset();
  const auto frame = unity::Time::frameCount();
  if (press.retry || frame < 0 || !fleet || !viewer || !context)
    return;
  const auto fleet_id  = fleet->TryCall<"get_Id", int64_t>();
  const auto target_id = context->TryCall<"get_TargetID", int64_t>();
  if (!fleet_id || fleet_id.value <= 0 || !target_id || target_id.value <= 0)
    return;
  DeferredFleetPress request{press,
                             frame,
                             fleet_id.value,
                             target_id.value,
                             il2cpp::GcHandle::Strong(fleet),
                             il2cpp::GcHandle::Strong(viewer),
                             il2cpp::GcHandle::Strong(context)};
  if (request.fleet && request.viewer && request.context)
    pending_replay = std::move(request);
}

bool FleetReplayMatches(const DeferredFleetPress& request, int32_t frame, FleetPlayerData* fleet,
                        PreScanTargetWidget* viewer, BattleTargetData* context, int64_t fleet_id, int64_t target_id)
{
  return request.frame >= 0 && static_cast<int64_t>(frame) == static_cast<int64_t>(request.frame) + 1 && fleet && viewer
         && context && request.fleet.Get() == fleet && request.viewer.Get() == viewer
         && request.context.Get() == context && fleet_id == request.fleet_id && target_id == request.target_id;
}

void RetireFleetReplay()
{
  if (!pending_replay)
    return;
  const auto now = unity::Time::frameCount();
  if (now < pending_replay->frame || static_cast<int64_t>(now) > static_cast<int64_t>(pending_replay->frame) + 1)
    pending_replay.reset();
}

bool ControllerShown(VisibilityController* controller)
{ return controller && controller->IsShown(); }

template <typename T> bool TargetShown(T* widget, const FleetPress& pressed)
{
  if (!widget || !ControllerShown(widget->visibilityController()))
    return false;
  if (!pressed.primary || !Config::Get().control.primary_requires_visible_target)
    return true;
  auto* parent = widget->Parent();
  return parent && parent->IsShowing();
}

// The cards that show a target or a place in space.
template <typename... Viewers> struct ViewerList {
  static bool AnyShown()
  { return (std::ranges::any_of(unity::Instances<Viewers>(), Shown<Viewers>) || ...); }

  // Hides the shown viewers of the first type that has any.
  static bool HideFirst()
  { return (Hide<Viewers>() || ...); }

private:
  template <typename T> static bool Shown(T* widget)
  { return ControllerShown(widget->visibilityController()); }

  template <typename T> static bool Hide()
  {
    bool hid = false;
    for (auto* widget : unity::Instances<T>())
      if (Shown(widget)) {
        widget->HideAllViewers();
        hid = true;
      }
    return hid;
  }
};

using TargetViewers = ViewerList<AllianceStarbaseObjectViewerWidget, ArmadaObjectViewerWidget,
                                 CelestialObjectViewerWidget, EmbassyObjectViewer, HousingObjectViewerWidget,
                                 MiningObjectViewerWidget, MissionsObjectViewerWidget, PreScanTargetWidget>;

HullType TargetHullType(BattleTargetData* context)
{
  auto* deployed = context ? context->TargetFleetDeployedData() : nullptr;
  auto* hull     = deployed ? deployed->Hull() : nullptr;
  return hull ? hull->Type() : HullType::Any;
}

// A viewer without a visibility controller counts as visible.
bool ArmadaViewerShown(ArmadaObjectViewerWidget* armada)
{
  if (!armada)
    return false;
  if (auto* controller = armada->visibilityController()) {
    const auto state = controller->State();
    return state == VisibilityState::Visible || state == VisibilityState::Show;
  }
  static bool reported = false;
  if (!std::exchange(reported, true))
    spdlog::warn("[Hotkeys] the armada viewer has no visibility controller; treating it as shown");
  return true;
}

// Requests `type` in one of `states`. If that did not succeed, a fleet already in `help_state` asks for alliance
// help. Preserve the frozen short circuit: successful normal actions do not also submit a help action.
bool RequestFleetAction(FleetLocalViewController* panel, FleetPlayerData* fleet, ActionType type,
                        std::span<const FleetState> states, FleetState help_state = FleetState::Unknown)
{
  bool requested = false;
  if (std::ranges::contains(states, fleet->CurrentState())) {
    if (type != ActionType::Recall || !base_card::KeepOpenForRecall(fleet->Id()))
      NavigationSectionManager::HideInteraction();
    requested = panel->RequestAction(fleet, type, 0, ActionBehaviour::Default);
  }
  if (help_state != FleetState::Unknown && (requested || fleet->CurrentState() == help_state))
    requested = requested || panel->RequestAction(fleet, type, 0, ActionBehaviour::AskHelp);
  spdlog::trace("[Hotkeys] action {} on fleet {} in state {}: {}", type.flagValue, fleet->Id(),
                std::to_underlying(fleet->CurrentState()), requested);
  return requested;
}

// Acts on a shown pre-scan card. Returns true when the press was used, including when it was deferred to the next
// frame.
bool ActOnPreScan(PreScanTargetWidget* pre_scan, const FleetPress& pressed, FleetPlayerData* fleet,
                  ActionQueueManager* queue)
{
  auto* buttons = pre_scan->scanEngageButtonsWidget();

  // A mining node's card sits on top of the pre-scan: scan it or start mining.
  if (auto* mine = unity::Newest<MiningObjectViewerWidget>(); TargetShown(mine, pressed)) {
    if (pressed.retry)
      return true; // A saved pre-scan action cannot transfer to an overlapping mining card.
    if (pressed.secondary && buttons) {
      buttons->OnScanButtonClicked();
      return true;
    }
    if (pressed.primary) {
      mine->MineClicked();
      return true;
    }
  }

  if (auto* add_to_queue = pre_scan->addToQueueButtonWidget();
      pressed.queue && queue->IsQueueUnlocked() && add_to_queue && buttons) {
    const auto type = TargetHullType(buttons->Context());
    if (type == HullType::Any && !pressed.retry) {
      DeferFleetPress(pressed, fleet, pre_scan, buttons->Context());
      return true;
    }
    if (type != HullType::ArmadaTarget && (type != HullType::Any || pressed.retry)) {
      if (add_to_queue->isActiveAndEnabled()) {
        if (auto* listener = add_to_queue->semaphoreButtonListener(); listener && !queue->IsQueueFull(fleet)) {
          if (auto* button = listener->button()) {
            button->Press();
            TargetViewers::HideFirst();
          }
        }
        return true;
      }
      if (type == HullType::Any) {
        DeferFleetPress(pressed, fleet, pre_scan, buttons->Context());
        return true;
      }
    }
  }

  if (pressed.secondary && buttons) {
    buttons->OnScanButtonClicked();
    return true;
  }

  if (pressed.primary && buttons && buttons->enabled()) {
    const auto type = TargetHullType(buttons->Context());
    // An armada target whose armada card is already open is joined from that card instead.
    const bool joins_from_card =
        type == HullType::ArmadaTarget && ArmadaViewerShown(unity::Newest<ArmadaObjectViewerWidget>());
    if (!joins_from_card && (type != HullType::Any || pressed.retry)) {
      if (type != HullType::ArmadaTarget) {
        buttons->OnEngageButtonClicked();
      } else if (auto* attack = pre_scan->armadaAttackButton(); attack && attack->isActiveAndEnabled()) {
        if (auto* button = attack->UnityButton())
          button->Press();
      } else {
        buttons->OnArmadaButtonClicked();
      }
      return true;
    }
    if (type == HullType::Any) {
      DeferFleetPress(pressed, fleet, pre_scan, buttons->Context());
      return true;
    }
  }
  return false;
}

template <class T> std::optional<T> WarpObservation(const il2cpp::CallResult<T>& value)
{ return value ? std::optional{value.value} : std::nullopt; }
template <class T> il2cpp::GcHandle WarpRoot(const il2cpp::CallResult<T*>& value)
{
  if (!value || !value.value)
    return {};
  auto root = il2cpp::GcHandle::Strong(value.value);
  if (!root || !root.Get() || !root.template Get<il2cpp::Object>()->template Is<T>())
    return {};
  return root;
}
template <class T> il2cpp::GcHandle WarpRoot(T* value)
{ return WarpRoot(il2cpp::CallResult<T*>{true, value}); }
planned_system_warp::Vec3 WarpPosition(unity::Vector3 position)
{ return {position.x, position.y, position.z}; }
std::uintptr_t WarpIdentity(const il2cpp::GcHandle& root)
{ return reinterpret_cast<std::uintptr_t>(root.Get()); }

// --- Discovery towing ---

namespace tow_native = discovery_tow_binding;
using TowClock       = std::chrono::steady_clock;
struct DiscoveryTowRun {
  discovery_tow::Request request;
  il2cpp::GcHandle       owner, selected, discovery, destination, baseline, iterator, coroutine, callbacks;
  TowClock::time_point   next_poll{};
  bool                   starting = true;
};
struct DiscoveryTowState {
  std::uint64_t                    generation = 0;
  std::shared_ptr<DiscoveryTowRun> run;
  bool                             updating = false;
};
DiscoveryTowState& DiscoveryTowing()
{
  // Pending roots are released by the game-thread cancel/finish path, never by
  // a static destructor calling IL2CPP after runtime shutdown.
  static auto* state = new DiscoveryTowState;
  return *state;
}
std::int64_t TowMilliseconds()
{ return std::chrono::duration_cast<std::chrono::milliseconds>(TowClock::now().time_since_epoch()).count(); }
bool TowInputMode()
{
  const auto& config = Config::Get();
  return config.control.hotkeys_enabled && !config.control.use_scopely_hotkeys && !Shortcut::CaptureBlocksShortcuts();
}
bool TowOwnerAlive(DeploymentManager* owner)
{
  if (!owner || !owner->Is<unity::UnityObject>())
    return false;
  const auto alive = unity::TryAlive(owner);
  return alive && alive.value;
}
bool TowCurrent(const std::shared_ptr<DiscoveryTowRun>& run)
{ return run && DiscoveryTowing().run == run && DiscoveryTowing().generation == run->request.generation; }
void StopDiscoveryTow(const std::shared_ptr<DiscoveryTowRun>& run)
{
  if (run && TowOwnerAlive(run->owner.Get<DeploymentManager>())) {
    // The iterator overload also retires a StartCoroutine that changed native
    // state and then failed without returning a Coroutine handle.
    if (!tow_native::Stop(run->owner.Get<DeploymentManager>(), run->iterator.Get<tow_native::Routine>(),
                          run->coroutine.Get<tow_native::Coroutine>()))
      spdlog::warn("[Hotkeys] Discovery course stop unconfirmed; tow submission retired");
  }
}
void CancelDiscoveryTow()
{
  auto& state = DiscoveryTowing();
  ++state.generation;
  auto old = std::exchange(state.run, {}); // StopCoroutine may reenter; it never owns the replacement request.
  StopDiscoveryTow(old);
}
struct DiscoveryTowObservation {
  discovery_tow::Evidence evidence;
  il2cpp::GcHandle        course;
};
discovery_tow::Evidence ObserveDiscoveryTowSource(const std::shared_ptr<DiscoveryTowRun>& run)
{
  discovery_tow::Evidence e;
  e.generation  = DiscoveryTowing().generation;
  e.now_ms      = TowMilliseconds();
  e.input_mode  = TowInputMode();
  auto* owner   = run->owner.Get<DeploymentManager>();
  e.owner_alive = TowOwnerAlive(owner);
  if (!e.owner_alive)
    return e;
  auto current_owner =
      WarpRoot(planned_warp_binding::StaticReference<"get_Instance", DeploymentManager, DeploymentManager>());
  auto fleets   = WarpRoot(planned_warp_binding::StaticReference<"get_Instance", FleetsManager, FleetsManager>());
  auto sections = WarpRoot(
      planned_warp_binding::StaticReference<"get_Instance", NavigationSectionManager, NavigationSectionManager>());
  if (!current_owner || !fleets || !sections)
    return e;
  e.owner            = WarpIdentity(current_owner);
  const auto section = sections.Get<NavigationSectionManager>()->TryCall<"get_CurrentSection", SectionID>();
  if (section)
    e.section = std::to_underlying(section.value);
  auto selected  = WarpRoot(fleets.Get<FleetsManager>()->TryCall<"GetFleetPlayerData", FleetPlayerData*(int32_t)>(
      run->request.selected_index));
  auto discovery = WarpRoot(fleets.Get<FleetsManager>()->TryCall<"GetFleetPlayerData", FleetPlayerData*(int32_t)>(
      run->request.discovery_index));
  if (!selected || !discovery || current_owner.Get() != run->owner.Get())
    return e;
  e.selected_object  = WarpIdentity(selected);
  e.discovery_object = WarpIdentity(discovery);
  e.selected         = WarpObservation(selected.Get<FleetPlayerData>()->TryId());
  e.discovery        = WarpObservation(discovery.Get<FleetPlayerData>()->TryId());
  auto address       = WarpRoot(discovery.Get<FleetPlayerData>()->TryAddress());
  if (address)
    e.destination_matches = WarpObservation(
        run->destination.Get<NodeAddress>()->TryCall<"Equals", bool(NodeAddress*)>(address.Get<NodeAddress>()));
  e.can_tow      = WarpObservation(owner->TryCall<"CanTow", bool(FleetPlayerData*)>(discovery.Get<FleetPlayerData>()));
  e.can_be_towed = WarpObservation(owner->TryCall<"CanBeTowed", bool(FleetPlayerData*, FleetPlayerData*)>(
      selected.Get<FleetPlayerData>(), discovery.Get<FleetPlayerData>()));
  e.generation   = DiscoveryTowing().generation;
  e.input_mode   = TowInputMode();
  e.now_ms       = TowMilliseconds();
  return e;
}
DiscoveryTowObservation ObserveDiscoveryTow(const std::shared_ptr<DiscoveryTowRun>& run,
                                            const discovery_tow::Request&           expected)
{
  DiscoveryTowObservation result;
  auto&                   e = result.evidence;
  e                         = ObserveDiscoveryTowSource(run);
  auto* owner               = run->owner.Get<DeploymentManager>();
  // The final recheck passes its new retirement ticket explicitly. Do not query
  // a course on an unavailable/replaced source merely to reject it afterward.
  if (!owner || !discovery_tow::SourceMatches(expected, e))
    return result;
  const auto state = tow_native::State(run->iterator.Get<tow_native::Routine>());
  e.iterator_state = WarpObservation(state);
  if (!state || state.value != -1)
    return result;
  const auto course = owner->TryPlannedCourse(run->request.selected);
  if (!course)
    return result;
  if (!course.value) {
    e.course = 0; // Successful null is distinct from unavailable observation.
    return result;
  }
  result.course = WarpRoot(course);
  if (!result.course)
    return result;
  e.course            = WarpIdentity(result.course);
  e.course_fleet      = WarpObservation(result.course.Get<CourseData>()->TryFleetId());
  e.course_in_use     = WarpObservation(result.course.Get<CourseData>()->TryInUse());
  auto path           = WarpRoot(result.course.Get<CourseData>()->TryPath());
  auto target         = WarpRoot(result.course.Get<CourseData>()->TryTarget());
  auto target_address = target ? WarpRoot(target.Get<CourseNode>()->TryAddress()) : il2cpp::GcHandle{};
  if (path)
    e.course_valid = WarpObservation(path.Get<PathFinderResultData>()->TryCall<"get_Valid", bool>());
  if (target_address)
    e.course_destination_matches = WarpObservation(
        run->destination.Get<NodeAddress>()->TryCall<"Equals", bool(NodeAddress*)>(target_address.Get<NodeAddress>()));
  e.generation = DiscoveryTowing().generation; // Every managed call above can reenter cancellation.
  return result;
}
DiscoveryTowObservation ObserveDiscoveryTow(const std::shared_ptr<DiscoveryTowRun>& run)
{ return ObserveDiscoveryTow(run, run->request); }
void DiscoveryTowFrame()
{
  auto&      state = DiscoveryTowing();
  const auto run   = state.run;
  if (!run || state.updating)
    return;
  if (!TowInputMode()) {
    CancelDiscoveryTow();
    return;
  }
  if (TowMilliseconds() >= run->request.deadline_ms) {
    CancelDiscoveryTow();
    return;
  }
  if (run->starting || TowClock::now() < run->next_poll)
    return;
  state.updating = true;
  struct Updating {
    bool& active;
    ~Updating()
    { active = false; }
  } updating{state.updating};
  run->next_poll      = TowClock::now() + std::chrono::milliseconds(50);
  const auto observed = ObserveDiscoveryTow(run);
  if (!TowCurrent(run))
    return;
  const auto decision = discovery_tow::Evaluate(run->request, observed.evidence);
  if (decision == discovery_tow::Decision::Wait)
    return;
  if (decision == discovery_tow::Decision::Cancel) {
    CancelDiscoveryTow();
    return;
  }
  // Retire BEFORE StopCoroutine or submitting: both can reenter. A replacement
  // request wins, and this exact operation can submit at most once.
  state.run.reset();
  const auto ticket = ++state.generation;
  StopDiscoveryTow(run);
  if (state.generation != ticket || state.run)
    return;
  auto completed       = run->request;
  completed.generation = ticket;
  const auto current   = ObserveDiscoveryTow(run, completed);
  if (state.generation != ticket || state.run || !TowInputMode() || !TowOwnerAlive(run->owner.Get<DeploymentManager>())
      || current.course.Get() != observed.course.Get()
      || discovery_tow::Evaluate(completed, current.evidence) != discovery_tow::Decision::Submit)
    return;
  // Course getters above can synchronously change section/input/source without
  // cancelling the request. Refresh those identities at the command boundary.
  const auto source = ObserveDiscoveryTowSource(run);
  if (state.generation != ticket || state.run || !discovery_tow::SourceMatches(completed, source))
    return;
  const auto submitted = tow_native::Submit(run->owner.Get<DeploymentManager>(), completed.selected,
                                            completed.discovery, run->callbacks.Get<tow_native::Reference>());
  spdlog::info("[Hotkeys] Discovery tow for fleet {}: {}", completed.selected,
               submitted ? "request submitted" : "request unavailable");
}
bool BeginDiscoveryTow(FleetBarViewController* bar, int32_t index)
{
  auto&      state  = DiscoveryTowing();
  const auto ticket = ++state.generation;
  auto       old    = std::exchange(state.run, {});
  StopDiscoveryTow(old);
  if (state.generation != ticket || !TowInputMode() || !tow_native::Native().ready)
    return false;
  auto owner  = WarpRoot(planned_warp_binding::StaticReference<"get_Instance", DeploymentManager, DeploymentManager>());
  auto fleets = WarpRoot(planned_warp_binding::StaticReference<"get_Instance", FleetsManager, FleetsManager>());
  auto sections = WarpRoot(
      planned_warp_binding::StaticReference<"get_Instance", NavigationSectionManager, NavigationSectionManager>());
  auto context = WarpRoot(bar->TryCall<"get_CanvasContext", FleetBarContext*>());
  if (!owner || !fleets || !sections || !context || !TowOwnerAlive(owner.Get<DeploymentManager>()))
    return false;
  auto selected =
      WarpRoot(fleets.Get<FleetsManager>()->TryCall<"GetFleetPlayerData", FleetPlayerData*(int32_t)>(index));
  auto data = WarpRoot(context.template Get<FleetBarContext>()->TryCall<"get_FleetData", il2cpp::ArrayObject*>());
  if (!selected || !data)
    return false;
  const auto array = il2cpp::TryReferenceArray<FleetPlayerData*>(data.Get());
  if (!array || !array.value)
    return false;
  il2cpp::GcHandle discovery;
  int32_t          discovery_slot = -1;
  for (auto* candidate : il2cpp::Items(array.value)) {
    auto root = WarpRoot(candidate);
    if (!candidate)
      continue;
    if (!root)
      return false;
    const auto hull = planned_warp_binding::Scalar<"get_HullId", int64_t>(root.template Get<FleetPlayerData>());
    const auto slot = planned_warp_binding::Scalar<"get_Index", int32_t>(root.template Get<FleetPlayerData>());
    if (!hull || !slot)
      return false;
    if (hull.value == 1307832955 && slot.value >= 0 && (discovery_slot < 0 || slot.value < discovery_slot)) {
      discovery      = std::move(root);
      discovery_slot = slot.value;
    }
  }
  if (!discovery)
    return false;
  const auto selected_id     = selected.Get<FleetPlayerData>()->TryId();
  const auto discovery_id    = discovery.Get<FleetPlayerData>()->TryId();
  const auto discovery_index = planned_warp_binding::Scalar<"get_Index", int32_t>(discovery.Get<FleetPlayerData>());
  const auto section         = sections.Get<NavigationSectionManager>()->TryCall<"get_CurrentSection", SectionID>();
  auto       address         = WarpRoot(discovery.Get<FleetPlayerData>()->TryAddress());
  auto       destination =
      address ? WarpRoot(address.Get<NodeAddress>()->TryCall<"Clone", NodeAddress*>()) : il2cpp::GcHandle{};
  if (!selected_id || !discovery_id || !discovery_index || !section || !destination || selected_id.value <= 0
      || discovery_id.value <= 0 || selected_id.value == discovery_id.value || discovery_index.value != discovery_slot
      || discovery_index.value == index)
    return false;
  const auto can_tow =
      owner.Get<DeploymentManager>()->TryCall<"CanTow", bool(FleetPlayerData*)>(discovery.Get<FleetPlayerData>());
  const auto can_be_towed =
      owner.Get<DeploymentManager>()->TryCall<"CanBeTowed", bool(FleetPlayerData*, FleetPlayerData*)>(
          selected.Get<FleetPlayerData>(), discovery.Get<FleetPlayerData>());
  if (!can_tow || !can_tow.value || !can_be_towed || !can_be_towed.value)
    return false;
  auto run            = std::make_shared<DiscoveryTowRun>();
  run->callbacks      = tow_native::Callbacks(); // Construct/read back real delegates before native work starts.
  const auto baseline = owner.Get<DeploymentManager>()->TryPlannedCourse(selected_id.value);
  if (!run->callbacks || !baseline)
    return false;
  run->baseline = WarpRoot(baseline);
  if (baseline.value && !run->baseline)
    return false;
  run->owner       = std::move(owner);
  run->selected    = std::move(selected);
  run->discovery   = std::move(discovery);
  run->destination = std::move(destination);
  run->request     = {.generation       = ticket,
                      .selected         = selected_id.value,
                      .discovery        = discovery_id.value,
                      .selected_index   = index,
                      .discovery_index  = discovery_index.value,
                      .section          = std::to_underlying(section.value),
                      .owner            = WarpIdentity(run->owner),
                      .selected_object  = WarpIdentity(run->selected),
                      .discovery_object = WarpIdentity(run->discovery),
                      .baseline         = WarpIdentity(run->baseline),
                      .deadline_ms      = TowMilliseconds() + 30'000};
  if (state.generation != ticket)
    return false;
  state.run              = run;
  const auto before_plan = ObserveDiscoveryTowSource(run);
  if (!TowCurrent(run) || !discovery_tow::SourceMatches(run->request, before_plan)) {
    if (TowCurrent(run))
      CancelDiscoveryTow();
    return false;
  }
  run->iterator = WarpRoot(tow_native::Plan(run->owner.Get<DeploymentManager>(), run->selected.Get<FleetPlayerData>(),
                                            run->destination.Get<NodeAddress>()));
  if (!TowCurrent(run))
    return false;
  if (!run->iterator
      || !tow_native::FreshIterator(run->iterator.Get<tow_native::Routine>(), run->selected.Get<FleetPlayerData>(),
                                    run->destination.Get<NodeAddress>())) {
    CancelDiscoveryTow();
    return false;
  }
  const auto before_start = ObserveDiscoveryTowSource(run);
  if (!TowCurrent(run) || !discovery_tow::SourceMatches(run->request, before_start)) {
    if (TowCurrent(run))
      CancelDiscoveryTow();
    return false;
  }
  const auto started = tow_native::Start(run->owner.Get<DeploymentManager>(), run->iterator.Get<tow_native::Routine>());
  run->coroutine     = WarpRoot(started);
  if (!TowCurrent(run)) {
    StopDiscoveryTow(run);
    return false;
  }
  if (!started || !run->coroutine) {
    CancelDiscoveryTow();
    return false;
  }
  run->starting = false;
  return true;
}

planned_system_warp::Visibility WarpViewerVisibility(StarNodeObjectViewerWidget* node)
{
  auto source = WarpRoot(node);
  if (!source)
    return planned_system_warp::Visibility::Unavailable;
  auto visibility = WarpRoot(source.Get<StarNodeObjectViewerWidget>()->visibilityController());
  if (!visibility || !VisibilityController::Class::HasField<"_state", VisibilityState>())
    return planned_system_warp::Visibility::Unavailable;
  const auto state = visibility.Get<VisibilityController>()->state();
  if (state != VisibilityState::Visible && state != VisibilityState::Show)
    return state == VisibilityState::Hide || state == VisibilityState::Hidden
               ? planned_system_warp::Visibility::Hidden
               : planned_system_warp::Visibility::Unavailable;
  auto       parent = WarpRoot(planned_warp_binding::Reference<"get_Parent", ParentObjectViewerViewController>(
      source.Get<StarNodeObjectViewerWidget>()));
  const auto showing =
      parent ? planned_warp_binding::Scalar<"get_IsShowing", bool>(parent.Get<ParentObjectViewerViewController>())
             : il2cpp::CallResult<bool>{};
  return !showing        ? planned_system_warp::Visibility::Unavailable
         : showing.value ? planned_system_warp::Visibility::Visible
                         : planned_system_warp::Visibility::Hidden;
}

// Resume only an unused ordinary course that still describes this fleet and the retained galaxy selection.
// Every managed source is operation-local and rooted; all policy observations are copied values.
template <class ReadContext> bool TryPlannedSystemWarp(FleetPlayerData* fleet, ReadContext&& read_context)
{
  try {
    auto fleet_root = WarpRoot(fleet);
    auto context    = WarpRoot(read_context());
    if (!fleet_root || !context || Hub::CurrentSection() != SectionID::Navigation_Galaxy)
      return false;
    const auto fleet_id = fleet_root.template Get<FleetPlayerData>()->TryId();
    if (!fleet_id)
      return false;
    auto deployment =
        WarpRoot(planned_warp_binding::StaticReference<"get_Instance", DeploymentManager, DeploymentManager>());
    auto section = WarpRoot(
        planned_warp_binding::StaticReference<"get_Instance", NavigationSectionManager, NavigationSectionManager>());
    if (!deployment || !section)
      return false;
    auto navigation = WarpRoot(planned_warp_binding::Reference<"get_NavigationManager", NavigationManager>(
        section.Get<NavigationSectionManager>()));
    if (!navigation)
      return false;
    auto controller = WarpRoot(navigation.Get<NavigationManager>()->TryInteractionController());
    if (!controller)
      return false;
    auto helper = WarpRoot(controller.Get<NavigationInteractionUIViewController>()->TryFleetHelper());
    if (!helper)
      return false;
    LocaleTextContext* message      = nullptr;
    const auto         can_navigate = helper.Get<FleetNavigationHelper>()->TryValidateInput(fleet_id.value, message);
    auto               message_root = WarpRoot(message);
    if (!can_navigate || !can_navigate.value || (message && !message_root))
      return false;
    auto course = WarpRoot(deployment.Get<DeploymentManager>()->TryPlannedCourse(fleet_id.value));
    if (!course)
      return false;
    auto path          = WarpRoot(course.Get<CourseData>()->TryPath());
    auto target        = WarpRoot(course.Get<CourseData>()->TryTarget());
    auto start         = WarpRoot(course.Get<CourseData>()->TryStart());
    auto fleet_address = WarpRoot(fleet_root.template Get<FleetPlayerData>()->TryAddress());
    if (!path || !target || !start || !fleet_address)
      return false;
    auto target_address = WarpRoot(target.Get<CourseNode>()->TryAddress());
    auto start_address  = WarpRoot(start.Get<CourseNode>()->TryAddress());
    auto context_path   = WarpRoot(context.template Get<NavigationInteractionUIContext>()->TryPath());
    auto animator       = WarpRoot(navigation.Get<NavigationManager>()->TryTransitionAnimator());
    if (!target_address || !start_address || !context_path || !animator)
      return false;

    planned_system_warp::Evidence evidence{.primary = true, .galaxy = true};
    evidence.selected_fleet_id   = fleet_id.value;
    evidence.course_fleet_id     = WarpObservation(course.Get<CourseData>()->TryFleetId());
    evidence.native_can_navigate = can_navigate.value;
    evidence.course_type         = WarpObservation(course.Get<CourseData>()->TryType());
    evidence.target_action       = WarpObservation(course.Get<CourseData>()->TryTargetAction());
    evidence.in_use              = WarpObservation(course.Get<CourseData>()->TryInUse());
    evidence.is_warp             = WarpObservation(course.Get<CourseData>()->TryWarp());
    evidence.is_recall           = WarpObservation(course.Get<CourseData>()->TryRecall());
    evidence.is_wormhole         = WarpObservation(course.Get<CourseData>()->TryWormhole());
    evidence.has_toll            = WarpObservation(course.Get<CourseData>()->TryToll());
    evidence.path_result         = WarpObservation(path.Get<PathFinderResultData>()->TryResult());
    evidence.target_depth        = WarpObservation(target_address.Get<NodeAddress>()->TryNodeDepth());
    evidence.start_system        = WarpObservation(start_address.Get<NodeAddress>()->TrySystem());
    evidence.start_instance      = WarpObservation(start_address.Get<NodeAddress>()->TryInstance());
    evidence.fleet_system        = WarpObservation(fleet_address.Get<NodeAddress>()->TrySystem());
    evidence.fleet_instance      = WarpObservation(fleet_address.Get<NodeAddress>()->TryInstance());
    evidence.target_node_id      = WarpObservation(target.Get<CourseNode>()->TryId());
    const auto destination       = target.Get<CourseNode>()->TrySystemPosition();
    const auto galaxy_position   = target.Get<CourseNode>()->TryGalaxyPosition();
    const auto retained_position = context.template Get<NavigationInteractionUIContext>()->TryPosition();
    if (destination)
      evidence.system_position = WarpPosition(destination.value);
    if (galaxy_position)
      evidence.target_galaxy_position = WarpPosition(galaxy_position.value);
    if (retained_position) {
      const auto projected = animator.Get<NavigationTransitionAnimator>()->TryWorldToDepth(retained_position.value, 1);
      if (projected)
        evidence.projected_context_position = WarpPosition(projected.value);
    }
    const auto observed_tree = context.template Get<NavigationInteractionUIContext>()->TryTreeNode();
    auto       tree          = WarpRoot(observed_tree);
    if (observed_tree && !observed_tree.value)
      evidence.tree_node = 0;
    else if (tree) {
      evidence.tree_node = WarpIdentity(tree);
      evidence.tree_id   = WarpObservation(tree.template Get<ITreeNode>()->TryId());
      evidence.tree_type = WarpObservation(tree.template Get<ITreeNode>()->TryType());
    }
    evidence.path_identity = {WarpIdentity(path), WarpIdentity(context_path)};

    // Getter/validator calls can reenter game code. Recheck the selected source before dispatch, without keeping
    // any previous result across actions or using a different fleet/context's destination.
    auto current_context = WarpRoot(read_context());
    auto panel           = WarpRoot(FleetBarViewController::NewestPanel());
    auto current_fleet   = panel ? WarpRoot(planned_warp_binding::Reference<"get_fleet", FleetPlayerData>(
                                       panel.Get<FleetLocalViewController>()))
                                 : il2cpp::GcHandle{};
    auto current_course  = WarpRoot(deployment.Get<DeploymentManager>()->TryPlannedCourse(fleet_id.value));
    if (!current_context || !current_fleet || !current_course || WarpIdentity(course) != WarpIdentity(current_course)
        || Hub::CurrentSection() != SectionID::Navigation_Galaxy)
      return false;
    const auto current_fleet_id = current_fleet.Get<FleetPlayerData>()->TryId();
    if (!current_fleet_id || current_fleet_id.value != fleet_id.value)
      return false;
    auto       current_path     = WarpRoot(current_course.Get<CourseData>()->TryPath());
    auto       retained_path    = WarpRoot(current_context.template Get<NavigationInteractionUIContext>()->TryPath());
    const auto current_position = current_context.template Get<NavigationInteractionUIContext>()->TryPosition();
    if (!current_path || !retained_path || WarpIdentity(current_path) != WarpIdentity(path)
        || WarpIdentity(retained_path) != WarpIdentity(context_path) || !current_position || !retained_position
        || WarpPosition(current_position.value) != WarpPosition(retained_position.value))
      return false;
    evidence.fleet_identity   = {WarpIdentity(fleet_root), WarpIdentity(current_fleet)};
    evidence.context_identity = {WarpIdentity(context), WarpIdentity(current_context)};
    const auto request        = planned_system_warp::Prepare(evidence);
    return request
           && navigation.Get<NavigationManager>()
                  ->TryMovePlanned({request->system_position.x, request->system_position.y, request->system_position.z},
                                   request->node_id, request->fleet_id)
                  .succeeded;
  } catch (...) {
    return false; // Optional recovery failed; the consumed hidden action is never retried or sent elsewhere.
  }
}

// Acts on whatever other viewer is open: a mining node, a system node, or an empty spot in space. Returns true
// when the press was used.
bool ActOnViewer(const FleetPress& pressed, FleetPlayerData* fleet)
{
  if (auto* mine = unity::Newest<MiningObjectViewerWidget>(); TargetShown(mine, pressed)) {
    if (auto* buttons = mine->scanEngageButtonsWidget(); pressed.secondary && buttons) {
      if (!buttons->Context())
        return false;
      buttons->OnScanButtonClicked();
      return true;
    }
    if (pressed.primary) {
      mine->MineClicked();
      return true;
    }
    return false;
  }

  if (auto* node = unity::Newest<StarNodeObjectViewerWidget>(); node && node->Context()) {
    const bool galaxy     = Hub::CurrentSection() == SectionID::Navigation_Galaxy;
    const auto visibility = pressed.primary ? WarpViewerVisibility(node) : planned_system_warp::Visibility::Unavailable;
    if (planned_system_warp::ViewerEligible(pressed.primary, galaxy, visibility,
                                            Config::Get().control.primary_requires_visible_target)) {
      if (pressed.secondary) {
        node->OnViewButtonActivation();
        return true;
      }
      const auto route = planned_system_warp::RoutePrimary(pressed.primary, pressed.secondary, galaxy, visibility,
                                                           Config::Get().control.primary_requires_visible_target);
      if (route == planned_system_warp::PrimaryRoute::Retained) {
        auto source = WarpRoot(node);
        if (source)
          (void)TryPlannedSystemWarp(fleet, [&] {
            auto* current = source.Get<StarNodeObjectViewerWidget>();
            if (!current || current != unity::Newest<StarNodeObjectViewerWidget>()
                || !current->Is<unity::UnityObject>())
              return il2cpp::CallResult<NavigationInteractionUIContext*>{};
            const auto alive = unity::TryAlive(current);
            if (!alive || !alive.value)
              return il2cpp::CallResult<NavigationInteractionUIContext*>{};
            return planned_warp_binding::Reference<"get_Context", NavigationInteractionUIContext>(
                source.Get<StarNodeObjectViewerWidget>());
          });
        return true;
      }
      if (route == planned_system_warp::PrimaryRoute::Native) {
        node->InitiateWarp();
        return true;
      }
    }
  }

  auto* navigation = unity::Newest<NavigationInteractionUIViewController>();
  if (!navigation || !pressed.primary)
    return false;

  auto* armada = unity::Newest<ArmadaObjectViewerWidget>();
  if (!ArmadaViewerShown(armada)) {
    if (Hub::CurrentSection() == SectionID::Navigation_Galaxy && !navigation->IsSetCourseVisible()) {
      auto source = WarpRoot(navigation);
      if (source)
        (void)TryPlannedSystemWarp(fleet, [&] {
          auto* current = source.Get<NavigationInteractionUIViewController>();
          if (!current || current != unity::Newest<NavigationInteractionUIViewController>()
              || !current->Is<unity::UnityObject>())
            return il2cpp::CallResult<NavigationInteractionUIContext*>{};
          const auto alive = unity::TryAlive(current);
          return alive && alive.value ? current->TryContext() : il2cpp::CallResult<NavigationInteractionUIContext*>{};
        });
      return true;
    }
    if (Config::Get().control.primary_requires_visible_target && !navigation->IsSetCourseVisible())
      return false;
    navigation->OnSetCourseButtonClick();
    return true;
  }
  if (auto* join = armada->joinContext(); join && join->Interactable()) {
    armada->ValidateThenJoinArmada();
    return true;
  }
  return false;
}

void ActOnSelectedFleet(const FleetPress& pressed, const DeferredFleetPress* replay = nullptr)
{
  auto* panel = FleetBarViewController::NewestPanel();
  auto* fleet = panel ? panel->fleet() : nullptr;
  if (!fleet)
    return;
  auto* queue = ActionQueueManager::Instance();
  if (!queue)
    return;

  if (replay) {
    auto*      viewer = replay->viewer.Get<PreScanTargetWidget>();
    const auto alive  = viewer ? unity::TryAlive(viewer) : il2cpp::CallResult<bool>{};
    if (!alive || !alive.value)
      return;
    auto*      buttons   = viewer->scanEngageButtonsWidget();
    auto*      context   = buttons ? buttons->Context() : nullptr;
    const auto fleet_id  = fleet->TryCall<"get_Id", int64_t>();
    const auto target_id = context ? context->TryCall<"get_TargetID", int64_t>() : il2cpp::CallResult<int64_t>{};
    if (fleet_id && target_id
        && FleetReplayMatches(*replay, unity::Time::frameCount(), fleet, viewer, context, fleet_id.value,
                              target_id.value)
        && TargetShown(viewer, pressed))
      (void)ActOnPreScan(viewer, pressed, fleet, queue);
    return; // An expired/replaced target must never fall through to a different visible card or travel action.
  }

  if (pressed.queue_clear) {
    queue->ClearQueue(fleet);
    return;
  }
  const auto state = fleet->CurrentState();
  if (pressed.recall_cancel && (state == FleetState::WarpCharging || state == FleetState::Warping)) {
    panel->CancelButtonClicked();
    return;
  }

  for (auto* pre_scan : unity::Instances<PreScanTargetWidget>())
    if (TargetShown(pre_scan, pressed) && ActOnPreScan(pre_scan, pressed, fleet, queue))
      return;

  if (ActOnViewer(pressed, fleet))
    return;

  static constexpr std::array kRecallable{FleetState::IdleInSpace, FleetState::Impulsing, FleetState::Mining,
                                          FleetState::Capturing};
  static constexpr std::array kRepairable{FleetState::Docked, FleetState::Destroyed};
  if (pressed.recall && RequestFleetAction(panel, fleet, ActionType::Recall, kRecallable))
    return;
  if (pressed.repair)
    RequestFleetAction(panel, fleet, ActionType::Repair, kRepairable, FleetState::Repairing);
}

// --- Target cargo ---

// A delayed cargo request belongs to the exact visible card, rewards widget and target context. The original
// one-/five-pass retry windows are absolute Unity-frame windows, so skipped shortcut dispatch cannot keep work
// alive indefinitely or transfer it to a newly scanned target.
struct PendingTargetCargo {
  il2cpp::GcHandle viewer, rewards, context, controller;
  int64_t          target_id = 0;
  int32_t          frame = -1, last_frame = -1, remaining_passes = 0;
  bool             automatic = false;
};
auto&    pending_target_cargo     = *new std::vector<PendingTargetCargo>;
uint64_t cargo_request_generation = 0;

bool CargoStateShown(VisibilityState state)
{ return state == VisibilityState::Show || state == VisibilityState::Visible; }

void CancelTargetCargo()
{
  pending_target_cargo.clear();
  ++cargo_request_generation; // Also invalidates a detached request being processed inside a reentrant native call.
}
void CancelTargetCargoForWidget(RewardsButtonWidget* widget)
{
  std::erase_if(pending_target_cargo, [widget](const auto& request) { return request.rewards.Get() == widget; });
  ++cargo_request_generation;
}

std::optional<PendingTargetCargo> ObserveTargetCargo(PreScanTargetWidget* pre_scan)
{
  PendingTargetCargo result;
  result.viewer     = WarpRoot(pre_scan);
  auto*      viewer = result.viewer.Get<PreScanTargetWidget>();
  const auto alive  = viewer ? unity::TryAlive(viewer) : il2cpp::CallResult<bool>{};
  const auto active = viewer ? viewer->TryCall<"get_isActiveAndEnabled", bool>() : il2cpp::CallResult<bool>{};
  if (!alive || !alive.value || !active || !active.value)
    return {};
  auto       visibility = WarpRoot(viewer->TryVisibilityController());
  const auto shown =
      visibility ? visibility.Get<VisibilityController>()->TryState() : il2cpp::CallResult<VisibilityState>{};
  if (!shown || !CargoStateShown(shown.value))
    return {};
  result.rewards = WarpRoot(viewer->TryRewardsButton());
  if (!result.rewards)
    return {};
  auto* rewards     = result.rewards.Get<RewardsButtonWidget>();
  result.context    = WarpRoot(rewards->TryCall<"get_Context", BattleTargetData*>());
  result.controller = WarpRoot(rewards->TryRewardsController());
  if (!result.context || !result.controller)
    return {};
  const auto id = result.context.Get<BattleTargetData>()->TryCall<"get_TargetID", int64_t>();
  if (!id)
    return {};
  result.target_id = id.value; // An observed zero is not an unavailable station/target observation.
  return result;
}

bool SameCargoSource(const PendingTargetCargo& expected, const PendingTargetCargo& current)
{
  return expected.viewer.Get() == current.viewer.Get() && expected.rewards.Get() == current.rewards.Get()
         && expected.context.Get() == current.context.Get() && expected.controller.Get() == current.controller.Get()
         && expected.target_id == current.target_id;
}

void RetireTargetCargo()
{
  if (pending_target_cargo.empty())
    return;
  const auto frame = unity::Time::frameCount();
  if (!Config::Get().control.hotkeys_enabled || Config::Get().control.use_scopely_hotkeys
      || Shortcut::CaptureBlocksShortcuts() || unity::Key::IsInputFocused()) {
    CancelTargetCargo();
    return;
  }
  std::erase_if(pending_target_cargo,
                [frame](const auto& request) { return frame < request.frame || frame > request.last_frame; });
}

void ScheduleTargetCargo(PendingTargetCargo request, bool automatic, int32_t passes)
{
  const auto frame = unity::Time::frameCount();
  if (frame < 0 || passes < 1 || frame > std::numeric_limits<int32_t>::max() - passes)
    return;
  CancelTargetCargoForWidget(request.rewards.Get<RewardsButtonWidget>());
  request.frame            = frame;
  request.last_frame       = frame + passes;
  request.remaining_passes = passes;
  request.automatic        = automatic;
  pending_target_cargo.push_back(std::move(request));
}

void ToggleTargetCargo()
{
  CancelTargetCargo();
  for (auto* pre_scan : unity::Instances<PreScanTargetWidget>()) {
    auto current = ObserveTargetCargo(pre_scan);
    if (!current)
      continue;
    const auto state = current->controller.Get<VisibilityController>()->TryState();
    if (!state)
      continue;
    if (CargoStateShown(state.value))
      current->controller.Get<VisibilityController>()->Hide();
    else
      ScheduleTargetCargo(std::move(*current), false, 5);
  }
}

// Whether the target's cargo opens by itself, per the show_*_cargo settings for its kind. Failed field/getter
// reads cannot become a station or hostile classification merely because an unavailable value defaults to null.
bool ShowsCargoByDefault(RewardsButtonWidget* widget)
{
  if (!widget || !Config::Get().ui.show_cargo_default || mining_preview::SuppressAutomaticCargo(widget))
    return false;
  auto owner  = WarpRoot(widget);
  auto target = owner ? WarpRoot(owner.Get<RewardsButtonWidget>()->TryCall<"get_Context", BattleTargetData*>())
                      : il2cpp::GcHandle{};
  if (!target)
    return false;
  const auto observed_deployed = target.Get<BattleTargetData>()->TryTargetFleetData();
  if (!observed_deployed)
    return false;
  if (!observed_deployed.value)
    return Config::Get().ui.show_station_cargo;
  auto       deployed = WarpRoot(observed_deployed);
  const auto type =
      deployed ? deployed.Get<FleetDeployedData>()->TryFleetType() : il2cpp::CallResult<DeployedFleetType>{};
  if (!type)
    return false;
  switch (type.value) {
    case DeployedFleetType::Player:
      return Config::Get().ui.show_player_cargo;
    case DeployedFleetType::Marauder: {
      const auto observed_hull = deployed.Get<FleetDeployedData>()->TryHull();
      if (!observed_hull)
        return false;
      if (!observed_hull.value)
        return Config::Get().ui.show_hostile_cargo;
      auto       hull = WarpRoot(observed_hull);
      const auto hull_type =
          hull ? hull.Get<HullSpec>()->TryCall<"get_Type", HullType>() : il2cpp::CallResult<HullType>{};
      return hull_type
             && (hull_type.value == HullType::ArmadaTarget ? Config::Get().ui.show_armada_cargo
                                                           : Config::Get().ui.show_hostile_cargo);
    }
    default:
      return false;
  }
}

void ShowPendingTargetCargo()
{
  RetireTargetCargo();
  auto       requests   = std::exchange(pending_target_cargo, {});
  const auto generation = cargo_request_generation;
  const auto frame      = unity::Time::frameCount();
  for (auto& request : requests) {
    if (generation != cargo_request_generation)
      break; // A reentrant bind/cancel owns the new request episode.
    auto current = ObserveTargetCargo(request.viewer.Get<PreScanTargetWidget>());
    if (!current || !SameCargoSource(request, *current)
        || (request.automatic && !ShowsCargoByDefault(current->rewards.Get<RewardsButtonWidget>())))
      continue;
    const auto state = current->controller.Get<VisibilityController>()->TryState();
    if (!state)
      continue;
    // Category getters can reenter native code; verify the card/context once more before showing anything.
    auto confirmed = ObserveTargetCargo(request.viewer.Get<PreScanTargetWidget>());
    if (!confirmed || !SameCargoSource(*current, *confirmed) || generation != cargo_request_generation)
      continue;
    if (!CargoStateShown(state.value))
      confirmed->controller.Get<VisibilityController>()->Show(true);
    --request.remaining_passes;
    if (generation == cargo_request_generation && request.remaining_passes > 0 && frame < request.last_frame)
      pending_target_cargo.push_back(std::move(request));
  }
}

void ShowCargoByDefault(RewardsButtonWidget* widget)
{
  auto       owner   = WarpRoot(widget);
  auto       context = owner ? WarpRoot(owner.Get<RewardsButtonWidget>()->TryCall<"get_Context", BattleTargetData*>())
                             : il2cpp::GcHandle{};
  auto       cargo   = owner ? WarpRoot(owner.Get<RewardsButtonWidget>()->TryRewardsController()) : il2cpp::GcHandle{};
  const auto alive   = owner ? unity::TryAlive(owner.Get()) : il2cpp::CallResult<bool>{};
  const auto target_id =
      context ? context.Get<BattleTargetData>()->TryCall<"get_TargetID", int64_t>() : il2cpp::CallResult<int64_t>{};
  if (!owner || !context || !cargo || !alive || !alive.value || !target_id
      || !ShowsCargoByDefault(owner.Get<RewardsButtonWidget>()))
    return;
  // Qualify the post-getter episode; callbacks must not open a rebound widget's new target using old policy.
  auto       current_context = WarpRoot(owner.Get<RewardsButtonWidget>()->TryCall<"get_Context", BattleTargetData*>());
  auto       current_cargo   = WarpRoot(owner.Get<RewardsButtonWidget>()->TryRewardsController());
  const auto current_id = current_context ? current_context.Get<BattleTargetData>()->TryCall<"get_TargetID", int64_t>()
                                          : il2cpp::CallResult<int64_t>{};
  if (!current_context || !current_cargo || current_context.Get() != context.Get() || current_cargo.Get() != cargo.Get()
      || !current_id || current_id.value != target_id.value)
    return;
  cargo.Get<VisibilityController>()->Show(true);
  for (auto* pre_scan : unity::Instances<PreScanTargetWidget>()) {
    auto request = ObserveTargetCargo(pre_scan);
    if (request && request->rewards.Get() == owner.Get() && request->context.Get() == context.Get()
        && request->controller.Get() == cargo.Get())
      ScheduleTargetCargo(std::move(*request), true, 1);
  }
}

void RewardsButtonWidget_OnDidBindContext_Hook(auto original, RewardsButtonWidget* _this)
{
  auto owner = WarpRoot(_this);
  CancelTargetCargoForWidget(_this);
  original(_this);
  if (owner)
    ShowCargoByDefault(owner.Get<RewardsButtonWidget>());
}

void PreScanTargetWidget_ShowWithFleet_Hook(auto original, PreScanTargetWidget* _this, void* fleet)
{
  auto owner = WarpRoot(_this);
  original(_this, fleet);
  if (owner) {
    auto rewards = WarpRoot(owner.Get<PreScanTargetWidget>()->TryRewardsButton());
    if (rewards)
      ShowCargoByDefault(rewards.Get<RewardsButtonWidget>());
  }
}

// --- Queue switch ---

// queue_enabled switches the game's action queue off by telling it the queue is locked.
bool ActionQueueManager_IsQueueUnlocked_Hook(auto original, ActionQueueManager* _this)
{ return Config::Get().control.queue_enabled && original(_this); }

// --- Ability buttons ---

using FleetSelectionClock = std::chrono::steady_clock;

struct FleetSelectionSource {
  il2cpp::GcHandle bar, context, slots, fleet;
  int32_t          index = -1;
  int64_t          id    = 0;
};
struct FleetSelectionRepeat {
  il2cpp::GcHandle                bar, context, slots, fleet;
  FleetSelectionClock::time_point when{};
  int32_t                         index = -1;
  int64_t                         id    = 0;
};
// Only weak identities cross frames. A replaced bar, context, array, or fleet must receive an ordinary selection
// before another press can locate it. Process-lifetime storage avoids managed cleanup after runtime shutdown.
auto& fleet_selection_repeat = *new std::optional<FleetSelectionRepeat>;

bool FleetSelectionInputAvailable()
{
  return Config::Get().control.hotkeys_enabled && !Config::Get().control.use_scopely_hotkeys
         && !Shortcut::CaptureBlocksShortcuts() && !unity::Key::IsInputFocused();
}

std::optional<FleetSelectionSource> ObserveFleetSelection(FleetBarViewController* bar, int32_t index)
{
  FleetSelectionSource result;
  result.bar = WarpRoot(bar);
  if (!result.bar || bar != unity::Newest<FleetBarViewController>() || index < 0)
    return {};
  result.context = WarpRoot(bar->TryCall<"get_CanvasContext", FleetBarContext*>());
  if (!result.context)
    return {};
  const auto observed = result.context.Get<FleetBarContext>()->TryCall<"get_FleetData", il2cpp::ArrayObject*>();
  result.slots        = observed ? il2cpp::GcHandle::Strong(observed.value) : il2cpp::GcHandle{};
  const auto slots    = result.slots ? il2cpp::TryReferenceArray<FleetPlayerData*>(result.slots.Get())
                                     : il2cpp::CallResult<il2cpp::Array<FleetPlayerData*>*>{};
  if (!slots || !slots.value || static_cast<size_t>(index) >= slots.value->size())
    return {};
  result.fleet  = WarpRoot((*slots.value)[index]);
  const auto id = result.fleet ? result.fleet.Get<FleetPlayerData>()->TryId() : il2cpp::CallResult<int64_t>{};
  const auto current_context = bar->TryCall<"get_CanvasContext", FleetBarContext*>();
  if (!id || id.value <= 0 || bar != unity::Newest<FleetBarViewController>() || !current_context
      || current_context.value != result.context.Get<FleetBarContext>()
      || (*slots.value)[index] != result.fleet.Get<FleetPlayerData>())
    return {};
  result.index = index;
  result.id    = id.value;
  return result;
}

bool SameFleetSelection(const FleetSelectionSource& expected, const FleetSelectionSource& current)
{
  return expected.bar.Get() == current.bar.Get() && expected.context.Get() == current.context.Get()
         && expected.slots.Get() == current.slots.Get() && expected.fleet.Get() == current.fleet.Get()
         && expected.index == current.index && expected.id == current.id;
}

bool RepeatedFleetSelection(const FleetSelectionSource& current, FleetSelectionClock::time_point now)
{
  if (!fleet_selection_repeat || now < fleet_selection_repeat->when
      || now - fleet_selection_repeat->when >= std::chrono::milliseconds(Config::Get().control.select_timer))
    return false;
  const auto& previous = *fleet_selection_repeat;
  return previous.index == current.index && previous.id == current.id && previous.bar.Get() == current.bar.Get()
         && previous.context.Get() == current.context.Get() && previous.slots.Get() == current.slots.Get()
         && previous.fleet.Get() == current.fleet.Get();
}

void RememberFleetSelection(const FleetSelectionSource& source, FleetSelectionClock::time_point now)
{
  FleetSelectionRepeat next{il2cpp::GcHandle::Weak(source.bar.Get()),
                            il2cpp::GcHandle::Weak(source.context.Get()),
                            il2cpp::GcHandle::Weak(source.slots.Get()),
                            il2cpp::GcHandle::Weak(source.fleet.Get()),
                            now,
                            source.index,
                            source.id};
  fleet_selection_repeat.reset();
  if (next.bar && next.context && next.slots && next.fleet)
    fleet_selection_repeat = std::move(next);
}

void RetireFleetSelection()
{
  if (!fleet_selection_repeat)
    return;
  const auto& source  = *fleet_selection_repeat;
  const auto  now     = FleetSelectionClock::now();
  const bool  expired = now < source.when
                        || now - source.when >= std::chrono::milliseconds(Config::Get().control.select_timer)
                        || !source.bar.Get() || !source.context.Get() || !source.slots.Get() || !source.fleet.Get();
  if (expired || !FleetSelectionInputAvailable())
    fleet_selection_repeat.reset();
}

bool LocateFleetSelection(FleetBarViewController* bar, const FleetSelectionSource* slot = nullptr)
{
  auto       bar_root = WarpRoot(bar);
  auto       panel    = bar_root ? WarpRoot(bar->TryPanel()) : il2cpp::GcHandle{};
  auto       fleet = panel ? WarpRoot(panel.Get<FleetLocalViewController>()->TryCall<"get_fleet", FleetPlayerData*>())
                           : il2cpp::GcHandle{};
  const auto id    = fleet ? fleet.Get<FleetPlayerData>()->TryId() : il2cpp::CallResult<int64_t>{};
  if (!id || id.value <= 0 || (slot && (slot->fleet.Get() != fleet.Get() || slot->id != id.value)))
    return false;
  auto manager = WarpRoot(FleetsManager::Instance());
  if (!manager)
    return false;
  NavigationSectionManager::HideInteraction();
  // Hiding a target card and obtaining the manager may synchronously replace the fleet bar or selected fleet.
  auto current_panel = WarpRoot(bar->TryPanel());
  auto current_fleet =
      current_panel ? WarpRoot(current_panel.Get<FleetLocalViewController>()->TryCall<"get_fleet", FleetPlayerData*>())
                    : il2cpp::GcHandle{};
  const auto current_id = current_fleet ? current_fleet.Get<FleetPlayerData>()->TryId() : il2cpp::CallResult<int64_t>{};
  if (bar != unity::Newest<FleetBarViewController>() || current_panel.Get() != panel.Get()
      || current_fleet.Get() != fleet.Get() || !current_id || current_id.value != id.value
      || !FleetSelectionInputAvailable())
    return false;
  if (slot) {
    const auto current_slot = ObserveFleetSelection(bar, slot->index);
    if (!current_slot || !SameFleetSelection(*slot, *current_slot))
      return false;
  }
  return manager.Get<FleetsManager>()
      ->TryCall<"RequestViewFleet", void(FleetPlayerData*, bool)>(fleet.Get<FleetPlayerData>(), true)
      .succeeded;
}

// The game can leave an ability's button disabled after its cooldown ends; this enables it again once the ability is
// usable. Four checks a second are plenty for a button.
void RecoverAbilityButtons()
{
  if (Shortcut::CaptureBlocksShortcuts())
    return;
  auto bar        = WarpRoot(unity::Newest<FleetBarViewController>());
  auto controller = bar ? WarpRoot(bar.Get<FleetBarViewController>()->TryPanel()) : il2cpp::GcHandle{};
  auto fleet      = controller
                        ? WarpRoot(controller.Get<FleetLocalViewController>()->TryCall<"get_fleet", FleetPlayerData*>())
                        : il2cpp::GcHandle{};
  const auto id   = fleet ? fleet.Get<FleetPlayerData>()->TryId() : il2cpp::CallResult<int64_t>{};
  const auto state = fleet ? fleet.Get<FleetPlayerData>()->TryCall<"get_CurrentState", FleetState>()
                           : il2cpp::CallResult<FleetState>{};
  if (!id || id.value <= 0 || !state
      || (std::to_underlying(state.value) & std::to_underlying(FleetState::CanActivateAbility)) == 0)
    return;
  auto manager = WarpRoot(ActivatedAbilityManager::Instance());
  auto panel   = WarpRoot(controller.Get<FleetLocalViewController>()->TryActionPanel());
  auto canvas =
      panel ? WarpRoot(
                  panel.Get<ShipActionPanelLocalViewController>()->TryCall<"GetComponentInParent", CanvasController*>(
                      CanvasController::Class::TypeObject()))
            : il2cpp::GcHandle{};
  const auto visible =
      canvas ? canvas.Get<CanvasController>()->TryCall<"get_Visible", bool>() : il2cpp::CallResult<bool>{};
  const auto effect =
      panel ? panel.Get<ShipActionPanelLocalViewController>()->TryCall<"HasShipAnActiveAbilityWithActiveEffect", bool>()
            : il2cpp::CallResult<bool>{};
  if (!manager || !visible || !visible.value || !effect || effect.value)
    return;
  // The panel's ability ids and buttons are parallel arrays.
  const auto ids          = panel.Get<ShipActionPanelLocalViewController>()->TryAbilityIds();
  const auto buttons      = panel.Get<ShipActionPanelLocalViewController>()->TryButtons();
  auto       ids_root     = ids ? il2cpp::GcHandle::Strong(ids.value) : il2cpp::GcHandle{};
  auto       buttons_root = buttons ? il2cpp::GcHandle::Strong(buttons.value) : il2cpp::GcHandle{};
  if (!ids_root || !buttons_root)
    return;
  for (size_t index = 0; index < std::min(ids.value->size(), buttons.value->size()); ++index) {
    const auto ability_id = (*ids.value)[index];
    auto       button     = WarpRoot((*buttons.value)[index]);
    auto context = button ? WarpRoot(button.Get<GenericButtonWidget>()->TryCall<"get_Context", GenericButtonContext*>())
                          : il2cpp::GcHandle{};
    const auto active = button ? button.Get<GenericButtonWidget>()->TryCall<"get_isActiveAndEnabled", bool>()
                               : il2cpp::CallResult<bool>{};
    const auto interactable =
        context ? context.Get<GenericButtonContext>()->TryCall<"get_Interactable", bool>() : il2cpp::CallResult<bool>{};
    if (!context || !active || !active.value || !interactable || interactable.value)
      continue;
    const auto observed_timer = panel.Get<ShipActionPanelLocalViewController>()->TryRemainingCooldown(ability_id);
    auto       timer          = observed_timer ? il2cpp::GcHandle::Strong(observed_timer.value) : il2cpp::GcHandle{};
    const auto remaining =
        timer ? timer.Get<TimerDataContext>()->TryRemainingTime() : il2cpp::CallResult<unity::TimeSpan>{};
    // TimeSpan is an eight-byte value in 100 ns ticks. Repair at -1 second or below.
    if (!remaining || remaining.value.ticks > -10'000'000)
      continue;
    const auto ready =
        manager.Get<ActivatedAbilityManager>()->TryCall<"CanUseAbility", bool(FleetPlayerData*, int64_t)>(
            fleet.Get<FleetPlayerData>(), ability_id);
    if (!ready || !ready.value)
      continue;
    // No action is authorized by the old panel after native availability/cooldown calls rebind it or change its data.
    const auto observed_current_timer =
        panel.Get<ShipActionPanelLocalViewController>()->TryRemainingCooldown(ability_id);
    auto current_timer =
        observed_current_timer ? il2cpp::GcHandle::Strong(observed_current_timer.value) : il2cpp::GcHandle{};
    const auto current_remaining = current_timer ? current_timer.Get<TimerDataContext>()->TryRemainingTime()
                                                 : il2cpp::CallResult<unity::TimeSpan>{};
    const auto current_effect =
        panel.Get<ShipActionPanelLocalViewController>()->TryCall<"HasShipAnActiveAbilityWithActiveEffect", bool>();
    const auto current_visible      = canvas.Get<CanvasController>()->TryCall<"get_Visible", bool>();
    const auto current_active       = button.Get<GenericButtonWidget>()->TryCall<"get_isActiveAndEnabled", bool>();
    const auto current_interactable = context.Get<GenericButtonContext>()->TryCall<"get_Interactable", bool>();
    auto       current_controller   = WarpRoot(bar.Get<FleetBarViewController>()->TryPanel());
    auto       current_fleet =
        current_controller
            ? WarpRoot(current_controller.Get<FleetLocalViewController>()->TryCall<"get_fleet", FleetPlayerData*>())
            : il2cpp::GcHandle{};
    const auto current_id =
        current_fleet ? current_fleet.Get<FleetPlayerData>()->TryId() : il2cpp::CallResult<int64_t>{};
    const auto current_state   = current_fleet
                                     ? current_fleet.Get<FleetPlayerData>()->TryCall<"get_CurrentState", FleetState>()
                                     : il2cpp::CallResult<FleetState>{};
    auto       current_panel   = current_controller
                                     ? WarpRoot(current_controller.Get<FleetLocalViewController>()->TryActionPanel())
                                     : il2cpp::GcHandle{};
    const auto current_ids     = panel.Get<ShipActionPanelLocalViewController>()->TryAbilityIds();
    const auto current_buttons = panel.Get<ShipActionPanelLocalViewController>()->TryButtons();
    const auto current_context = button.Get<GenericButtonWidget>()->TryCall<"get_Context", GenericButtonContext*>();
    if (!current_remaining || current_remaining.value.ticks > -10'000'000 || !current_effect || current_effect.value
        || !current_visible || !current_visible.value || !current_active || !current_active.value
        || !current_interactable || current_interactable.value || Shortcut::CaptureBlocksShortcuts()
        || bar.Get() != unity::Newest<FleetBarViewController>() || current_controller.Get() != controller.Get()
        || current_fleet.Get() != fleet.Get() || !current_id || current_id.value != id.value || !current_state
        || (std::to_underlying(current_state.value) & std::to_underlying(FleetState::CanActivateAbility)) == 0
        || current_panel.Get() != panel.Get() || !current_ids || current_ids.value != ids.value || !current_buttons
        || current_buttons.value != buttons.value || (*ids.value)[index] != ability_id
        || (*buttons.value)[index] != button.Get<GenericButtonWidget>() || !current_context
        || current_context.value != context.Get<GenericButtonContext>())
      continue;
    (void)context.Get<GenericButtonContext>()->TryCall<"set_Interactable", void(bool)>(true);
  }
}

void FleetActionMaintenanceFrame()
{
  RetireFleetReplay(); // Also runs in capture/native-hotkey/disabled modes which skip ordinary shortcut dispatch.
  RetireTargetCargo();
  DiscoveryTowFrame(); // Runs in every input mode so disable/capture can retire pending native work.
  RetireFleetSelection();
}

void AbilityButtonsFrame()
{
  static Throttle throttle(std::chrono::milliseconds(250));
  if (throttle.Due())
    RecoverAbilityButtons();
}

bool InstallQueueSwitch()
{
  il2cpp::HookSet hooks("QueueSwitch");
  IL2CPP_STAGE(hooks, ActionQueueManager::Class::Method("IsQueueUnlocked"), ActionQueueManager_IsQueueUnlocked_Hook);
  return hooks.Commit();
}
} // namespace

bool TargetViewerShown()
{ return TargetViewers::AnyShown(); }

bool SelectShipShortcutsAt(FleetSelectionClock::time_point now)
{
  if (!FleetSelectionInputAvailable()) {
    fleet_selection_repeat.reset();
    return false;
  }
  // The number keys select a ship in the fleet bar; pressing the same one again within select_timer flies the
  // camera to it. With disable_preview_locate, the second press only selects while a target card is open.
  constexpr std::array kShips{Action::kSelectShip1, Action::kSelectShip2, Action::kSelectShip3, Action::kSelectShip4,
                              Action::kSelectShip5, Action::kSelectShip6, Action::kSelectShip7, Action::kSelectShip8};
  const bool tow  = unity::Key::Pressed(unity::KeyCode::LeftShift) || unity::Key::Pressed(unity::KeyCode::RightShift);
  const auto ship = std::ranges::find_if(
      kShips, [tow](Action action) { return tow ? Shortcut::IsDownWithShift(action) : Shortcut::IsDown(action); });
  if (ship == kShips.end())
    return false;
  const auto index = static_cast<int32_t>(ship - kShips.begin());

  auto fleet_bar = WarpRoot(unity::Newest<FleetBarViewController>());
  if (!fleet_bar) {
    fleet_selection_repeat.reset();
    CancelDiscoveryTow();
    return false;
  }

  if (tow) {
    fleet_selection_repeat.reset();
    (void)BeginDiscoveryTow(fleet_bar.Get<FleetBarViewController>(), index);
    return true; // Shift+ship is the tow request, including an unavailable native contract.
  }
  CancelDiscoveryTow(); // Ordinary ship selection must not leave a previous tow pending.

  const bool can_locate = !Config::Get().ui.disable_preview_locate || !TargetViewerShown();
  auto       source     = ObserveFleetSelection(fleet_bar.Get<FleetBarViewController>(), index);
  const auto selected = can_locate && source && RepeatedFleetSelection(*source, now)
                            ? fleet_bar.Get<FleetBarViewController>()->TryCall<"IsIndexSelected", bool(int32_t)>(index)
                            : il2cpp::CallResult<bool>{};
  if (selected && selected.value) {
    if (!LocateFleetSelection(fleet_bar.Get<FleetBarViewController>(), &*source)) {
      fleet_selection_repeat.reset();
      return true; // Consume this locate without redirecting it to a replacement source.
    }
  } else {
    if (fleet_bar.Get() != unity::Newest<FleetBarViewController>()
        || !fleet_bar.Get<FleetBarViewController>()->TryCall<"RequestSelect", void(int32_t, bool)>(index, false)) {
      fleet_selection_repeat.reset();
      return true;
    }
  }
  source = ObserveFleetSelection(fleet_bar.Get<FleetBarViewController>(), index);
  if (source)
    RememberFleetSelection(*source, now);
  else
    fleet_selection_repeat.reset();
  return true;
}

bool SelectShipShortcuts()
{ return SelectShipShortcutsAt(FleetSelectionClock::now()); }

bool FleetShortcuts()
{
  if (Shortcut::IsDown(Action::kSelectCurrent) && FleetSelectionInputAvailable()) {
    if (LocateFleetSelection(unity::Newest<FleetBarViewController>()))
      return true;
  }
  if (Shortcut::IsDown(Action::kToggleQueue)) {
    auto& queue_enabled = Config::Get().control.queue_enabled;
    gameplay_settings::ToggleBoolean(queue_enabled, gameplay_settings::Boolean::Queue,
                                     [](const gameplay_settings::Key& key, bool value) {
                                       runtime_config::SaveSetting(key.section, key.name, value, key.delay);
                                     });
    ResetQueueRecoveryTracking();
    return true;
  }
  return false;
}

bool FleetActionShortcuts()
{
  // A replay lives for exactly the next frame, whatever that frame does.
  auto replayed = std::exchange(pending_replay, std::nullopt);
  if (unity::Key::IsInputFocused())
    return false;

  if (unity::Key::Pressed(unity::KeyCode::Escape) && TargetViewers::HideFirst())
    return true;

  // Escape or the primary action dismisses the golden rewards screen. Down, not Pressed: a held Escape would go
  // back a section every frame while the screen closes.
  if (Shortcut::IsDown(Action::kActionPrimary) || unity::Key::Down(unity::KeyCode::Escape)) {
    if (auto* rewards = unity::Newest<AnimatedRewardsScreenViewController>(); rewards && rewards->IsActive()) {
      rewards->GoBackToLastSection();
      return true;
    }
  }

  FleetPress pressed{
      .primary     = Shortcut::IsDown(Action::kActionPrimary),
      .secondary   = Shortcut::IsDown(Action::kActionSecondary),
      .queue       = Shortcut::IsDown(Action::kActionQueue),
      .queue_clear = Shortcut::IsDown(Action::kActionQueueClear),
      // With disable_preview_recall, recall only works while no target card is open.
      .recall =
          Shortcut::IsDown(Action::kActionRecall) && (!Config::Get().ui.disable_preview_recall || !TargetViewerShown()),
      .recall_cancel = Shortcut::IsDown(Action::kActionRecallCancel),
      .repair        = Shortcut::IsDown(Action::kActionRepair),
  };
  if (replayed && !pressed.Any()) {
    pressed       = replayed->press;
    pressed.retry = true;
  } else {
    replayed.reset();
  }
  if (pressed.Any() && Hub::IsInSystemOrGalaxyOrStarbase() && !Hub::IsInChat())
    ActOnSelectedFleet(pressed, replayed ? &*replayed : nullptr);

  if (Shortcut::IsDown(Action::kActionView)) {
    if (mining_preview::Cycle())
      CancelTargetCargo();
    else
      ToggleTargetCargo();
  }
  if (!pending_target_cargo.empty())
    ShowPendingTargetCargo();
  return false;
}

void InstallFleetActions()
{
  il2cpp::HookSet hooks("FleetActions");
  IL2CPP_STAGE(hooks, RewardsButtonWidget::Class::Method("OnDidBindContext"),
               RewardsButtonWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE(hooks, PreScanTargetWidget::Class::Method("ShowWithFleet"), PreScanTargetWidget_ShowWithFleet_Hook);
  hooks.Commit();
}

const Feature kQueueSwitch{"QueueSwitch", &Config::Patches::testpatches, InstallQueueSwitch};
// Keep housekeeping under its original startup gate and frame position, independently of optional hook success.
const Feature kFleetActionMaintenance{"FleetActionMaintenance", &Config::Patches::hotkeyhooks, nullptr,
                                      FleetActionMaintenanceFrame};
const Feature kAbilityButtons{"AbilityButtons", &Config::Patches::abilitybuttonrecoveryhooks, nullptr,
                              AbilityButtonsFrame};
