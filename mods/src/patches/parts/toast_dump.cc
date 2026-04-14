#include "toast_dump.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/BattleResultHeader.h>
#include <prime/HullSpec.h>
#include <prime/LanguageManager.h>
#include <prime/SpecService.h>
#include <prime/Toast.h>
#include <prime/UserProfile.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include <string>

#if _WIN32
#include <windows.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#endif

static const MethodInfo* s_localize_ltc = nullptr;

void toast_dump_setup()
{
  // Resolve LanguageManager::Localize(out string, LTC) — the 2-arg overload
  auto lm_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Localization", "LanguageManager");
  if (lm_helper.isValidHelper()) {
    auto cls = lm_helper.get_cls();
    if (cls) {
      void* iter = nullptr;
      while (auto method = il2cpp_class_get_methods(cls, &iter)) {
        auto name = std::string_view(il2cpp_method_get_name(method));
        auto pc = il2cpp_method_get_param_count(method);
        if (name == "Localize" && pc == 2 && !s_localize_ltc) {
          s_localize_ltc = method;
          spdlog::info("Resolved LanguageManager::Localize(out string, LTC) at {:p}", (const void*)method);
          break;
        }
      }
    }
  }

#if _WIN32
  try { winrt::init_apartment(); } catch (...) {}
#endif
}

// ---- helpers ----

static const char* toast_state_title(int state)
{
  switch (state) {
    case Victory:           return "Victory!";
    case Defeat:            return "Defeat";
    case PartialVictory:    return "Partial Victory";
    case StationVictory:    return "Station Victory!";
    case StationDefeat:     return "Station Defeat";
    case StationBattle:     return "Station Under Attack!";
    case IncomingAttack:    return "Incoming Attack!";
    case FleetBattle:       return "Fleet Battle";
    case ArmadaBattleWon:   return "Armada Victory!";
    case ArmadaBattleLost:  return "Armada Defeated";
    case ArmadaCreated:     return "Armada Created";
    case ArmadaCanceled:    return "Armada Canceled";
    case AssaultVictory:    return "Assault Victory!";
    case AssaultDefeat:     return "Assault Defeat";
    default:                return nullptr;
  }
}

#if _WIN32
__declspec(noinline) static void show_system_notification(const char* title, const char* body)
{
  try {
    using namespace winrt::Windows::UI::Notifications;
    using namespace winrt::Windows::Data::Xml::Dom;

    auto xml = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);
    auto nodes = xml.GetElementsByTagName(L"text");
    nodes.Item(0).InnerText(winrt::to_hstring(title));
    nodes.Item(1).InnerText(winrt::to_hstring(body));

    auto notification = ToastNotification(xml);
    auto notifier = ToastNotificationManager::CreateToastNotifier(L"Star Trek Fleet Command");
    notifier.Show(notification);
  } catch (const winrt::hresult_error& e) {
    spdlog::warn("[Toast] WinRT notification failed: {}", winrt::to_string(e.message()));
  } catch (...) {
    spdlog::warn("[Toast] WinRT notification failed (unknown error)");
  }
}
#endif

// SEH wrapper — calls fn(), returns true on success, false on crash
template<typename Fn>
static bool seh_call(Fn fn)
{
#if _WIN32
  __try { fn(); return true; }
  __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
  fn(); return true;
#endif
}

// Parse internal hull key into a readable name
// "Hull_L38_Battleship_Liborg" -> "Lv.38 Battleship (Liborg)"
// "Hull_G4_Destroyer_Fed_Kelvin_LIVE" -> "G4 Destroyer (Fed Kelvin)"
// "Kelvin_LIVE" -> "Kelvin"
static std::string parse_hull_key(const std::string& key)
{
  auto s = key;

  // Strip trailing _LIVE
  if (s.size() > 5 && s.ends_with("_LIVE"))
    s = s.substr(0, s.size() - 5);

  // Strip leading "Hull_"
  if (s.starts_with("Hull_"))
    s = s.substr(5);

  // Replace underscores with spaces
  for (auto& c : s) if (c == '_') c = ' ';

  // Convert L## to Lv.##
  if (s.size() >= 2 && s[0] == 'L' && std::isdigit(s[1])) {
    auto space = s.find(' ');
    auto lvl = s.substr(1, space == std::string::npos ? std::string::npos : space - 1);
    auto rest = space == std::string::npos ? "" : s.substr(space);
    s = "Lv." + lvl + rest;
  }

  return s;
}

static std::string resolve_hull_name(BattleResultHeader* brh, long hullId)
{
  if (hullId == 0) return "";

  auto* specSvc = reinterpret_cast<SpecService*>(brh->get_SpecService());
  if (specSvc) {
    auto* hull = specSvc->GetHull(hullId);
    if (hull) {
      auto* nameStr = hull->Name;
      auto nameKey = nameStr ? to_string(nameStr) : std::string{};
      if (!nameKey.empty()) return parse_hull_key(nameKey);
    }
  }

  return fmt::format("Hull#{}", hullId);
}

// Split a parsed hull name into (class, faction)
// "Lv.38 Battleship Liborg" -> ("Lv.38 Battleship", "Liborg")
// "G4 Destroyer Fed Kelvin" -> ("G4 Destroyer", "Fed Kelvin")
static std::pair<std::string, std::string> split_hull_class_faction(const std::string& hull)
{
  static constexpr std::string_view classes[] = {
    "Battleship", "Destroyer", "Explorer", "Survey", "Interceptor"
  };
  for (auto cls : classes) {
    auto pos = hull.find(cls);
    if (pos != std::string::npos) {
      auto end = pos + cls.size();
      auto type = hull.substr(0, end);
      auto faction = (end < hull.size()) ? hull.substr(end + 1) : std::string{};
      return {type, faction};
    }
  }
  return {hull, {}};
}

struct BattleSummaryData {
  std::string playerName;
  std::string enemyName;
  std::string playerShip;
  std::string enemyShip;

  std::string format_body() const {
    auto format_side = [](const std::string& name, const std::string& ship) -> std::string {
      if (name.empty()) return "";
      if (ship.empty()) return name;
      if (name.starts_with("NPC#")) {
        auto [shipType, faction] = split_hull_class_faction(ship);
        if (!faction.empty())
          return fmt::format("{} ({} {})", shipType, faction, name);
        return fmt::format("{} ({})", ship, name);
      }
      return fmt::format("{} ({})", name, ship);
    };

    auto left  = format_side(playerName, playerShip);
    auto right = format_side(enemyName, enemyShip);
    if (left.empty() && right.empty()) return "";
    if (left.empty()) return right;
    if (right.empty()) return left;
    return left + " vs " + right;
  }
};

static BattleSummaryData build_battle_data(Il2CppObject* data)
{
  BattleSummaryData result;
  if (!data) return result;

  auto* brh = reinterpret_cast<BattleResultHeader*>(data);

  if (!seh_call([&] {
    auto* p = brh->get_PlayerUserProfile();
    auto* profile = reinterpret_cast<UserProfile*>(p);
    if (profile) {
      auto* nameStr = profile->Name;
      if (nameStr) result.playerName = to_string(nameStr);
      if (result.playerName.empty()) {
        auto locaId = profile->LocaId;
        if (locaId > 0)
          result.playerName = fmt::format("NPC#{}", locaId);
      }
    }
  })) spdlog::warn("[Toast] SEH: get_PlayerUserProfile crashed");

  if (!seh_call([&] {
    auto* e = brh->get_EnemyUserProfile();
    auto* profile = reinterpret_cast<UserProfile*>(e);
    if (profile) {
      auto* nameStr = profile->Name;
      if (nameStr) result.enemyName = to_string(nameStr);
      if (result.enemyName.empty()) {
        auto locaId = profile->LocaId;
        if (locaId > 0)
          result.enemyName = fmt::format("NPC#{}", locaId);
      }
    }
  })) spdlog::warn("[Toast] SEH: get_EnemyUserProfile crashed");

  if (!seh_call([&] {
    auto hid = brh->PlayerShipHullId;
    result.playerShip = resolve_hull_name(brh, hid);
  })) spdlog::warn("[Toast] SEH: PlayerShipHullId crashed");

  if (!seh_call([&] {
    auto hid = brh->EnemyShipHullId;
    result.enemyShip = resolve_hull_name(brh, hid);
  })) spdlog::warn("[Toast] SEH: EnemyShipHullId crashed");

  spdlog::info("[Toast] {} ({}) vs {} ({})", result.playerName, result.playerShip,
               result.enemyName, result.enemyShip);
  return result;
}

// ---- public entry point (called from detour thin wrapper) ----

void toast_handle_notification(Il2CppObject* toast_obj)
{
  auto* toast = reinterpret_cast<Toast*>(toast_obj);

  if (!s_localize_ltc) {
    spdlog::info("[Toast] s_localize_ltc is null, skipping");
    return;
  }

  auto* ltc = *reinterpret_cast<void**>(reinterpret_cast<char*>(toast) + 32);
  if (!ltc) return;

  auto* langMgr = LanguageManager::Instance();
  if (!langMgr) return;

  Il2CppString* resolved = nullptr;
  void* params[2] = { &resolved, ltc };
  Il2CppException* exc = nullptr;
  il2cpp_runtime_invoke(s_localize_ltc, langMgr, params, &exc);
  if (exc || !resolved) return;

  auto primary = to_string(resolved);
  spdlog::info("[Toast] State={}, Text={}", toast->get_State(), primary);

  auto* data = *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(toast) + 56);
  auto bsd = build_battle_data(data);

#if _WIN32
  auto* titleStr = toast_state_title(toast->get_State());
  if (!titleStr) return;

  std::string title = titleStr;
  auto body = bsd.format_body();
  if (body.empty()) body = primary;
  spdlog::info("[Toast] Notify: \"{}\" -- \"{}\"", title, body);
  show_system_notification(title.c_str(), body.c_str());
#endif
}
