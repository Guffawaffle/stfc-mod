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
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

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
DiscoveryTowObservation ObserveDiscoveryTow(const std::shared_ptr<DiscoveryTowRun>& run)
{
  DiscoveryTowObservation result;
  auto&                   e = result.evidence;
  e.generation              = DiscoveryTowing().generation;
  e.now_ms                  = TowMilliseconds();
  e.input_mode              = TowInputMode();
  auto* owner               = run->owner.Get<DeploymentManager>();
  e.owner_alive             = TowOwnerAlive(owner);
  auto current_owner =
      WarpRoot(planned_warp_binding::StaticReference<"get_Instance", DeploymentManager, DeploymentManager>());
  auto fleets   = WarpRoot(planned_warp_binding::StaticReference<"get_Instance", FleetsManager, FleetsManager>());
  auto sections = WarpRoot(
      planned_warp_binding::StaticReference<"get_Instance", NavigationSectionManager, NavigationSectionManager>());
  if (!current_owner || !fleets || !sections)
    return result;
  e.owner            = WarpIdentity(current_owner);
  const auto section = sections.Get<NavigationSectionManager>()->TryCall<"get_CurrentSection", SectionID>();
  if (section)
    e.section = std::to_underlying(section.value);
  auto selected  = WarpRoot(fleets.Get<FleetsManager>()->TryCall<"GetFleetPlayerData", FleetPlayerData*(int32_t)>(
      run->request.selected_index));
  auto discovery = WarpRoot(fleets.Get<FleetsManager>()->TryCall<"GetFleetPlayerData", FleetPlayerData*(int32_t)>(
      run->request.discovery_index));
  if (!selected || !discovery || current_owner.Get() != run->owner.Get())
    return result;
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
  const auto current   = ObserveDiscoveryTow(run);
  if (state.generation != ticket || state.run || !TowInputMode() || !TowOwnerAlive(run->owner.Get<DeploymentManager>())
      || current.course.Get() != observed.course.Get()
      || discovery_tow::Evaluate(completed, current.evidence) != discovery_tow::Decision::Submit)
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
  state.run     = run;
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

void ViewSelectedFleet(FleetPlayerData* fleet)
{
  NavigationSectionManager::HideInteraction();
  if (auto* fleets = FleetsManager::Instance())
    fleets->RequestViewFleet(fleet, true);
}

// --- Target cargo ---

// Frames left in which to show the cargo of a pre-scanned target. The rewards widget can appear a few frames
// after the pre-scan itself, so a request keeps trying for a while.
int show_cargo_frames = 0;

// The cargo panel of a pre-scan card that is showing, or null.
VisibilityController* ShownTargetCargo(PreScanTargetWidget* pre_scan)
{
  auto* rewards = pre_scan->rewardsButtonWidget();
  auto* cargo   = rewards ? rewards->rewardsController() : nullptr;
  return cargo && ControllerShown(pre_scan->visibilityController()) ? cargo : nullptr;
}

void ToggleTargetCargo()
{
  for (auto* pre_scan : unity::Instances<PreScanTargetWidget>()) {
    if (auto* cargo = ShownTargetCargo(pre_scan)) {
      if (cargo->IsShown())
        cargo->Hide();
      else
        show_cargo_frames = 5;
    }
  }
}

void ShowPendingTargetCargo()
{
  for (auto* pre_scan : unity::Instances<PreScanTargetWidget>())
    if (auto* cargo = ShownTargetCargo(pre_scan); cargo && !cargo->IsShown())
      cargo->Show(true);
  --show_cargo_frames;
}

// Whether the target's cargo opens by itself, per the show_*_cargo settings for its kind.
bool ShowsCargoByDefault(RewardsButtonWidget* widget)
{
  if (mining_preview::SuppressAutomaticCargo(widget))
    return false;
  const auto& config = Config::Get();
  auto*       target = widget && config.ui.show_cargo_default ? widget->Context() : nullptr;
  if (!target)
    return false;

  auto* deployed = target->TargetFleetDeployedData();
  if (!deployed)
    return config.ui.show_station_cargo;
  switch (deployed->FleetType()) {
    case DeployedFleetType::Player:
      return config.ui.show_player_cargo;
    case DeployedFleetType::Marauder: {
      auto* hull = deployed->Hull();
      return hull && hull->Type() == HullType::ArmadaTarget ? config.ui.show_armada_cargo
                                                            : config.ui.show_hostile_cargo;
    }
    default:
      return false;
  }
}

void ShowCargoByDefault(RewardsButtonWidget* widget)
{
  if (auto* cargo = widget ? widget->rewardsController() : nullptr; cargo && ShowsCargoByDefault(widget)) {
    cargo->Show(true);
    show_cargo_frames = 1;
  }
}

void RewardsButtonWidget_OnDidBindContext_Hook(auto original, RewardsButtonWidget* _this)
{
  original(_this);
  ShowCargoByDefault(_this);
}

void PreScanTargetWidget_ShowWithFleet_Hook(auto original, PreScanTargetWidget* _this, void* fleet)
{
  original(_this, fleet);
  ShowCargoByDefault(_this->rewardsButtonWidget());
}

// --- Queue switch ---

// queue_enabled switches the game's action queue off by telling it the queue is locked.
bool ActionQueueManager_IsQueueUnlocked_Hook(auto original, ActionQueueManager* _this)
{ return Config::Get().control.queue_enabled && original(_this); }

// --- Ability buttons ---

// The game can leave an ability's button disabled after its cooldown ends; this enables it again once the ability is
// usable. Four checks a second are plenty for a button.
void AbilityButtonsFrame()
{
  RetireFleetReplay(); // Also runs in capture/native-hotkey/disabled modes which skip ordinary shortcut dispatch.
  DiscoveryTowFrame(); // Runs in every input mode so disable/capture can retire pending native work.
  static Throttle throttle(std::chrono::milliseconds(250));
  if (!throttle.Due())
    return;
  auto* controller = FleetBarViewController::NewestPanel();
  auto* fleet      = controller ? controller->fleet() : nullptr;
  if (!fleet || (std::to_underlying(fleet->CurrentState()) & std::to_underlying(FleetState::CanActivateAbility)) == 0)
    return;
  auto* manager = ActivatedAbilityManager::Instance();
  auto* panel   = controller->shipActionPanelLocalViewController();
  if (!manager || !panel || !CanvasController::Shows(panel) || panel->HasShipAnActiveAbilityWithActiveEffect())
    return;
  // The panel's ability ids and buttons are parallel arrays.
  auto* ids     = panel->abilityIDs();
  auto* buttons = panel->buttons();
  for (const auto [ability_id, button] : std::views::zip(il2cpp::Items(ids), il2cpp::Items(buttons))) {
    auto* context = button ? button->Context() : nullptr;
    if (!context || !button->isActiveAndEnabled() || context->Interactable())
      continue;
    auto* timer = panel->GetRemainingCooldown(ability_id);
    // TimeSpan is an eight-byte value in 100 ns ticks. Repair at -1 second or below.
    if (timer && timer->RemainingTime().ticks <= -10'000'000 && manager->CanUseAbility(fleet, ability_id))
      context->SetInteractable(true);
  }
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

bool SelectShipShortcuts()
{
  if (unity::Key::IsInputFocused())
    return false;
  using Clock = std::chrono::steady_clock;
  // The number keys select a ship in the fleet bar; pressing the same one again within select_timer flies the
  // camera to it. With disable_preview_locate, the second press only selects while a target card is open.
  constexpr std::array kShips{Action::kSelectShip1, Action::kSelectShip2, Action::kSelectShip3, Action::kSelectShip4,
                              Action::kSelectShip5, Action::kSelectShip6, Action::kSelectShip7, Action::kSelectShip8};
  static auto          last_press = Clock::now();
  static int32_t       last_index = -1;

  const bool tow  = unity::Key::Pressed(unity::KeyCode::LeftShift) || unity::Key::Pressed(unity::KeyCode::RightShift);
  const auto ship = std::ranges::find_if(
      kShips, [tow](Action action) { return tow ? Shortcut::IsDownWithShift(action) : Shortcut::IsDown(action); });
  if (ship == kShips.end())
    return false;
  const auto index = static_cast<int32_t>(ship - kShips.begin());

  auto* fleet_bar = unity::Newest<FleetBarViewController>();
  if (!fleet_bar) {
    CancelDiscoveryTow();
    return false;
  }

  if (tow) {
    (void)BeginDiscoveryTow(fleet_bar, index);
    return true; // Shift+ship is the tow request, including an unavailable native contract.
  }
  CancelDiscoveryTow(); // Ordinary ship selection must not leave a previous tow pending.

  const auto now        = Clock::now();
  const bool can_locate = !Config::Get().ui.disable_preview_locate || !TargetViewerShown();
  if (can_locate && index == last_index && fleet_bar->IsIndexSelected(index)
      && now - last_press < std::chrono::milliseconds(Config::Get().control.select_timer)) {
    auto* fleet = fleet_bar->SelectedFleet();
    if (!fleet)
      return true;
    ViewSelectedFleet(fleet);
  } else {
    fleet_bar->RequestSelect(index);
  }
  last_index = index;
  last_press = now;
  return true;
}

bool FleetShortcuts()
{
  if (Shortcut::IsDown(Action::kSelectCurrent)) {
    auto* panel = FleetBarViewController::NewestPanel();
    if (auto* fleet = panel ? panel->fleet() : nullptr) {
      ViewSelectedFleet(fleet);
      return true;
    }
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
      show_cargo_frames = 0;
    else
      ToggleTargetCargo();
  }
  if (show_cargo_frames > 0)
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
// Frame work only, installed with the hotkeys it was introduced with.
const Feature kAbilityButtons{"AbilityButtons", &Config::Patches::hotkeyhooks, nullptr, AbilityButtonsFrame};
