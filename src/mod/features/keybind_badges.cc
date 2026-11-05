// The game's keybind badges, relabelled with the mod's shortcuts. The game shows a badge per input action when
// its show_keybindings action is toggled; with mod hotkeys on, each badge shows the first binding of the mod
// action that replaces it.

#include "config.h"
#include "features/features.h"
#include "features/ship_shortcut_hint.h"
#include "shortcuts.h"

#include "game/fleet.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/tracker.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace
{
bool badges_installed = false;

// --- Badge text ---

struct NativeAction {
  std::string_view name;
  Action           primary;
  Action           fallback = Action::kCount;
};

// The game's input action names and the mod actions that replace them.
constexpr auto kNativeActions = std::to_array<NativeAction>({
    {"interior_view", Action::kShowStationInterior},
    {"exterior_view", Action::kShowStationExterior},
    {"system_view", Action::kShowSystem},
    {"galaxy_view", Action::kShowGalaxy},
    {"events", Action::kShowEvents},
    {"ship_a", Action::kSelectShip1},
    {"ship_b", Action::kSelectShip2},
    {"ship_c", Action::kSelectShip3},
    {"ship_d", Action::kSelectShip4},
    {"ship_e", Action::kSelectShip5},
    {"ship_f", Action::kSelectShip6},
    {"ship_g", Action::kSelectShip7},
    {"ship_h", Action::kSelectShip8},
    {"alliance", Action::kShowAlliance},
    {"chat", Action::kShowChat},
    {"side_chat", Action::kShowChatSide1, Action::kShowChatSide2},
    {"away_teams", Action::kShowAwayTeam},
    {"missions", Action::kShowMissions},
    {"daily_goals", Action::kShowDaily},
    {"ship_locate", Action::kSelectCurrent},
    {"ship_manage", Action::kShowShips},
    {"research", Action::kShowResearch},
    {"consumables", Action::kShowExoComp},
    {"ship_recall", Action::kActionRecall},
    {"show_keybindings", Action::kToggleShortcutHints},
    {"gifts", Action::kShowGifts},
    {"help_alliance", Action::kShowAllianceHelp},
    {"officers", Action::kShowOfficers},
    {"factions", Action::kShowFactions},
    {"items", Action::kShowInventory},
    {"refinery", Action::kShowRefinery},
    {"commanders", Action::kShowCommander},
    {"challenges", Action::kShowQTrials},
});

constexpr const NativeAction* FindNativeAction(std::string_view name)
{
  const auto action = std::ranges::find(kNativeActions, name, &NativeAction::name);
  return action == kNativeActions.end() ? nullptr : &*action;
}

static_assert(std::ranges::all_of(kNativeActions, [](const NativeAction& action) {
  return std::ranges::count(kNativeActions, action.name, &NativeAction::name) == 1;
}));
static_assert(FindNativeAction("side_chat")->fallback == Action::kShowChatSide2);
static_assert(FindNativeAction("unknown") == nullptr);

// The badge text for a native action: its mod binding, or "-" when it has none.
std::string BadgeText(std::string_view action_name)
{
  const auto* action = FindNativeAction(action_name);
  if (!action)
    return "-";
  auto hint_action = action->primary;
  if (hint_action >= Action::kSelectShip1 && hint_action <= Action::kSelectShip8) {
    // The mod's ship keys select by position in the bar's context array, as RequestSelect does, not by dock index.
    auto bar = il2cpp::GcHandle::Strong(unity::Newest<FleetBarViewController>());
    auto context =
        bar ? il2cpp::GcHandle::Strong(bar.Get<FleetBarViewController>()->CanvasContext()) : il2cpp::GcHandle{};
    auto array = context ? il2cpp::GcHandle::Strong(context.Get<FleetBarContext>()->FleetData()) : il2cpp::GcHandle{};
    const auto fleets = il2cpp::Items(array.Get<il2cpp::Array<FleetPlayerData*>>());
    hint_action       = ShipShortcutHintAction(hint_action, fleets.size(), [fleets](std::size_t position) {
      auto       fleet = il2cpp::GcHandle::Strong(fleets[position]);
      const auto index =
          fleet ? fleet.Get<FleetPlayerData>()->TryCall<"get_Index", int32_t>() : il2cpp::CallResult<int32_t>{};
      return index ? index.value : -1;
    });
  }
  auto hint = hint_action == Action::kCount ? std::string{} : Shortcut::Hint(hint_action);

  if (hint.empty() && action->fallback != Action::kCount)
    hint = Shortcut::Hint(action->fallback);
  return hint.empty() ? "-" : hint;
}

// --- Hooks ---

void ShortcutKeybindHint_UpdateText_Hook(auto original, ShortcutKeybindHint* _this)
{
  auto owner = il2cpp::GcHandle::Strong(_this);
  auto label = owner ? il2cpp::GcHandle::Strong(_this->keyTextLocalizer()) : il2cpp::GcHandle{};
  if (!label)
    return original(_this);

  // UpdateText does not clear an existing override; clear ours first so native, disabled and unknown actions
  // cannot keep a stale mod badge.
  label.Get<TextLocalizer>()->ClearTextOverride();
  original(_this);

  if (_this->keyTextLocalizer() != label.Get() || !Config::Get().control.hotkeys_enabled
      || Config::Get().control.use_scopely_hotkeys)
    return;

  auto reference = il2cpp::GcHandle::Strong(_this->inputAction());
  auto action =
      reference ? il2cpp::GcHandle::Strong(reference.Get<unity::InputActionReference>()->action()) : il2cpp::GcHandle{};
  auto name = action
                  ? il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(action.Get<unity::InputAction>()->name()))
                  : il2cpp::GcHandle{};
  if (!name)
    return;
  const auto hint = BadgeText(il2cpp::ToUtf8(reinterpret_cast<Il2CppString*>(name.Get())));
  auto       text = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(il2cpp::NewString(hint)));
  if (text && _this->keyTextLocalizer() == label.Get() && _this->inputAction() == reference.Get()
      && Config::Get().control.hotkeys_enabled && !Config::Get().control.use_scopely_hotkeys)
    label.Get<TextLocalizer>()->OverrideLocalizedText(reinterpret_cast<Il2CppString*>(text.Get()));
}

void ShortcutKeybindHint_UpdateVisibility_Hook(auto original, ShortcutKeybindHint* _this, bool visible)
{
  auto owner = il2cpp::GcHandle::Strong(_this);
  original(_this, visible);
  // A badge that becomes visible keeps the text it had while hidden. Update its text again, which runs the text hook,
  // so it shows the current binding.
  if (visible && owner)
    _this->UpdateText();
}
} // namespace

bool ToggleKeybindBadges()
{
  if (!badges_installed)
    return false;
  auto  owner   = il2cpp::GcHandle::Strong(ShortcutsManager::Instance());
  auto* manager = owner.Get<ShortcutsManager>();
  if (!manager) {
    spdlog::warn("[Hotkeys] the game's shortcut manager is not up; no badges to toggle");
    return false;
  }
  // The same gate the game's own show_keybindings action checks.
  const auto allowed = ShortcutsManager::Class::TryCall<"get_CanUseShortcuts", bool>();
  if (!allowed || !allowed.value)
    return false;
  const auto shown = manager->TryCall<"get_ShowKeybindings", bool>();
  return shown && manager->TryCall<"set_ShowKeybindings", void(bool)>(!shown.value).succeeded;
}

void HideKeybindBadges()
{
  auto       owner = il2cpp::GcHandle::Strong(badges_installed ? ShortcutsManager::Instance() : nullptr);
  const auto shown =
      owner ? owner.Get<ShortcutsManager>()->TryCall<"get_ShowKeybindings", bool>() : il2cpp::CallResult<bool>{};
  if (shown && shown.value)
    owner.Get<ShortcutsManager>()->SetShowKeybindings(false);
}

void InstallKeybindBadges()
{
  // Frozen PLAY installs independently of the current binding: Settings can
  // assign an initially unbound toggle later without restarting the game.

  // Only the detour targets gate the feature. Members the badge path reads resolve on first use; a missing one is
  // reported once and reads as null, which leaves the game's own badge text.
  il2cpp::HookSet hooks("KeybindBadges");
  IL2CPP_STAGE(hooks, ShortcutKeybindHint::Class::Method("UpdateText", 0), ShortcutKeybindHint_UpdateText_Hook);
  IL2CPP_STAGE(hooks, ShortcutKeybindHint::Class::Method("UpdateVisibility", 1),
               ShortcutKeybindHint_UpdateVisibility_Hook);
  badges_installed = hooks.Commit();
}
