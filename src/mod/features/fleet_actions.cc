// The selected fleet and the target viewers in space: selecting ships with the number keys, engaging, scanning,
// mining, queueing, warping, recalling and repairing from the keyboard, dismissing viewers with Escape, opening a
// target's cargo when it is scanned, the action queue switch, and re-enabling ability buttons whose cooldown ended.

#include "config.h"
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

// The target's hull type is only known once the target's data has arrived; until then it reads as Any. A press
// that lands in that window is replayed, as itself, on the next frame only.
std::optional<FleetPress> pending_replay;

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

// Requests `type` on the fleet when it is in one of `states`. With `help_state`, a fleet in that state (or one the
// action was just requested for) also asks its alliance for help.
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
    requested = panel->RequestAction(fleet, type, 0, ActionBehaviour::AskHelp) || requested;
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
        pending_replay = pressed;
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
      pending_replay = pressed;
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
  if (!root || !root.Get() || !root.Get<il2cpp::Object>()->Is<T>())
    return {};
  return root;
}
template <class T> il2cpp::GcHandle WarpRoot(T* value)
{ return WarpRoot(il2cpp::CallResult<T*>{true, value}); }
planned_system_warp::Vec3 WarpPosition(unity::Vector3 position)
{ return {position.x, position.y, position.z}; }
std::uintptr_t WarpIdentity(const il2cpp::GcHandle& root)
{ return reinterpret_cast<std::uintptr_t>(root.Get()); }
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
    const auto fleet_id = fleet_root.Get<FleetPlayerData>()->TryId();
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
    auto fleet_address = WarpRoot(fleet_root.Get<FleetPlayerData>()->TryAddress());
    if (!path || !target || !start || !fleet_address)
      return false;
    auto target_address = WarpRoot(target.Get<CourseNode>()->TryAddress());
    auto start_address  = WarpRoot(start.Get<CourseNode>()->TryAddress());
    auto context_path   = WarpRoot(context.Get<NavigationInteractionUIContext>()->TryPath());
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
    const auto retained_position = context.Get<NavigationInteractionUIContext>()->TryPosition();
    if (destination)
      evidence.system_position = WarpPosition(destination.value);
    if (galaxy_position)
      evidence.target_galaxy_position = WarpPosition(galaxy_position.value);
    if (retained_position) {
      const auto projected = animator.Get<NavigationTransitionAnimator>()->TryWorldToDepth(retained_position.value, 1);
      if (projected)
        evidence.projected_context_position = WarpPosition(projected.value);
    }
    const auto observed_tree = context.Get<NavigationInteractionUIContext>()->TryTreeNode();
    auto       tree          = WarpRoot(observed_tree);
    if (observed_tree && !observed_tree.value)
      evidence.tree_node = 0;
    else if (tree) {
      evidence.tree_node = WarpIdentity(tree);
      evidence.tree_id   = WarpObservation(tree.Get<ITreeNode>()->TryId());
      evidence.tree_type = WarpObservation(tree.Get<ITreeNode>()->TryType());
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
    auto       retained_path    = WarpRoot(current_context.Get<NavigationInteractionUIContext>()->TryPath());
    const auto current_position = current_context.Get<NavigationInteractionUIContext>()->TryPosition();
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

void ActOnSelectedFleet(const FleetPress& pressed)
{
  auto* panel = FleetBarViewController::NewestPanel();
  auto* fleet = panel ? panel->fleet() : nullptr;
  if (!fleet)
    return;
  auto* queue = ActionQueueManager::Instance();
  if (!queue)
    return;

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
  using Clock = std::chrono::steady_clock;
  // The number keys select a ship in the fleet bar; pressing the same one again within select_timer flies the
  // camera to it. With disable_preview_locate, the second press only selects while a target card is open.
  constexpr std::array kShips{Action::kSelectShip1, Action::kSelectShip2, Action::kSelectShip3, Action::kSelectShip4,
                              Action::kSelectShip5, Action::kSelectShip6, Action::kSelectShip7, Action::kSelectShip8};
  static auto          last_press = Clock::now();
  static int32_t       last_index = -1;

  const auto ship = std::ranges::find_if(kShips, [](Action action) { return Shortcut::IsDown(action); });
  // TODO(tashcan): Shift+ship used to request a tow by the Discovery. Its bindings had drifted, so it never ran on
  // recent clients, and it was disabled until someone verifies it in game. Shift+ship does nothing meanwhile.
  if (ship == kShips.end() || unity::Key::IsInputFocused())
    return false;
  const auto index = static_cast<int32_t>(ship - kShips.begin());

  auto* fleet_bar = unity::Newest<FleetBarViewController>();
  if (!fleet_bar)
    return false;

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
  const auto replayed = std::exchange(pending_replay, std::nullopt);
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
    pressed       = *replayed;
    pressed.retry = true;
  }
  if (pressed.Any() && Hub::IsInSystemOrGalaxyOrStarbase() && !Hub::IsInChat())
    ActOnSelectedFleet(pressed);

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
