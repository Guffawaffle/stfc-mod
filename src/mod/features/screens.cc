// Shortcuts that open game screens, press the buttons on them, and step through them: the show_* navigation
// shortcuts, Ship Swap and the officer swap and preset buttons, the arrow keys on docks, artifacts and selectors, and
// focusing the search box.

#include "config.h"
#include "features/features.h"
#include "shortcuts.h"

#include "game/app.h"
#include "game/fleet.h"
#include "game/inventory.h"
#include "game/navigation.h"
#include "game/officers.h"
#include "game/ui.h"

#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/tracker.h>

#include <algorithm>
#include <memory>
#include <optional>

namespace
{
// --- Opening screens ---

// Runs one of the game's own shortcut handlers. A handler missing from this client is reported once by the binding
// layer and does nothing.
template <il2cpp::Name Handler> void Native()
{
  if (auto* manager = ShortcutsManager::Instance())
    manager->Run<Handler>();
  else
    spdlog::warn("[Hotkeys] native {} shortcut is unavailable", Handler.View());
}

template <SectionID Section> void Goto()
{ Hub::GotoSection(Section); }

// Galaxy and system views keep their state in section storage; reuse it so the camera returns where it was.
template <SectionID Section> void GotoNavigation()
{
  auto* manager = Hub::Sections();
  auto* storage = manager ? manager->sectionStorage() : nullptr;
  if (auto* state = storage ? storage->GetState(Section) : nullptr)
    Hub::GotoSection(Section, state);
  else
    NavigationSectionManager::ChangeNavigationSection(Section);
}

void ShowBookmarks()
{
  if (auto* bookmarks = BookmarksManager::Instance())
    bookmarks->ViewBookmarks();
  else
    Hub::GotoSection(SectionID::Bookmarks_Main);
}

void ShowCoordinateSearch()
{
  if (auto* bookmarks = BookmarksManager::Instance()) {
    bookmarks->ViewCoordinateSearch();
  } else {
    spdlog::warn("[Hotkeys] no bookmarks manager for the coordinate search; opening bookmarks instead");
    Hub::GotoSection(SectionID::Bookmarks_Main);
  }
}

// --- Swap and preset buttons ---

// Ship Swap and Officer Swap live on the Manage Ships screen, and Officer Presets on the officer assignment screen
// that Officer Swap opens. Their shortcuts open those screens first, then press the button once it exists. 300
// frames is a few seconds at the game's frame rate.
constexpr int kButtonWaitFrames = 300;

// kOfficerAssignment waits on Manage Ships for Officer Swap, on the way to Officer Presets.
enum class ButtonStage { kShipSwap, kOfficerSwap, kOfficerAssignment, kOfficerPresets };

struct ButtonWait {
  ButtonStage      stage;
  int              frames              = kButtonWaitFrames;
  SectionID        origin              = SectionID::AppInit;
  bool             entered_destination = false, entered_assignment = false;
  int64_t          fleet_id = 0;
  uint64_t         revision = 0;
  il2cpp::GcHandle sections, bar, panel, fleet;
};
// Reset explicitly on the game thread. A local shared request survives synchronous native cancellation/replacement;
// process teardown must not invoke GC APIs after IL2CPP has stopped.
auto&    pending_button          = *new std::shared_ptr<ButtonWait>;
uint64_t button_request_revision = 0;

bool ScreenActionInputAvailable()
{
  const auto& control = Config::Get().control;
  return control.hotkeys_enabled && !control.use_scopely_hotkeys && !Shortcut::CaptureBlocksShortcuts()
         && !unity::Key::IsInputFocused() && !unity::Key::Pressed(unity::KeyCode::Escape);
}

bool AwaitedSourceCurrent(const std::shared_ptr<ButtonWait>& request)
{
  if (!request || pending_button != request || request->revision != button_request_revision
      || !ScreenActionInputAvailable())
    return false;
  const auto sections = Hub::Class::TryCall<"get_SectionManager", SectionManager*>();
  if (!sections || sections.value != request->sections.Get())
    return false;
  const auto current = sections.value->TryCall<"get_CurrentSection", SectionID>();
  if (!current)
    return false;
  const bool assignment =
      current.value == SectionID::Officer_Assignment
      && (request->stage == ButtonStage::kOfficerAssignment || request->stage == ButtonStage::kOfficerPresets);
  const bool management = current.value == SectionID::ShipManagement_Details && !request->entered_assignment;
  if (assignment)
    request->entered_assignment = true;
  if (assignment || management)
    request->entered_destination = true;
  else if (request->entered_destination || current.value != request->origin)
    return false;
  if (request->bar) {
    auto* bar = unity::Newest<FleetBarViewController>();
    if (bar != request->bar.Get()
        || !FleetBarViewController::Class::HasField<"_fleetPanelController", FleetLocalViewController*>()
        || bar->fleetPanelController() != request->panel.Get())
      return false;
    const auto fleet = request->panel.Get<FleetLocalViewController>()->TryCall<"get_fleet", FleetPlayerData*>();
    if (!fleet || fleet.value != request->fleet.Get())
      return false;
    const auto id = fleet.value->TryId();
    if (!id || id.value != request->fleet_id)
      return false;
  }
  // All managed observations may synchronously cancel or replace this request.
  return pending_button == request && request->revision == button_request_revision && ScreenActionInputAvailable();
}

std::shared_ptr<ButtonWait> AwaitButton(ButtonStage stage)
{
  const auto revision = button_request_revision;
  const auto sections = Hub::Class::TryCall<"get_SectionManager", SectionManager*>();
  auto       root     = sections ? il2cpp::GcHandle::Strong(sections.value) : il2cpp::GcHandle{};
  const auto current =
      root ? root.Get<SectionManager>()->TryCall<"get_CurrentSection", SectionID>() : il2cpp::CallResult<SectionID>{};
  if (!current || revision != button_request_revision || !ScreenActionInputAvailable())
    return {};
  auto request      = std::make_shared<ButtonWait>();
  request->stage    = stage;
  request->origin   = current.value;
  request->sections = std::move(root);
  request->revision = ++button_request_revision;
  pending_button    = request;
  return request;
}

void CompleteAwaitedButton(const std::shared_ptr<ButtonWait>& request)
{
  if (pending_button == request) {
    pending_button.reset();
    ++button_request_revision;
  }
}

// Opens Manage Ships for the selected fleet. A deferred request owns the exact bar/panel/fleet before the native
// request, so a later selection or pooled replacement cannot redirect its delayed button press.
bool OpenManageShips(const std::shared_ptr<ButtonWait>& request = {})
{
  auto bar = il2cpp::GcHandle::Strong(unity::Newest<FleetBarViewController>());
  if (!bar || !FleetBarViewController::Class::HasField<"_fleetPanelController", FleetLocalViewController*>())
    return false;
  auto       panel = il2cpp::GcHandle::Strong(bar.Get<FleetBarViewController>()->fleetPanelController());
  const auto read  = panel ? panel.Get<FleetLocalViewController>()->TryCall<"get_fleet", FleetPlayerData*>()
                           : il2cpp::CallResult<FleetPlayerData*>{};
  auto       fleet = read ? il2cpp::GcHandle::Strong(read.value) : il2cpp::GcHandle{};
  if (!fleet)
    return false;
  if (request) {
    const auto id = fleet.Get<FleetPlayerData>()->TryId();
    if (!id || id.value <= 0 || pending_button != request)
      return false;
    request->fleet_id = id.value;
    request->bar      = std::move(bar);
    request->panel    = std::move(panel);
    request->fleet    = std::move(fleet);
    if (!AwaitedSourceCurrent(request))
      return false;
  }
  auto*      owner  = request ? request->panel.Get<FleetLocalViewController>() : panel.Get<FleetLocalViewController>();
  auto*      target = request ? request->fleet.Get<FleetPlayerData>() : fleet.Get<FleetPlayerData>();
  const auto accepted =
      owner->TryCall<"RequestAction", bool(il2cpp::Object*, ActionType, int64_t, ActionBehaviour, void*)>(
          target, ActionType::Manage, 0, ActionBehaviour::Default, nullptr);
  return accepted && accepted.value;
}

// Presses the button `pick` returns on the first visible, active Controller.
template <typename Controller>
bool PressButton(unity::Button* (*pick)(Controller*), const std::shared_ptr<ButtonWait>& request = {})
{
  const auto revision = button_request_revision;
  for (auto* controller : unity::Instances<Controller>()) {
    if (revision != button_request_revision)
      return false;
    auto       owner = il2cpp::GcHandle::Strong(controller);
    const bool shown = owner && CanvasController::OnScreen(controller);
    if (revision != button_request_revision)
      return false;
    if (!shown)
      continue;
    auto       button  = il2cpp::GcHandle::Strong(pick(controller));
    const bool current = button && CanvasController::OnScreen(controller) && pick(controller) == button.Get()
                         && ScreenActionInputAvailable() && (!request || AwaitedSourceCurrent(request));
    // Immediate callers have no request yet, but native visibility/getter callbacks can still create or cancel a
    // newer one. Stop this whole source episode instead of continuing into another pooled screen or old fallback.
    if (revision != button_request_revision)
      return false;
    if (!current)
      continue;
    // Success means the actual native Press invocation completed, not merely that the field was nonnull.
    return button.template Get<unity::Button>()->template TryCall<"Press", void()>().succeeded;
  }
  return false;
}

bool PressShipSwapButton(const std::shared_ptr<ButtonWait>& request = {})
{
  return PressButton<ShipManagementScreenStateViewController>(
      [](ShipManagementScreenStateViewController* controller) { return controller->swapShipButton(); }, request);
}

bool PressOfficerSwapButton(const std::shared_ptr<ButtonWait>& request = {})
{
  return PressButton<ShipManagementScreenStateViewController>(
      [](ShipManagementScreenStateViewController* controller) { return controller->officerIconButton(); }, request);
}

bool PressOfficerPresetsButton(const std::shared_ptr<ButtonWait>& request = {})
{
  return PressButton<OfficerAssignmentViewController>(
      [](OfficerAssignmentViewController* controller) -> unity::Button* {
        auto* widget = controller->officerPresetsButton();
        return widget ? widget->UnityButton() : nullptr;
      },
      request);
}

// Opens Manage Ships for a button that only it has. False, with a warning, when no fleet is selected.
bool OpenManageShipsFor(const char* what, const std::shared_ptr<ButtonWait>& request)
{
  if (OpenManageShips(request))
    return true;
  spdlog::warn("[Hotkeys] no fleet is selected, so Manage Ships cannot open for {}", what);
  return false;
}

void ShowShipSwap()
{
  const auto revision = button_request_revision;
  if (PressShipSwapButton() || revision != button_request_revision)
    return;
  if (auto request = AwaitButton(ButtonStage::kShipSwap); request && !OpenManageShipsFor("Ship Swap", request))
    CompleteAwaitedButton(request);
}

void ShowOfficerSwap()
{
  const auto revision = button_request_revision;
  if (PressOfficerSwapButton() || revision != button_request_revision)
    return;
  if (auto request = AwaitButton(ButtonStage::kOfficerSwap); request && !OpenManageShipsFor("Officer Swap", request))
    CompleteAwaitedButton(request);
}

void ShowOfficerPresets()
{
  const auto revision = button_request_revision;
  if (PressOfficerPresetsButton() || revision != button_request_revision)
    return;
  auto request = AwaitButton(ButtonStage::kOfficerAssignment);
  if (!request)
    return;
  if (PressOfficerSwapButton(request)) {
    if (pending_button == request)
      request->stage = ButtonStage::kOfficerPresets;
  } else if (pending_button == request && !OpenManageShipsFor("Officer Presets", request))
    CompleteAwaitedButton(request);
}

// --- Shortcut table ---

struct ScreenShortcut {
  Action shortcut;
  void (*run)();
};

// Checked in order; the first pressed one runs.
constexpr ScreenShortcut kScreens[] = {
    {Action::kShowQTrials, Goto<SectionID::ChallengeSelection>},
    {Action::kShowBookmarks, ShowBookmarks},
    {Action::kShowLookup, ShowCoordinateSearch},
    {Action::kShowRefinery, Goto<SectionID::Shop_Refining_List>},
    {Action::kShowFactions, Goto<SectionID::Shop_MainFactions>},
    {Action::kShowStationExterior, Goto<SectionID::Starbase_Exterior>},
    {Action::kNativeShortcutGalaxy, Native<"OnGalaxyAction">},
    {Action::kShowGalaxy, GotoNavigation<SectionID::Navigation_Galaxy>},
    {Action::kShowStationInterior, Goto<SectionID::Starbase_Interior>},
    {Action::kShowHaven, PlanetaryBaseManager::ViewOwnHaven},
    {Action::kShowSystem, GotoNavigation<SectionID::Navigation_System>},
    {Action::kShowArtifacts, Goto<SectionID::ArtifactHall_Inventory>},
    {Action::kShowShipConstruction, Native<"OnShipsAction">},
    {Action::kShowOfficerSwap, ShowOfficerSwap},
    {Action::kShowShipSwap, ShowShipSwap},
    {Action::kShowShields, Native<"OnPeaceShieldAction">},
    {Action::kShowBattlelogs, Native<"OnInboxAction">},
    {Action::kShowInventory, Goto<SectionID::InventoryList>},
    {Action::kShowMissions, Goto<SectionID::Missions_AcceptedList>},
    {Action::kShowResearch, Goto<SectionID::Research_LandingPage>},
    {Action::kShowScrapYard, Goto<SectionID::ShipScrapping_List>},
    {Action::kShowOfficers, Goto<SectionID::OfficerInventory>},
    {Action::kShowOfficerPresets, ShowOfficerPresets},
    // TODO: opens the first fleet commander (Spock), not the selected fleet's.
    {Action::kShowCommander, Goto<SectionID::FleetCommander_Management>},
    {Action::kShowAwayTeam, Goto<SectionID::Missions_AwayTeamsList>},
    {Action::kNativeShortcutEvents, Native<"OnEventsAction">},
    {Action::kShowEvents, Goto<SectionID::Tournament_Group_Selection>},
    {Action::kShowExoComp, Goto<SectionID::Consumables>},
    {Action::kShowDaily, Goto<SectionID::Missions_DailyGoals>},
    {Action::kShowGifts, Goto<SectionID::Shop_List>},
    {Action::kShowAlliance, Goto<SectionID::Alliance_Main>},
    {Action::kShowAllianceHelp, Goto<SectionID::Alliance_Help>},
    {Action::kShowAllianceArmada, Goto<SectionID::Alliance_Armadas>},
    {Action::kShowSettings, Goto<SectionID::GameSettings>},
};

// --- Arrow keys and search ---

// Left and right step through docks, ship lists, artifacts and officer pages on the screens that have them, before
// they pan the camera. Each Step* moves one screen by `step`, -1 or 1, and says whether it moved.

bool StepShipDetails(ShipManagementViewController* controller, int32_t step)
{
  auto owner   = il2cpp::GcHandle::Strong(controller);
  auto context = owner && CanvasController::OnScreen(controller) ? il2cpp::GcHandle::Strong(controller->Context())
                                                                 : il2cpp::GcHandle{};
  if (!context)
    return false;
  auto*         value   = context.Get<ShipManagementViewContext>();
  const auto    count   = value->TryCall<"get_ListCount", int32_t>();
  const auto    current = value->TryCall<"get_CurrentIndex", int32_t>();
  const int64_t next    = current ? static_cast<int64_t>(current.value) + step : -1;
  if (!count || !current || count.value <= 1 || current.value < 0 || current.value >= count.value || next < 0
      || next >= count.value || controller->Context() != value)
    return false;
  if (!value->TryCall<"set_LastMovementWasToIncrement", void(bool)>(step > 0) || controller->Context() != value)
    return false;
  return value->TryCall<"set_CurrentIndex", void(int32_t)>(static_cast<int32_t>(next)).succeeded;
}

// Skips empty dock slots.
bool StepDock(AssignShipsWidget* widget, int32_t step)
{
  auto  owner = il2cpp::GcHandle::Strong(widget);
  auto* list  = owner && CanvasController::OnScreen(widget) ? widget->selectableList() : nullptr;
  if (!list)
    return false;
  auto list_root = il2cpp::GcHandle::Strong(list);
  if (!list_root)
    return false;
  const auto count          = list->TryCall<"get_Count", int32_t>();
  const auto selected_index = list->TryCall<"get_SelectedIndex", int32_t>();
  if (!count || !selected_index || count.value <= 0 || selected_index.value < -1 || selected_index.value >= count.value)
    return false;
  for (int64_t index = static_cast<int64_t>(selected_index.value) + step; index >= 0 && index < count.value;
       index += step) {
    auto       fleet = il2cpp::GcHandle::Strong(list->DataItem(static_cast<int32_t>(index)));
    const auto has_ship =
        fleet ? fleet.Get<FleetPlayerData>()->TryCall<"get_HasShip", bool>() : il2cpp::CallResult<bool>{};
    if (has_ship && has_ship.value && widget->selectableList() == list) {
      return list->TryCall<"RequestSelect", void(int32_t, bool)>(static_cast<int32_t>(index), false).succeeded;
    }
  }
  return false;
}

bool StepArtifact(ArtifactHallDetailsViewController* controller, int32_t step)
{
  if (!controller->isActiveAndEnabled())
    return false;
  step < 0 ? controller->PressLeftArrow() : controller->PressRightArrow();
  return true;
}

bool StepSelector(ElementSelectorViewController* selector, int32_t step)
{
  if (!selector->isActiveAndEnabled())
    return false;
  step < 0 ? selector->PressDecrement() : selector->PressIncrement();
  return true;
}

// Steps every live Screen, and says whether any of them moved.
template <typename Screen> bool StepAll(bool (*step_one)(Screen*, int32_t), int32_t step)
{
  bool moved = false;
  for (auto* screen : unity::Instances<Screen>())
    moved |= step_one(screen, step);
  return moved;
}

// The first kind of screen that moves wins.
bool StepScreen(int32_t step)
{
  if (Config::Get().ui.arrow_keys_to_select_ship) {
    if (Hub::CurrentSection() == SectionID::ShipManagement_Details && StepAll(StepShipDetails, step))
      return true;
    if (StepAll(StepDock, step))
      return true;
  }
  return StepAll(StepArtifact, step) || StepAll(StepSelector, step);
}

// Focuses the search box of whichever inventory, officer or ship list is on screen.
bool FocusSearchBox()
{
  if (!Config::Get().ui.focus_search || unity::Key::IsInputFocused())
    return false;
  const auto focus = [](auto* owner) {
    auto source = il2cpp::GcHandle::Strong(owner);
    auto field  = source && CanvasController::OnScreen(owner) ? il2cpp::GcHandle::Strong(owner->inputField())
                                                              : il2cpp::GcHandle{};
    if (!field || !field.template Get<InputFieldWidget>()->isActiveAndEnabled() || !CanvasController::OnScreen(owner)
        || owner->inputField() != field.Get() || !Config::Get().ui.focus_search)
      return false;
    field.template Get<InputFieldWidget>()->Focus();
    return true;
  };
  // InventoryListViewController also covers the officer roster and the other inventory screens.
  return std::ranges::any_of(unity::Instances<InventoryListViewController>(), focus)
         || std::ranges::any_of(unity::Instances<OfficerAssignmentViewController>(), focus)
         || std::ranges::any_of(unity::Instances<AssignShipsWidget>(), focus);
}
} // namespace

void CancelAwaitedButton()
{
  ++button_request_revision;
  pending_button.reset();
}

bool PressAwaitedButton()
{
  const auto& control = Config::Get().control;
  if (!control.hotkeys_enabled || control.use_scopely_hotkeys) {
    CancelAwaitedButton();
    return false;
  }
  if (!pending_button)
    return false;
  const auto request = pending_button;
  if (Shortcut::IsDown(Action::kShowShips)
      || std::ranges::any_of(kScreens, [](const auto& screen) { return Shortcut::IsDown(screen.shortcut); })) {
    CompleteAwaitedButton(request);
    return false; // Let the new screen request win before an old deferred button can fire on this frame.
  }
  if (!AwaitedSourceCurrent(request)) {
    CompleteAwaitedButton(request);
    return false;
  }

  bool        pressed = false;
  const char* name    = "Officer Presets";
  switch (request->stage) {
    case ButtonStage::kShipSwap:
      name    = "Ship Swap";
      pressed = PressShipSwapButton(request);
      break;
    case ButtonStage::kOfficerSwap:
      name    = "Officer Swap";
      pressed = PressOfficerSwapButton(request);
      break;
    case ButtonStage::kOfficerAssignment:
      pressed = PressOfficerPresetsButton(request);
      if (!pressed && pending_button == request && PressOfficerSwapButton(request) && pending_button == request)
        request->stage = ButtonStage::kOfficerPresets;
      break;
    case ButtonStage::kOfficerPresets:
      pressed = PressOfficerPresetsButton(request);
      break;
  }
  if (pressed) {
    CompleteAwaitedButton(request);
    return true;
  }
  if (pending_button != request)
    return false;
  if (--request->frames == 0) {
    CompleteAwaitedButton(request);
    spdlog::warn("[Hotkeys] timed out waiting for the {} button", name);
  }
  return false;
}

bool ScreenShortcuts()
{
  if (Shortcut::IsDown(Action::kMoveLeft) && StepScreen(-1))
    return true;
  if (Shortcut::IsDown(Action::kMoveRight) && StepScreen(1))
    return true;
  if (Config::Get().ui.focus_search && Shortcut::IsDown(Action::kFocusSearch) && FocusSearchBox())
    return true;
  for (const auto& screen : kScreens) {
    if (Shortcut::IsDown(screen.shortcut)) {
      // A new screen request replaces any button still waiting on the previous screen.
      CancelAwaitedButton();
      screen.run();
      return true;
    }
  }
  return false;
}

// The source places Manage Ships after the setting-shortcut else-if chain and lets native Update continue.
// HandleShortcuts invokes this only if no setting shortcut matched; keep that precedence without a second table.
void LateScreenShortcut()
{
  if (Shortcut::IsDown(Action::kShowShips)) {
    CancelAwaitedButton();
    (void)OpenManageShips();
  }
}
