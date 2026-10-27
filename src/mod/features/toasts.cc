// Toast banners: [ui] disabled_banner_types hides banner types, [ui] notify_banner_types mirrors them to Windows
// notifications with a readable body for battle results, and [audio] alert_* plays a cue for battle results.

#include "config.h"
#include "features/features.h"
#include "features/fleet_notification_settings.h"
#include "features/incoming_player_attack_tracker.h"
#include "features/notification_audio.h"
#include "features/notifications.h"
#include "features/toast_text.h"

#include "game/notifications.h"
#include "game/server.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if _WIN32
#include <Windows.h>
#include <winrt/windows.data.xml.dom.h>
#include <winrt/windows.ui.notifications.h>
#endif

namespace
{
// --- Toast states ---

constexpr std::pair<ToastState, const char*> kTitles[] = {
    {ToastState::Standard, "Notification"},
    {ToastState::FactionWarning, "Faction Warning"},
    {ToastState::FactionLevelUp, "Faction Level Up"},
    {ToastState::FactionLevelDown, "Faction Level Down"},
    {ToastState::FactionDiscovered, "Faction Discovered"},
    {ToastState::IncomingAttack, "Incoming Attack!"},
    {ToastState::IncomingAttackFaction, "Incoming Faction Attack!"},
    {ToastState::FleetBattle, "Fleet Battle"},
    {ToastState::StationBattle, "Station Under Attack!"},
    {ToastState::StationVictory, "Station Victory!"},
    {ToastState::Victory, "Victory!"},
    {ToastState::Defeat, "Defeat"},
    {ToastState::StationDefeat, "Station Defeat"},
    {ToastState::Tournament, "Event Progress"},
    {ToastState::ArmadaCreated, "Armada Created"},
    {ToastState::ArmadaCanceled, "Armada Canceled"},
    {ToastState::ArmadaIncomingAttack, "Armada Under Attack!"},
    {ToastState::ArmadaBattleWon, "Armada Victory!"},
    {ToastState::ArmadaBattleLost, "Armada Defeated"},
    {ToastState::DiplomacyUpdated, "Diplomacy Updated"},
    {ToastState::JoinedTakeover, "Territory Capture Joined"},
    {ToastState::CompetitorJoinedTakeover, "Competitor Joined Territory"},
    {ToastState::AbandonedTerritory, "Territory Abandoned"},
    {ToastState::TakeoverVictory, "Takeover Victory!"},
    {ToastState::TakeoverDefeat, "Takeover Defeat"},
    {ToastState::TreasuryProgress, "Treasury Progress"},
    {ToastState::TreasuryFull, "Treasury Full"},
    {ToastState::Achievement, "Achievement"},
    {ToastState::AssaultVictory, "Assault Victory!"},
    {ToastState::AssaultDefeat, "Assault Defeat"},
    {ToastState::ChallengeComplete, "Challenge Complete"},
    {ToastState::ChallengeFailed, "Challenge Failed"},
    {ToastState::StrikeHit, "Strike Hit"},
    {ToastState::StrikeDefeat, "Strike Defeat"},
    {ToastState::WarchestProgress, "Warchest Progress"},
    {ToastState::WarchestFull, "Warchest Full"},
    {ToastState::PartialVictory, "Partial Victory"},
    {ToastState::ArenaTimeLeft, "Arena Time Warning"},
    {ToastState::ChainedEventScored, "Event Progress"},
    {ToastState::FleetPresetApplied, "Fleet Preset Applied"},
    {ToastState::SurgeWarmUpEnded, "Surge Started"},
    {ToastState::SurgeHostileGroupDefeated, "Surge Hostiles Defeated"},
    {ToastState::SurgeTimeLeft, "Surge Time Warning"},
    {ToastState::QueueForLeaseActivated, "Queue Activated"},
    {ToastState::QueueForLeaseExpired, "Queue Expired"},
    {ToastState::PermanentQueuePurchased, "Permanent Queue Purchased"},
    {ToastState::OutpostStartedOrEnded, "Outpost Update"},
    {ToastState::CrossAllianceArmadaVictory, "Cross-Armada Victory!"},
    {ToastState::CrossAllianceArmadaDefeat, "Cross-Armada Defeated"},
    {ToastState::CrossAllianceArmadaPartialVictory, "Cross-Armada Partial Victory"},
    {ToastState::FactionWeeklyEventsProgress, "Faction Weekly Event Progress"},
    {ToastState::FactionWeeklyEventsComplete, "Faction Weekly Event Complete"},
    {ToastState::ArmadaPlayerBlocked, "Armada Player Blocked"},
    {ToastState::ArmadaPlayerUnblocked, "Armada Player Unblocked"},
    {ToastState::DynamicCrisisUpdate, "Dynamic Crisis Update"},
    {ToastState::DynamicCrisisFailed, "Dynamic Crisis Failed"},
    {ToastState::DynamicCrisisCompleted, "Dynamic Crisis Completed"},
    {ToastState::GalacticAnomalySystemEntered, "Galactic Anomaly Entered"},
};

// Indexed by Digit.PrimePlatform.Models.EventCategories.
constexpr const char* kEventCategories[] = {
    "Standard",           "Daily Goals",          "Daily Milestone",    "Leaderboard",           "Stat",
    "Battle Pass Season", "Battle Pass Event",    "Treasury Progress",  "Treasury Reward",       "Server Clash",
    "Webstore Event",     "Player Lifecycle",     "Field Training",     "FT Category",           "Cutscenes",
    "Minigame",           "Minigame Stage",       "Warchest",           "Alliance Game",         "Alliance Game Task",
    "Meta Event",         "Meta Event Objective", "Invasion",           "Loop Museum",           "Loop Museum Task",
    "PLC BP Season",      "PLC BP Event",         "Progression Reward", "Faction Weekly Events",
};

// Toasts whose Data is an EventModel; their body is prefixed with the event's category.
constexpr ToastState kEventToasts[] = {ToastState::Achievement,
                                       ToastState::Tournament,
                                       ToastState::ChainedEventScored,
                                       ToastState::TreasuryProgress,
                                       ToastState::TreasuryFull,
                                       ToastState::WarchestProgress,
                                       ToastState::WarchestFull,
                                       ToastState::FactionWeeklyEventsProgress,
                                       ToastState::FactionWeeklyEventsComplete};

// Toasts whose Data is a BattleResultHeader.
constexpr ToastState kBattleToasts[] = {
    ToastState::Victory,         ToastState::Defeat,           ToastState::PartialVictory, ToastState::StationVictory,
    ToastState::StationDefeat,   ToastState::StationBattle,    ToastState::IncomingAttack, ToastState::FleetBattle,
    ToastState::ArmadaBattleWon, ToastState::ArmadaBattleLost, ToastState::AssaultVictory, ToastState::AssaultDefeat};

constexpr BattleType kPvpBattles[] = {BattleType::Fleet,       BattleType::Base,      BattleType::ArmadaBase,
                                      BattleType::ArmadaAsb,   BattleType::ArmadaMta, BattleType::PvpCuttingBeam,
                                      BattleType::PvpChainShot};

// --- Notification text ---
//
// Toast data comes from a dozen game types whose layouts drift between builds. On Windows, an access violation
// while reading one is contained and logged instead of taking the game down, so every read below is Guarded.

using il2cpp::Guarded;

// Localizes `identifier` with one parameter.
std::string Localize(const char* identifier, const char* category, Il2CppObject* parameter)
{
  std::string result;
  Guarded(identifier, [&] { result = LocaleUtilities::Localize(identifier, category, parameter); });
  return result;
}

// NPC and marauder profiles have no name of their own; the game names them by their localization id.
std::string EnemyName(int64_t loca_id)
{ return loca_id ? Localize("marauder_name_only_{0}", "navigation", il2cpp::Box(loca_id)) : std::string{}; }

// A hull's display name: localized by numeric id, then by string id, then derived from its internal key.
std::string HullName(BattleResultHeader* header, int64_t hull_id)
{
  if (hull_id == 0)
    return "";
  auto* specs = header->specService();
  auto* hull  = specs ? specs->GetHull(hull_id) : nullptr;
  if (!hull)
    return fmt::format("Hull#{}", hull_id);

  // An unknown id localizes to the identifier itself, which counts as no name.
  const auto localized = [](Il2CppObject* id, const std::string& id_text) {
    auto name = Localize("ship_name_{0}", "ships", id);
    return name == "ship_name_" + id_text ? std::string{} : name;
  };
  if (auto* ids = hull->idRefs()) {
    if (const auto loca_id = ids->locaId())
      if (auto name = localized(il2cpp::Box(loca_id), std::to_string(loca_id)); !name.empty())
        return name;
    if (auto* loca_string = ids->locaStringId(); loca_string && loca_string->length)
      if (auto name = localized(reinterpret_cast<Il2CppObject*>(loca_string), il2cpp::ToUtf8(loca_string));
          !name.empty())
        return name;
  }
  auto* key = hull->name();
  return key ? toast_text::HullKeyName(il2cpp::ToUtf8(key)) : fmt::format("Hull#{}", hull_id);
}

// "Player (Ship) vs Enemy (Ship)". Outside PvP a named enemy is enough; its hull class is noise.
std::string BattleBody(BattleResultHeader* header)
{
  std::string player, enemy, player_ship, enemy_ship;
  bool        pvp = false;

  Guarded("the player profile", [&] {
    if (auto* profile = header->PlayerUserProfile())
      player = il2cpp::ToUtf8(profile->name());
  });
  Guarded("the enemy profile", [&] {
    auto* profile = header->EnemyUserProfile();
    if (!profile)
      return;
    enemy = il2cpp::ToUtf8(profile->name());
    if (enemy.empty()) {
      auto name = EnemyName(profile->locaId());
      if (name != "Unknown")
        enemy = std::move(name);
    }
  });
  Guarded("the battle type", [&] { pvp = std::ranges::contains(kPvpBattles, header->Type()); });
  Guarded("the player hull",
          [&] { player_ship = toast_text::TitleCase(HullName(header, header->PlayerShipHullId())); });
  Guarded("the enemy hull", [&] { enemy_ship = toast_text::TitleCase(HullName(header, header->EnemyShipHullId())); });

  const auto side = [](const std::string& name, const std::string& ship) {
    return name.empty() ? ship : ship.empty() ? name : fmt::format("{} ({})", name, ship);
  };
  const auto left  = side(player, player_ship);
  const auto right = !pvp && !enemy.empty() ? enemy : side(enemy, enemy_ship);
  if (left.empty() || right.empty())
    return left + right;
  return left + " vs " + right;
}

// "[TAG] Owner started an Armada!\nLv.042 Target"
std::string ArmadaCreatedBody(ArmadaCreatedData* data)
{
  std::string body;
  Guarded("armada data", [&] {
    auto* attack   = data ? data->ArmadaAttack() : nullptr;
    auto* alliance = data ? data->Alliance() : nullptr;
    auto* owner    = attack ? attack->Owner() : nullptr;
    auto* target   = attack ? attack->TargetUserProfile() : nullptr;

    const auto tag         = alliance ? il2cpp::ToUtf8(alliance->tag()) : std::string{};
    auto       owner_name  = owner ? il2cpp::ToUtf8(owner->name()) : std::string{};
    auto       target_name = target ? il2cpp::ToUtf8(target->name()) : std::string{};
    if (target_name.empty() && target)
      target_name = EnemyName(target->locaId());
    if (owner_name.empty())
      owner_name = "An alliance member";
    if (target_name.empty() || target_name == "Unknown")
      target_name = "Armada target";

    const auto owner_label = tag.empty() ? owner_name : fmt::format("[{}] {}", tag, owner_name);
    const auto level       = attack ? attack->TargetLevel() : 0;
    body                   = fmt::format("{} started an Armada!\nLv.{:03} {}", owner_label, level, target_name);
  });
  return body;
}

// The string form of each element: strings as they are, everything else through its ToString override.
toast_text::ParameterRead ParameterValues(const il2cpp::GcHandle& array_root, toast_text::ParameterAccess access)
{
  auto* array = array_root.Get<il2cpp::Array<Il2CppObject*>>();
  if (!array)
    return {};
  const auto count = array->size();
  if (!count)
    return {.state = toast_text::ParameterState::kEmpty};
  if (access == toast_text::ParameterAccess::kPresence)
    return {.state = toast_text::ParameterState::kValues};
  std::vector<std::string> values;
  values.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    // A managed ToString may allocate. Keep the array and current element rooted, and do not retain a span
    // into managed storage across it. Reacquire the array target for each observation.
    array = array_root.Get<il2cpp::Array<Il2CppObject*>>();
    if (!array)
      return {};
    auto* element      = (*array)[i];
    auto  element_root = il2cpp::GcHandle::Strong(element);
    if (element && !element_root)
      return {};
    Il2CppString* text = nullptr;
    if (element)
      Guarded("a text parameter", [&] { text = il2cpp::ToString(element_root.Get<il2cpp::Object>()); });
    auto text_root = il2cpp::GcHandle::Strong(text ? &text->object : nullptr);
    if (text && !text_root)
      return {};
    values.push_back(il2cpp::ToUtf8(reinterpret_cast<Il2CppString*>(text_root.Get())));
  }
  return {.state = toast_text::ParameterState::kValues, .values = std::move(values)};
}

template <typename Source>
toast_text::ParameterRead ReadToastParameters(Source* source, toast_text::ParameterAccess access)
{
  if (!source)
    return {};
  const auto observed = source->TryTextParameters();
  if (!observed)
    return {};
  if (!observed.value)
    return {.state = toast_text::ParameterState::kNull};
  // A getter may return a temporary array. Retain it before metadata resolution, not only before ToString.
  auto array_root = il2cpp::GcHandle::Strong(observed.value);
  if (!array_root || !array_root.Get())
    return {};
  const auto checked = il2cpp::TryReferenceArray<Il2CppObject*>(array_root.Get());
  if (!checked)
    return {.state = toast_text::ParameterState::kInvalid};
  return ParameterValues(array_root, access);
}

// Both named getters are object[] in the retained client metadata. Never scan unrelated array fields.
std::string FormatToastText(const std::string& text, const il2cpp::GcHandle& toast_root,
                            const il2cpp::GcHandle& context_root, toast_text::TextStage stage)
{
  auto result = toast_text::FormatParameters(
      text,
      [&](toast_text::ParameterSource source, toast_text::ParameterAccess access) {
        return source == toast_text::ParameterSource::kToast
                   ? ReadToastParameters(toast_root.Get<Toast>(), access)
                   : ReadToastParameters(context_root.Get<LocaleTextContext>(), access);
      },
      stage);
  if (result.source == toast_text::ParameterSource::kNone && result.toast && text.contains('{'))
    spdlog::warn("[Toasts] No usable text parameters; placeholders unresolved in template: \"{}\"", text);
  return std::move(result.text);
}

// The toast's own localized text with its parameters filled in, for toasts without a body of their own.
std::string ToastText(Toast* toast)
{
  auto toast_root = il2cpp::GcHandle::Strong(toast);
  if (!toast_root || !toast_root.Get<Toast>())
    return {};
  auto* context = toast_root.Get<Toast>()->TextLocaleTextContext();
  if (!context) {
    spdlog::debug("[Toasts] No TextLocaleTextContext for toast state {}",
                  std::to_underlying(toast_root.Get<Toast>()->State()));
    return {};
  }
  auto context_root = il2cpp::GcHandle::Strong(context);
  if (!context_root || !context_root.Get<il2cpp::Object>()
      || !context_root.Get<il2cpp::Object>()->As<LocaleTextContext>())
    return {};

  // LocaleUtilities.Localize fills the context's own parameters; the toast may carry them instead.
  if (auto* text = LocaleUtilities::Localize(context_root.Get<LocaleTextContext>(), true, false)) {
    auto result = il2cpp::ToUtf8(text);
    return result.contains('{')
               ? FormatToastText(result, toast_root, context_root, toast_text::TextStage::kContextFormatted)
               : result;
  }

  Il2CppString* text    = nullptr;
  auto*         manager = LanguageManager::Instance();
  if (!manager || !manager->Localize(text, context_root.Get<LocaleTextContext>()) || !text) {
    spdlog::debug("[Toasts] toast state {} could not be localized",
                  std::to_underlying(toast_root.Get<Toast>()->State()));
    return {};
  }
  // The retained LanguageManager body looks up the identifier hash/category without formatting text parameters.
  return FormatToastText(il2cpp::ToUtf8(text), toast_root, context_root, toast_text::TextStage::kRaw);
}

// Empty for a category this table does not name.
std::string_view EventCategory(Toast* toast)
{
  auto*      data     = toast->Data();
  auto*      event    = data ? data->As<EventModel>() : nullptr;
  const auto category = event ? event->category() : -1;
  return category >= 0 && std::cmp_less(category, std::size(kEventCategories)) ? kEventCategories[category] : "";
}

std::string NotificationBody(Toast* toast)
{
  const auto  state = toast->State();
  auto*       data  = toast->Data();
  std::string body;
  if (state == ToastState::ArmadaCreated)
    body = ArmadaCreatedBody(static_cast<ArmadaCreatedData*>(data));
  else if (auto* header =
               data && std::ranges::contains(kBattleToasts, state) ? data->As<BattleResultHeader>() : nullptr)
    body = BattleBody(header);
  if (body.empty())
    body = toast_text::StripRichText(ToastText(toast));
  if (body.empty())
    body = "(no details available)";

  if (std::ranges::contains(kEventToasts, state))
    if (const auto category = EventCategory(toast); !category.empty())
      body = fmt::format("{} - {}", category, body);
  return body;
}

// --- Windows notifications ---

#if _WIN32
void ShowWindowsNotification(const char* title, const std::string& body)
{
  // Notifications need a WinRT apartment on the game thread, set up by the first one. The game may already have
  // one in another mode, which only matters if notifications then fail.
  [[maybe_unused]] static const bool apartment = [] {
    try {
      winrt::init_apartment(winrt::apartment_type::single_threaded);
    } catch (const winrt::hresult_error& e) {
      spdlog::warn("[Toasts] Windows notification service failed: {:#x} {}", static_cast<unsigned long>(e.code()),
                   winrt::to_string(e.message()));
    } catch (...) {
      spdlog::warn("[Toasts] Windows notification service failed (unknown error)");
    }
    return true;
  }();
  try {
    using namespace winrt::Windows::UI::Notifications;
    auto xml   = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);
    auto nodes = xml.GetElementsByTagName(L"text");
    nodes.Item(0).InnerText(winrt::to_hstring(title));
    nodes.Item(1).InnerText(winrt::to_hstring(body));
    ToastNotificationManager::CreateToastNotifier(L"Star Trek Fleet Command").Show(ToastNotification(xml));
  } catch (const winrt::hresult_error& e) {
    spdlog::warn("[Toasts] WinRT notification failed: {}", winrt::to_string(e.message()));
  } catch (...) {
    spdlog::warn("[Toasts] WinRT notification failed (unknown error)");
  }
}
#endif

thread_local unsigned toast_notification_suppression_depth = 0;

void Notify(Toast* toast)
{
  if (toast_notification_suppression_depth)
    return;
  const auto  state = toast->State();
  const auto& types = Config::Get().ui.notify_banner_types;
  if (!std::ranges::contains(types, std::to_underlying(state)))
    return;
  const auto title = std::ranges::find(kTitles, state, &std::pair<ToastState, const char*>::first);
  if (title == std::end(kTitles)) {
    spdlog::debug("[Toasts] No title mapping for toast state {}, skipping", std::to_underlying(state));
    return;
  }
  EmitDesktopNotification(title->second, NotificationBody(toast));
}

// --- Banners ---

// True means the dedicated alert path handled side effects; native banner visibility remains independent.
bool HandleIncomingPlayerAlert(Toast* toast)
{
  const auto& config = Config::Get();
  if (toast->State() != ToastState::IncomingAttack
      || (!config.ui.notify_incoming_player_attack && !config.audio.alert_incoming_player_attack.enabled()))
    return false;
  const auto attack = ReadIncomingPlayerAttack(toast);
  if (!attack)
    return false;
  static IncomingPlayerAttackTracker tracker;
  static std::mutex                  mutex;
  bool                               first;
  {
    std::lock_guard lock(mutex);
    first =
        tracker.Accept(attack->event_id, attack->fleet_id, attack->attacker, IncomingPlayerAttackTracker::Clock::now());
  }
  if (!first)
    return true;
  PlayNotificationSound(config.audio.alert_incoming_player_attack);
  if (!toast_notification_suppression_depth
      && (config.ui.notify_incoming_player_attack
          || std::ranges::contains(config.ui.notify_banner_types, std::to_underlying(ToastState::IncomingAttack)))) {
    auto body = toast_text::StripRichText(ToastText(toast));
    if (body.empty()) {
      body = attack->attacker.empty() ? "A player is approaching your ship."
                                      : attack->attacker + " is approaching your ship.";
      if (attack->slot >= 0 && attack->slot < 100)
        body += " Ship slot " + std::to_string(attack->slot + 1) + ".";
    }
    EmitDesktopNotification("Incoming Player Attack", body);
  }
  return true;
}

// Plays the toast's alert and sends its notification, then says whether the game should show its banner.
bool ShowBanner(Toast* toast)
{
  if (!toast)
    return true;
  auto toast_root = il2cpp::GcHandle::Strong(toast);
  if (HandleIncomingPlayerAlert(toast))
    return !std::ranges::contains(Config::Get().ui.disabled_banner_types, std::to_underlying(toast->State()));
  const auto&                 audio = Config::Get().audio;
  const NotificationAudioCue* sound = nullptr;
  switch (toast->State()) {
    case ToastState::Victory:
      sound = &audio.alert_victory;
      break;
    case ToastState::Defeat:
      sound = &audio.alert_defeat;
      break;
    case ToastState::ArmadaCreated:
      sound = &audio.alert_armada_created;
      break;
    case ToastState::ArmadaBattleWon:
      sound = &audio.alert_armada_battle_won;
      break;
    case ToastState::ArmadaBattleLost:
      sound = &audio.alert_armada_battle_lost;
      break;
    default:
      break;
  }
  if (sound)
    PlayNotificationSound(*sound);
  Notify(toast);
  return !std::ranges::contains(Config::Get().ui.disabled_banner_types, std::to_underlying(toast->State()));
}

void ToastObserver_EnqueueToast_Hook(auto original, ToastObserver* _this, Toast* toast)
{
  if (ShowBanner(toast))
    original(_this, toast);
}

void ToastObserver_EnqueueOrCombineToast_Hook(auto original, ToastObserver* _this, Toast* toast, uintptr_t compare)
{
  if (ShowBanner(toast))
    original(_this, toast, compare);
}

bool InstallToasts()
{
  il2cpp::HookSet hooks("Toasts");
  IL2CPP_STAGE(hooks, ToastObserver::Class::Method("EnqueueToast"), ToastObserver_EnqueueToast_Hook);
  IL2CPP_STAGE(hooks, ToastObserver::Class::Method("EnqueueOrCombineToast"), ToastObserver_EnqueueOrCombineToast_Hook);
  return hooks.Commit();
}

void ToastNotificationFrame()
{
  const auto& ui = Config::Get().ui;
  if (!ui.notify_banner_types.empty() || ui.notify_incoming_player_attack || ui.notify_fleet_events)
    InitializeDesktopNotifications();
}
} // namespace

const Feature kToasts{"Toasts", &Config::Patches::toastbannerhooks, InstallToasts, ToastNotificationFrame};

bool ToastAudioAvailable()
{
  return std::ranges::any_of(FeatureInstallations(), [](const FeatureInstallation& feature) {
    return feature.name && std::string_view(feature.name) == "Toasts" && feature.state == InstallState::kInstalled;
  });
}

void InitializeDesktopNotifications()
{
#if __APPLE__
  static bool requested = false;
  if (!std::exchange(requested, true))
    NotificationDesktopMacInit();
#endif
}

void EmitDesktopNotification(std::string_view title, std::string_view body)
{
  InitializeDesktopNotifications();
#if _WIN32
  ShowWindowsNotification(std::string(title).c_str(), std::string(body));
#elif __APPLE__
  NotificationDesktopMacEmit(title, body);
#endif
}

ScopedToastNotificationSuppression::ScopedToastNotificationSuppression()
{ ++toast_notification_suppression_depth; }
ScopedToastNotificationSuppression::~ScopedToastNotificationSuppression()
{ --toast_notification_suppression_depth; }
