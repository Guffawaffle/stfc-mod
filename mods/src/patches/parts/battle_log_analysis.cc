#include "battle_log_analysis.h"
#include "battle_log_analysis_data.h"
#include "battle_log_ui.h"
#include "config.h"
#include "str_utils.h"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <map>
#include <numeric>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace
{
using namespace battle_log_ui;
namespace data = battle_analysis;
using Clock    = std::chrono::steady_clock;

constexpr float kWidth = 1200, kHeight = 780;
constexpr color kBackground{0.025f, 0.075f, 0.115f, 1}, kPanel{0.045f, 0.12f, 0.18f, 1};
constexpr color kRaised{0.07f, 0.18f, 0.24f, 1}, kSelected{0.065f, 0.235f, 0.285f, 1};
constexpr color kText{0.89f, 0.95f, 0.98f, 1}, kMuted{0.59f, 0.71f, 0.78f, 1};
constexpr color kCyan{0.40f, 0.87f, 0.95f, 1}, kGold{0.94f, 0.76f, 0.42f, 1};
constexpr color kBlue{0.55f, 0.69f, 0.95f, 1}, kRed{0.96f, 0.55f, 0.60f, 1};
constexpr color kLine{0.13f, 0.25f, 0.32f, 1};

struct Rect {
  float x, y, w, h;
};
enum class Tab { Overview, Offense, Defense, Crew, Timeline, Compare };
enum class Op {
  Open,
  Close,
  Retry,
  Tab,
  Participant,
  Raw,
  Round,
  Page,
  Inspect,
  Pin,
  AllEvents,
  Roster,
  Compare,
  PairView
};
struct Action {
  Op          op;
  std::size_t value = 0;
};
struct Hit {
  Il2CppGCHandle object;
  Action         action;
};

struct Surface {
  Root                        object;
  std::vector<Il2CppGCHandle> children;
  std::vector<Hit>            hits;
  bool                        failed = false;
  float                       sx = 1, sy = 1;

  void clear()
  {
    if (Alive(object.get())) {
      bool no = false;
      Set(object.get(), "SetActive", &no);
      void* args[] = {object.get()};
      TryInvoke(IL2CppClassHelper(UnityObject()).GetMethodInfo("Destroy", 1), nullptr, args);
    }
    for (auto handle : children)
      il2cpp_gchandle_free(handle);
    children.clear();
    hits.clear();
    object.reset();
    failed = false;
  }
  Il2CppGCHandle keep(Il2CppObject* value)
  {
    if (!value) {
      failed = true;
      return 0;
    }
    auto handle = il2cpp_gchandle_new(value, false);
    if (handle)
      children.push_back(handle);
    else
      failed = true;
    return handle;
  }
};

Root         view, result, font, nativeLog, rounds, subRounds, events, nested;
Surface      entry, overlay;
data::Battle battle;
std::int64_t reportId   = 0;
int          roundIndex = 0, subIndex = 0, eventIndex = 0, nestedIndex = 0;
int          roundCount = 0, subCount = 0, eventCount = 0, nestedCount = 0;
std::size_t  readSteps   = 0;
bool         overlayOpen = false, ready = false, reading = false, raw = false, allEvents = false, dirty = true;
bool         pairView = false;
Tab          tab      = Tab::Overview;
std::size_t  selected = data::none, inspected = data::none, pinned = data::none, comparison = data::none;
int          roundFilter = 0, page = 0, rosterPage = 0;
std::unordered_map<std::int64_t, std::size_t> ships;
std::unordered_map<std::int64_t, std::string> hullNames, officerNames, technologyNames;
Clock::time_point                             nextDiscovery{}, nextProgress{};
int                                           uiGeneration = 0, pendingGeneration = 0;
bool                                          pendingClick = false;
vec2                                          pendingPoint{};
Root                                          pressedObject;

std::string Text(Il2CppObject* value)
{
  if (!value || std::strcmp(il2cpp_class_get_name(value->klass), "String")
      || std::strcmp(il2cpp_class_get_namespace(value->klass), "System"))
    return {};
  auto* string = reinterpret_cast<Il2CppString*>(value);
  if (il2cpp_string_length(string) > 2048)
    return {};
  return to_string(string);
}

// User names and localized data are content, never TMP formatting instructions.
std::string Escape(std::string_view input)
{
  std::string output;
  for (char c : input) {
    if (c == '<')
      output += "\xe2\x80\xb9";
    else if (c == '>')
      output += "\xe2\x80\xba";
    else if (c == '\r' || c == '\n' || c == '\t')
      output += ' ';
    else
      output += c;
  }
  return output;
}

std::string Number(long double n)
{
  if (n >= 1e12L)
    return fmt::format("{:.2f}T", n / 1e12L);
  if (n >= 1e9L)
    return fmt::format("{:.2f}B", n / 1e9L);
  if (n >= 1e6L)
    return fmt::format("{:.2f}M", n / 1e6L);
  if (n >= 1e3L)
    return fmt::format("{:.2f}K", n / 1e3L);
  return fmt::format("{:.0f}", n);
}

std::string Position(const data::Event& e)
{ return fmt::format("R{} / SR{}", e.round, e.sub_round); }

long double Amount(const data::Damage& d)
{ return raw ? d.raw() : d.landed(); }

int Count(Il2CppObject* list)
{
  int count = -1;
  return list && Value(list, "get_Count", count) && count >= 0 && count <= static_cast<int>(data::max_events) ? count
                                                                                                              : -1;
}

Il2CppObject* Item(Il2CppObject* list, int index)
{
  if (!list)
    return nullptr;
  void*         args[] = {&index};
  Il2CppObject* item   = nullptr;
  TryInvoke(IL2CppClassHelper(list->klass).GetMethodInfo("get_Item", 1), list, args, &item);
  return item;
}

std::string Localize(const char* identifier, const char* category, std::int64_t id)
{
  static auto* contextClass = Class("Assembly-CSharp", "Digit.Client.UI", "LocaleTextContext");
  static auto* utility      = Class("Assembly-CSharp", "Digit.Client.Localization", "LocaleUtilities");
  static auto* objectClass  = Class("mscorlib", "System", "Object");
  static auto* int64Class   = Class("mscorlib", "System", "Int64");
  if (!contextClass || !utility || !objectClass || !int64Class || !id)
    return {};
  Root context, array, number;
  context.reset(il2cpp_object_new(contextClass));
  array.reset(reinterpret_cast<Il2CppObject*>(il2cpp_array_new(objectClass, 1)));
  number.reset(il2cpp_value_box(int64Class, &id));
  std::string translated;
  if (context.get() && array.get() && number.get()) {
    void* ctor[] = {il2cpp_string_new(identifier), il2cpp_string_new(category)};
    auto* values = reinterpret_cast<Il2CppObject**>(reinterpret_cast<Il2CppArraySize*>(array.get())->vector);
    il2cpp_gc_wbarrier_set_field(array.get(), reinterpret_cast<void**>(values), number.get());
    void*         apply[]    = {array.get()};
    bool          no         = false;
    void*         localize[] = {context.get(), &no, &no};
    Il2CppObject* answer     = nullptr;
    if (TryInvoke(IL2CppClassHelper(contextClass).GetMethodInfo(".ctor", 2), context.get(), ctor)
        && TryInvoke(IL2CppClassHelper(contextClass).GetMethodInfo("ApplyIdentifierParameters", 1), context.get(),
                     apply)
        && TryInvoke(IL2CppClassHelper(utility).GetMethodInfo("Localize", 3), nullptr, localize, &answer))
      translated = Text(answer);
  }
  number.reset();
  array.reset();
  context.reset();
  if (translated.find('{') != std::string::npos || translated.find("_name_") != std::string::npos)
    translated.clear();
  return translated;
}

Il2CppObject* Spec(const char* method, std::int64_t id)
{
  auto* service = Field(result.get(), "_specService");
  if (!service)
    return nullptr;
  void*         args[] = {&id};
  Il2CppObject* spec   = nullptr;
  TryInvoke(IL2CppClassHelper(service->klass).GetMethodInfo(method, 1), service, args, &spec);
  return spec;
}

std::string SpecName(const char* method, const char* key, const char* category, std::int64_t id)
{
  auto*        spec = Spec(method, id);
  std::int64_t loca = 0;
  if (spec && Value(Get(spec, "get_IdRefs"), "get_LocaId", loca)) {
    auto name = Localize(key, category, loca);
    if (!name.empty())
      return name;
  }
  return spec ? Text(Get(spec, "get_Name")) : std::string{};
}

std::size_t Participant(std::int64_t ship, std::int64_t hull)
{
  if (auto found = ships.find(ship); found != ships.end()) {
    auto& p = battle.participants[found->second];
    if (!p.hull_id && hull) {
      p.hull_id   = hull;
      auto name   = SpecName("GetHull", "ship_name_{0}", "ships", hull);
      p.ship_name = name.empty() ? fmt::format("Hull #{}", hull) : name;
    }
    return found->second;
  }
  if (battle.participants.size() >= data::max_participants) {
    battle.failed = true;
    return data::none;
  }
  data::Participant p;
  p.ship               = ship;
  p.hull_id            = hull;
  void*         args[] = {&ship};
  Il2CppObject* fleet  = nullptr;
  if (!TryInvoke(IL2CppClassHelper(result.get()->klass).GetMethodInfo("GetFleetForShip", 1), result.get(), args, &fleet)
      || !fleet) {
    battle.failed = true;
    return data::none;
  }
  auto* profile = Get(fleet, "get_UserData");
  if (profile) {
    if (!Value(profile, "get_IsLocalPlayer", p.local)) {
      battle.failed = true;
      return data::none;
    }
    p.name = Text(Get(profile, "get_Name"));
    if (p.name.empty()) {
      std::int64_t loca = 0;
      if (Value(profile, "get_LocaId", loca))
        p.name = Localize("marauder_name_only_{0}", "navigation", loca);
    }
  }
  Value(fleet, "get_IsAlliedToPlayer", p.allied);
  if (auto found = hullNames.find(hull); found != hullNames.end())
    p.ship_name = found->second;
  else {
    p.ship_name = SpecName("GetHull", "ship_name_{0}", "ships", hull);
    if (p.ship_name.empty())
      p.ship_name = fmt::format("Hull #{}", hull);
    hullNames.emplace(hull, p.ship_name);
  }
  if (p.name.empty())
    p.name = p.ship_name;
  auto index = battle.participant(std::move(p));
  ships.emplace(ship, index);
  return index;
}

template <typename T> bool Backing(Il2CppObject* object, const char* property, Il2CppTypeEnum type, T& value)
{
  const auto name = fmt::format("<{}>k__BackingField", property);
  return FieldValue(object, name.c_str(), type, value);
}

void ReadEvent(Il2CppObject* object)
{
  if (!object) {
    battle.failed = true;
    return;
  }
  data::Event e;
  e.round                     = roundIndex + 1;
  e.sub_round                 = subIndex + 1;
  const std::string_view type = il2cpp_class_get_name(object->klass);
  if (type == "BattleAttackEvent") {
    std::int64_t actor = 0, target = 0, hull = 0, targetHull = 0;
    std::int64_t hp = 0, shield = 0, iso = 0, mit = 0, isoMit = 0, barrier = 0, crit = 0, isoCrit = 0;
    std::int64_t remaining = 0, remainingShield = 0;
    bool         charging = false;
    if (!Backing(object, "AttackingShip", IL2CPP_TYPE_I8, actor)
        || !Backing(object, "TargetShip", IL2CPP_TYPE_I8, target)
        || !Backing(object, "AttackingHullId", IL2CPP_TYPE_I8, hull)
        || !Backing(object, "TargetHullId", IL2CPP_TYPE_I8, targetHull) || !Value(object, "get_IsCharging", charging)) {
      battle.failed = true;
      return;
    }
    e.actor  = Participant(actor, hull);
    e.target = Participant(target, targetHull);
    e.kind   = charging ? data::Kind::Charging : data::Kind::Attack;
    if (!charging) {
      if (!Backing(object, "HPDamage", IL2CPP_TYPE_I8, hp) || !Backing(object, "ShieldDamage", IL2CPP_TYPE_I8, shield)
          || !Backing(object, "IsolyticDamage", IL2CPP_TYPE_I8, iso)
          || !Backing(object, "MitigatedDamage", IL2CPP_TYPE_I8, mit)
          || !Backing(object, "MitigatedIsolyticDamage", IL2CPP_TYPE_I8, isoMit)
          || !Backing(object, "MitigatedApexBarrier", IL2CPP_TYPE_I8, barrier)
          || !Backing(object, "MitigatedCriticalDamage", IL2CPP_TYPE_I8, crit)
          || !Backing(object, "MitigatedIsolyticCriticalDamage", IL2CPP_TYPE_I8, isoCrit)
          || !Backing(object, "IsCritical", IL2CPP_TYPE_BOOLEAN, e.critical)
          || !Backing(object, "WeaponID", IL2CPP_TYPE_I8, e.weapon)
          || !Backing(object, "RemainingHP", IL2CPP_TYPE_I8, remaining)
          || !Backing(object, "RemainingShield", IL2CPP_TYPE_I8, remainingShield)) {
        battle.failed = true;
        return;
      }
      e.damage          = data::reported_damage(hp, shield, iso, mit, isoMit, barrier, crit, isoCrit);
      e.destroyed       = hp > 0 && remaining == 0;
      e.shield_depleted = shield > 0 && remainingShield == 0;
    }
    nested.reset(Get(object, "get_OfficerEvents"));
    nestedCount = nested.get() ? Count(nested.get()) : 0;
    nestedIndex = 0;
    if (nestedCount < 0)
      battle.failed = true;
  } else {
    std::int64_t ship = 0, hull = 0;
    if (type == "OfficerEvent" || type == "ForbiddenTechEvent" || type == "HullRepairEvent") {
      if (!Backing(object, "ShipId", IL2CPP_TYPE_I8, ship) || !Backing(object, "ShipLocaId", IL2CPP_TYPE_I8, hull)) {
        battle.failed = true;
        return;
      }
      // ShipLocaId is a localization identifier rather than a hull ID. Fleet
      // lookup establishes identity; later attacks can supply the actual hull.
      e.actor = Participant(ship, 0);
      if (e.actor < battle.participants.size() && !battle.participants[e.actor].hull_id) {
        auto name = Localize("ship_name_{0}", "ships", hull);
        if (!name.empty())
          battle.participants[e.actor].ship_name = name;
      }
      float value = 0;
      if (!Backing(object, "AbilityValue", IL2CPP_TYPE_R4, value)) {
        battle.failed = true;
        return;
      }
      e.value = value;
      e.kind  = type == "OfficerEvent"         ? data::Kind::Officer
                : type == "ForbiddenTechEvent" ? data::Kind::Technology
                                               : data::Kind::Repair;
      if (e.kind != data::Kind::Repair) {
        if (!Backing(object, "AbilityId", IL2CPP_TYPE_I8, e.ability)
            || !Backing(object, "AbilityOwnerId", IL2CPP_TYPE_I8, e.owner)) {
          battle.failed = true;
          return;
        }
        if (e.kind == data::Kind::Officer) {
          if (auto found = officerNames.find(e.owner); found != officerNames.end())
            e.source = found->second;
          else {
            e.source = SpecName("GetOfficerSpec", "officer_name_{0}", "officer_names", e.owner);
            if (e.source.empty())
              e.source = fmt::format("Officer #{}", e.owner);
            officerNames.emplace(e.owner, e.source);
          }
        } else {
          if (auto found = technologyNames.find(e.owner); found != technologyNames.end())
            e.source = found->second;
          else {
            e.source = SpecName("GetForbiddenTechSpecById", "forbidden_tech_name_{0}", "forbidden_tech", e.owner);
            if (e.source.empty())
              e.source = fmt::format("Forbidden tech #{}", e.owner);
            technologyNames.emplace(e.owner, e.source);
          }
        }
        e.description = fmt::format("Ability #{}", e.ability);
      }
    } else {
      e.kind        = type == "BattleTauntEvent"      ? data::Kind::Taunt
                      : type == "BattleHullMeltEvent" ? data::Kind::HullMelt
                                                      : data::Kind::Other;
      e.description = std::string(type);
    }
  }
  if (!battle.append(std::move(e)))
    battle.failed = true;
}

void ResetData()
{
  nativeLog.reset();
  rounds.reset();
  subRounds.reset();
  events.reset();
  nested.reset();
  result.reset();
  battle = {};
  ships.clear();
  hullNames.clear();
  officerNames.clear();
  technologyNames.clear();
  readSteps  = 0;
  roundIndex = subIndex = eventIndex = nestedIndex = 0;
  roundCount = subCount = eventCount = nestedCount = 0;
  selected = inspected = pinned = comparison = data::none;
  reading = ready = false;
  page = rosterPage = roundFilter = 0;
  raw = allEvents = pairView = false;
  tab                        = Tab::Overview;
  reportId                   = 0;
  dirty                      = true;
}

void BeginRead()
{
  auto*         report = Get(result.get(), "get_BattleResult");
  Il2CppObject* log    = nullptr;
  if (report && TryInvoke(IL2CppClassHelper(report->klass).GetMethodInfo("GetBattleLog", 0), report, nullptr, &log))
    nativeLog.reset(log);
  rounds.reset(Get(nativeLog.get(), "get_BattleRounds"));
  roundCount = Count(rounds.get());
  // Resolve the fleets once through the client's existing lazy getters.
  Get(result.get(), "get_PlayerFleets");
  Get(result.get(), "get_TargetFleets");
  if (roundCount < 1 || roundCount > 100000)
    battle.failed = true;
  battle.rounds = std::max(0, roundCount);
  reading       = !battle.failed;
  ready         = battle.failed;
  dirty         = true;
}

void AdvanceRead()
{
  if (!reading || !overlayOpen)
    return;
  const auto deadline = Clock::now() + std::chrono::milliseconds(3);
  for (int batch = 0; batch < 128 && roundIndex < roundCount && !battle.failed; ++batch) {
    // Bound empty hierarchy traversal too, rather than just retained events.
    if (++readSteps > data::max_events * 8) {
      battle.failed = true;
      break;
    }
    if (nestedIndex < nestedCount) {
      ReadEvent(Item(nested.get(), nestedIndex++));
    } else if (!subRounds.get()) {
      auto* round = Item(rounds.get(), roundIndex);
      subRounds.reset(Get(round, "get_SubRounds"));
      subCount = Count(subRounds.get());
      if (subCount < 0)
        battle.failed = true;
    } else if (subIndex >= subCount) {
      subRounds.reset();
      events.reset();
      nested.reset();
      ++roundIndex;
      subIndex = eventIndex = nestedIndex = nestedCount = 0;
    } else if (!events.get()) {
      events.reset(Get(Item(subRounds.get(), subIndex), "get_RoundEvents"));
      eventCount = Count(events.get());
      eventIndex = 0;
      if (eventCount < 0)
        battle.failed = true;
    } else if (eventIndex >= eventCount) {
      events.reset();
      nested.reset();
      ++subIndex;
      nestedIndex = nestedCount = 0;
    } else {
      ReadEvent(Item(events.get(), eventIndex++));
    }
    if ((batch & 7) == 7 && Clock::now() >= deadline)
      break;
  }
  if (battle.failed || roundIndex >= roundCount) {
    ready   = true;
    reading = false;
    if (selected >= battle.participants.size())
      selected = battle.local();
    if (selected != data::none) {
      const auto attacks = battle.attacks(selected);
      if (!attacks.empty())
        inspected = attacks.front();
    }
    for (std::size_t i = 0; i < battle.participants.size(); ++i)
      if (i != selected) {
        comparison = i;
        break;
      }
    spdlog::info("Battle analysis: report {}, {} native rounds, {} events, {} participants, complete={}", reportId,
                 battle.rounds, battle.events.size(), battle.participants.size(), !battle.failed);
    dirty = true;
  } else if (Clock::now() >= nextProgress) {
    nextProgress = Clock::now() + std::chrono::milliseconds(250);
    dirty        = true;
  }
}

bool Place(Il2CppObject* transform, Rect r, float sx, float sy)
{
  vec2 top{0, 1}, min{r.x * sx, -(r.y + r.h) * sy}, max{(r.x + r.w) * sx, -r.y * sy};
  return Set(transform, "set_anchorMin", &top) && Set(transform, "set_anchorMax", &top)
         && Set(transform, "set_offsetMin", &min) && Set(transform, "set_offsetMax", &max);
}

Il2CppObject* Make(Surface& surface, Il2CppObject* parent, Rect r, const color* fill, bool intercept = false)
{
  Root  go;
  auto* transform = NewObject("CommunityMod_BattleAnalysisElement", parent, go);
  if (!transform || !Place(transform, r, surface.sx, surface.sy)) {
    surface.failed = true;
    if (Alive(go.get())) {
      void* args[] = {go.get()};
      TryInvoke(IL2CppClassHelper(UnityObject()).GetMethodInfo("Destroy", 1), nullptr, args);
    }
    go.reset();
    return nullptr;
  }
  auto* object = go.get();
  if (object) {
    surface.children.push_back(go.handle);
    go.handle = 0;
    if (fill) {
      static auto* imageClass = Class("UnityEngine.UI", "UnityEngine.UI", "Image");
      auto*        image      = WithType(TypeMethod(object->klass, "AddComponent"), object, imageClass);
      if (!image || !Set(image, "set_color", const_cast<color*>(fill)) || !Set(image, "set_raycastTarget", &intercept))
        surface.failed = true;
    }
    bool yes = true;
    if (!Set(object, "SetActive", &yes))
      surface.failed = true;
  }
  return object;
}

void Label(Surface& surface, Il2CppObject* parent, Rect r, const std::string& text, float size = 16, color ink = kText,
           int alignment = 0x101, bool wrap = false)
{
  auto*        object   = Make(surface, parent, r, nullptr);
  static auto* tmp      = Class("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
  auto*        label    = object ? WithType(TypeMethod(object->klass, "AddComponent"), object, tmp) : nullptr;
  float        fontSize = size * std::min(surface.sx, surface.sy);
  bool         no       = false;
  int          overflow = 1; // TMP ellipsis: keep long localized names inside their cells.
  if (!label || !font.get() || !Set(label, "set_font", font.get()) || !Set(label, "set_fontSize", &fontSize)
      || !Set(label, "set_color", &ink) || !Set(label, "set_alignment", &alignment)
      || !Set(label, "set_raycastTarget", &no) || !Set(label, "set_enableWordWrapping", &wrap)
      || !Set(label, "set_overflowMode", &overflow) || !Set(label, "set_text", il2cpp_string_new(text.c_str())))
    surface.failed = true;
}

void Label(Rect r, const std::string& text, float size = 16, color ink = kText, bool wrap = false)
{ Label(overlay, Get(overlay.object.get(), "get_transform"), r, text, size, ink, 0x101, wrap); }

void Box(Rect r, color fill = kBackground)
{ Make(overlay, Get(overlay.object.get(), "get_transform"), r, &fill); }

void Button(Surface& surface, Il2CppObject* parent, Rect r, const std::string& text, Action action, bool active = false,
            float size = 15)
{
  auto* object = Make(surface, parent, r, active ? &kSelected : &kRaised, true);
  if (!object)
    return;
  const auto handle = surface.keep(object);
  surface.hits.push_back({handle, action});
  Label(surface, Get(object, "get_transform"), {6, 3, r.w - 12, r.h - 6}, text, size, active ? kCyan : kText, 0x202);
}

void Button(Rect r, const std::string& text, Action action, bool active = false, float size = 15)
{ Button(overlay, Get(overlay.object.get(), "get_transform"), r, text, action, active, size); }

void Rule(float x, float y, float w)
{ Box({x, y, w, 1}, kLine); }

void Ledger(float x, float y, float width, std::string_view label, const std::string& value)
{
  Label({x, y, width * .64f, 24}, std::string(label), 14, kMuted);
  Label(overlay, Get(overlay.object.get(), "get_transform"), {x + width * .64f, y, width * .36f, 24}, value, 15, kText,
        0x104);
  Rule(x, y + 27, width);
}

void Metric(float x, std::string_view label, const std::string& value, const std::string& context)
{
  Label({x, 186, 278, 24}, std::string(label), 15, kMuted);
  Label({x, 213, 278, 42}, value, 32);
  Label({x, 256, 278, 24}, context, 13, kMuted);
}

void Pager(int count, int perPage, float y = 708)
{
  const int pages = std::max(1, (count + perPage - 1) / perPage);
  page            = std::clamp(page, 0, pages - 1);
  Label({254, y + 4, 380, 28}, fmt::format("Page {} / {}  |  {} entries", page + 1, pages, count), 14, kMuted);
  if (page > 0)
    Button({956, y, 98, 32}, "Previous", {Op::Page, 0});
  if (page + 1 < pages)
    Button({1062, y, 98, 32}, "Next", {Op::Page, 1});
}

void RoundControls(float y)
{
  Button({254, y, 102, 30}, roundFilter ? fmt::format("Round {}", roundFilter) : "All rounds", {Op::Round, 0},
         !roundFilter, 13);
  if (roundFilter > 1)
    Button({364, y, 68, 30}, "< R", {Op::Round, static_cast<std::size_t>(roundFilter - 1)}, false, 13);
  if (roundFilter < battle.rounds)
    Button({440, y, 80, 30}, "Next R >", {Op::Round, static_cast<std::size_t>(roundFilter + 1)}, false, 13);
}

void Overview(const data::Participant& p)
{
  Metric(254, raw ? "Raw damage dealt" : "Damage landed", Number(Amount(p.outgoing)),
         fmt::format("{} shots fired", p.outgoing.shots));
  Metric(564, raw ? "Raw incoming damage" : "Damage taken", Number(Amount(p.incoming)),
         fmt::format("{} incoming hits", p.incoming.shots));
  Metric(874, "Critical shots", fmt::format("{} / {}", p.outgoing.critical, p.outgoing.shots),
         fmt::format("{:.1f}% critical", p.outgoing.critical_percent()));
  Rule(254, 288, 906);
  Box({254, 308, 438, 244});
  Box({710, 308, 450, 244});
  Label({270, 320, 406, 26}, "WHERE YOUR DAMAGE WENT", 16);
  Label({270, 357, 190, 24}, "RAW DAMAGE", 13, kMuted);
  Label({496, 357, 180, 24}, "LANDED", 13, kMuted);
  Label({270, 384, 200, 40}, Number(p.outgoing.raw()), 29);
  Label({496, 384, 180, 40}, Number(p.outgoing.landed()), 29);
  const auto  landed = p.outgoing.landed();
  const float hull   = landed > 0 ? static_cast<float>(p.outgoing.hull / landed) : 0;
  Box({270, 434, 406, 12}, kRaised);
  if (hull > 0)
    Box({270, 434, 406 * hull, 12}, kCyan);
  if (landed > 0 && hull < 1)
    Box({270 + 406 * hull, 434, 406 * (1 - hull), 12}, kBlue);
  Ledger(270, 462, 406, "Hull damage", Number(p.outgoing.hull));
  Ledger(270, 496, 406, "Shield damage", Number(p.outgoing.shield));
  Label({726, 320, 418, 26}, "YOUR FIRING WINDOW", 16);
  std::map<int, long double> totals;
  for (const auto& e : battle.events)
    if (e.kind == data::Kind::Attack && e.actor == selected)
      totals[e.round] += Amount(e.damage);
  long double maximum = 1;
  for (const auto& [r, value] : totals)
    maximum = std::max(maximum, value);
  int row = 0;
  for (const auto& [r, value] : totals) {
    if (row >= 4)
      break;
    const float y = 362 + row++ * 38.f;
    Label({726, y, 44, 24}, fmt::format("R{}", r), 14, kMuted);
    Box({774, y + 7, 250, 9}, kRaised);
    Box({774, y + 7, 250 * static_cast<float>(value / maximum), 9}, raw ? kGold : kCyan);
    Label({1032, y, 112, 26}, Number(value), 14);
  }
  Label({726, 517, 418, 26}, fmt::format("{} active rounds / {} battle rounds", p.active_rounds.size(), battle.rounds),
        13, kMuted);
  Ledger(254, 574, 434, "Raw / shot", Number(p.outgoing.average(true)));
  Ledger(726, 574, 434, "Landed / shot", Number(p.outgoing.average()));
  Ledger(254, 612, 434, "Landed / battle round", Number(battle.rounds ? p.outgoing.landed() / battle.rounds : 0));
  Ledger(726, 612, 434, "Repairs (reported)", Number(p.repairs));
  const auto best = battle.peak_attack(selected, raw);
  if (best != data::none)
    Button({254, 661, 430, 34}, "Biggest outgoing hit  |  " + Position(battle.events[best]), {Op::Inspect, best}, true,
           14);
  Label({710, 661, 450, 52},
        "Weapon damage may include overkill. Raw, landed, and net hull loss are separate measures.", 13, kMuted, true);
}

void AttackDetail(std::size_t index, float y)
{
  if (index >= battle.events.size())
    return;
  const auto& e = battle.events[index];
  Label({254, y, 680, 30},
        fmt::format("{}  |  Event #{}  |  {}", Position(e), index + 1, e.critical ? "CRITICAL" : "Normal hit"), 17);
  Button({984, y, 176, 30}, pinned == index ? "Unpin attack" : "Pin to compare", {Op::Pin, index}, pinned == index, 13);
  const auto& a = battle.participants[e.actor];
  const auto& t = battle.participants[e.target];
  Label({254, y + 32, 906, 26}, Escape(a.name) + "  ->  " + Escape(t.name) + fmt::format("  |  Weapon #{}", e.weapon),
        14, kMuted);
  Ledger(254, y + 66, 434, "Raw standard", Number(e.damage.standard));
  Ledger(726, y + 66, 434, "Raw isolytic", Number(e.damage.isolytic));
  Ledger(254, y + 100, 434, "Hull damage", Number(e.damage.hull));
  Ledger(726, y + 100, 434, "Shield damage", Number(e.damage.shield));
  Ledger(254, y + 134, 434, "Standard mitigated", Number(e.damage.mitigated_standard));
  Ledger(726, y + 134, 434, "Isolytic mitigated", Number(e.damage.mitigated_isolytic));
  if (pinned != data::none && pinned != index && pinned < battle.events.size()) {
    const auto& baseline = battle.events[pinned];
    const auto  before = Amount(baseline.damage), after = Amount(e.damage);
    const auto  delta = before > 0 ? fmt::format("{:+.1f}%", 100 * (after / before - 1)) : "zero baseline";
    Label({254, y + 173, 906, 36},
          fmt::format("PINNED {} #{}: {}   |   SELECTED: {}   |   {} {} damage", Position(baseline), pinned + 1,
                      Number(before), Number(after), delta, raw ? "raw" : "landed"),
          14, kCyan, true);
  }
}

void Offense(const data::Participant& p)
{
  Metric(254, raw ? "Raw damage dealt" : "Damage landed", Number(Amount(p.outgoing)), "Whole-battle total");
  Metric(564, raw ? "Raw / shot" : "Landed / shot", Number(p.outgoing.average(raw)),
         fmt::format("{} shots fired", p.outgoing.shots));
  Metric(874, "Critical rate", fmt::format("{:.1f}%", p.outgoing.critical_percent()),
         fmt::format("{} critical / {} normal", p.outgoing.critical, p.outgoing.shots - p.outgoing.critical));
  Rule(254, 288, 906);
  RoundControls(302);
  auto          indices = battle.attacks(selected, roundFilter);
  constexpr int perPage = 8;
  const int     pages   = std::max(1, (static_cast<int>(indices.size()) + perPage - 1) / perPage);
  page                  = std::clamp(page, 0, pages - 1);
  if (std::find(indices.begin(), indices.end(), inspected) == indices.end())
    inspected =
        indices.empty() ? data::none : indices[std::min(indices.size() - 1, static_cast<std::size_t>(page * perPage))];
  long double maximum = 1;
  for (auto i : indices)
    maximum = std::max(maximum, Amount(battle.events[i].damage));
  const auto  start = static_cast<std::size_t>(page * perPage);
  const auto  end   = std::min(indices.size(), start + perPage);
  const float slot  = 906.f / perPage;
  for (auto j = start; j < end; ++j) {
    const auto  i = indices[j];
    const auto& e = battle.events[i];
    const float x = 254 + (j - start) * slot;
    Button({x, 348, slot - 8, 132}, "", {Op::Inspect, i}, i == inspected);
    if (e.critical)
      Label({x + 7, 351, slot - 22, 22}, "CRIT", 12, kGold);
    const float height = std::max(2.f, static_cast<float>(Amount(e.damage) / maximum) * 59);
    Box({x + slot * .31f, 428 - height, slot * .32f, height}, e.critical ? kGold : kCyan);
    Label({x + 7, 435, slot - 22, 22}, Position(e), 12);
    Label({x + 7, 458, slot - 22, 20}, fmt::format("#{}", i + 1), 11, kMuted);
  }
  if (indices.empty())
    Label({254, 362, 906, 60}, "No shots fired in this round.", 18, kMuted);
  AttackDetail(inspected, 494);
  Pager(static_cast<int>(indices.size()), perPage, 731);
}

void Defense(const data::Participant& p)
{
  const auto& d = p.incoming;
  Metric(254, "Damage landed on this ship", Number(d.landed()), fmt::format("{} incoming hits", d.shots));
  Metric(564, "Damage to hull", Number(d.hull), "Logged attack damage");
  Metric(874, "Reported repairs", Number(p.repairs), fmt::format("{} repair events", p.repair_events));
  Rule(254, 288, 906);
  Box({254, 308, 438, 270});
  Box({710, 308, 450, 270});
  Label({270, 320, 406, 28}, "INCOMING DAMAGE FLOW", 16);
  Label({270, 356, 406, 45}, Number(d.raw()) + " raw", 29);
  const auto r       = d.raw();
  const auto missing = std::max(0.L, r - d.landed());
  Box({270, 414, 406, 18}, kRaised);
  if (r > 0) {
    const float a = static_cast<float>(missing / r), b = static_cast<float>(d.shield / r),
                c = static_cast<float>(d.hull / r);
    if (a > 0)
      Box({270, 414, 406 * a, 18}, kGold);
    if (b > 0)
      Box({270 + 406 * a, 414, 406 * b, 18}, kBlue);
    if (c > 0)
      Box({270 + 406 * (a + b), 414, 406 * c, 18}, kCyan);
  }
  Ledger(270, 454, 406, "Raw that did not land", Number(missing));
  Ledger(270, 488, 406, "Damage to shields", Number(d.shield));
  Ledger(270, 522, 406, "Damage to hull", Number(d.hull));
  Label({726, 320, 418, 28}, "REPORTED MITIGATION", 16);
  Ledger(726, 365, 418, "Standard", Number(d.mitigated_standard));
  Ledger(726, 403, 418, "Isolytic", Number(d.mitigated_isolytic));
  Ledger(726, 441, 418, "Apex barrier", Number(d.barrier));
  Ledger(726, 479, 418, "Critical", Number(d.critical_mitigation));
  Label({726, 522, 418, 48}, "Categories can overlap; they are not additive mitigation percentages.", 13, kMuted, true);
  Label({254, 600, 906, 30}, "INCOMING SOURCES", 16);
  std::map<std::size_t, data::Damage> sources;
  for (const auto& e : battle.events)
    if (e.kind == data::Kind::Attack && e.target == selected)
      data::add(sources[e.actor], e.damage, e.critical);
  int row = 0;
  for (const auto& [actor, damage] : sources) {
    if (row >= 3)
      break;
    Ledger(254, 638 + row++ * 34, 906, Escape(battle.participants[actor].name),
           Number(damage.landed()) + fmt::format(" / {} hits", damage.shots));
  }
}

const char* KindName(data::Kind kind)
{
  switch (kind) {
    case data::Kind::Attack:
      return "Attack";
    case data::Kind::Charging:
      return "Charging weapons";
    case data::Kind::Officer:
      return "Officer ability";
    case data::Kind::Technology:
      return "Forbidden technology";
    case data::Kind::Repair:
      return "Hull repair";
    case data::Kind::Taunt:
      return "Taunt";
    case data::Kind::HullMelt:
      return "Hull melt";
    default:
      return "Other event";
  }
}

void Crew(const data::Participant& p)
{
  Label({254, 190, 906, 34}, "OBSERVED OFFICER & TECHNOLOGY ACTIVATIONS", 20);
  Label({254, 232, 906, 45},
        "Activations retain their native event positions, including triggers between individual shots.", 15, kMuted,
        true);
  struct Ability {
    std::string source, description;
    int         count = 0;
    std::size_t first = 0;
  };
  std::map<std::tuple<data::Kind, std::int64_t, std::int64_t>, Ability> abilities;
  for (std::size_t i = 0; i < battle.events.size(); ++i) {
    const auto& e = battle.events[i];
    if (e.actor == selected && (e.kind == data::Kind::Officer || e.kind == data::Kind::Technology)) {
      auto& a = abilities[{e.kind, e.owner, e.ability}];
      if (!a.count) {
        a.source      = e.source;
        a.description = e.description;
        a.first       = i;
      }
      ++a.count;
    }
  }
  const int perPage = 7;
  int       row = 0, index = 0;
  const int count = static_cast<int>(abilities.size());
  page            = std::clamp(page, 0, std::max(0, (count - 1) / perPage));
  for (const auto& [key, a] : abilities) {
    if (index++ < page * perPage || row >= perPage)
      continue;
    const float y = 292 + row++ * 53.f;
    Label({254, y, 610, 25}, Escape(a.source), 16);
    Label({254, y + 25, 610, 24}, Escape(a.description), 13, kMuted);
    Button({894, y + 3, 266, 34}, fmt::format("{} events  |  {}", a.count, Position(battle.events[a.first])),
           {Op::Inspect, a.first}, false, 13);
    Rule(254, y + 51, 906);
  }
  if (abilities.empty())
    Label({254, 302, 906, 60}, "No officer or technology activations recorded for this ship.", 17, kMuted);
  Label({254, 682, 906, 26}, "These are observed activations, not inferred crew slots or predicted effects.", 13,
        kMuted);
  Pager(count, perPage, 731);
}

void Timeline(const data::Participant& p)
{
  RoundControls(190);
  Button({856, 190, 304, 30}, allEvents ? "Whole battle" : "Selected ship", {Op::AllEvents}, allEvents, 14);
  const auto    indices = battle.timeline(selected, roundFilter, allEvents);
  constexpr int perPage = 8;
  page                  = std::clamp(page, 0, std::max(0, (static_cast<int>(indices.size()) - 1) / perPage));
  for (std::size_t j = page * perPage; j < std::min(indices.size(), static_cast<std::size_t>((page + 1) * perPage));
       ++j) {
    const auto  i = indices[j];
    const auto& e = battle.events[i];
    const float y = 241 + (j - page * perPage) * 54.f;
    Label({254, y, 140, 26}, Position(e), 14, kMuted);
    Label({254, y + 25, 140, 22}, fmt::format("Event #{}", i + 1), 12, kMuted);
    std::string label = KindName(e.kind);
    if (e.actor < battle.participants.size())
      label = Escape(battle.participants[e.actor].name) + " | " + label;
    if (e.kind == data::Kind::Attack) {
      Button({402, y, 588, 27}, label + (e.critical ? " / CRITICAL" : ""), {Op::Inspect, i}, i == inspected, 13);
      Label({402, y + 28, 588, 22},
            Escape(battle.participants[e.target].name) + (e.destroyed ? " / destroyed" : "")
                + (e.shield_depleted ? " / shields depleted" : ""),
            12, e.destroyed ? kRed : kMuted);
      Label({1004, y, 156, 30}, Number(Amount(e.damage)), 18);
    } else {
      Label({402, y, 600, 26}, label, 14);
      Label({402, y + 26, 600, 24}, Escape(e.source.empty() ? e.description : e.source + " / " + e.description), 12,
            kMuted);
      if (e.kind == data::Kind::Repair)
        Label({1004, y, 156, 30}, Number(e.value), 18, kCyan);
    }
    Rule(254, y + 52, 906);
  }
  if (indices.empty())
    Label({254, 260, 906, 60}, "No events in this selection.", 18, kMuted);
  Label({254, 693, 906, 26}, "Native round / sub-round order. Select an attack to inspect its complete breakdown.", 13,
        kMuted);
  Pager(static_cast<int>(indices.size()), perPage, 731);
}

void Compare(const data::Participant& p)
{
  Button({254, 190, 190, 32}, "Armada table", {Op::PairView, 0}, !pairView, 14);
  Button({454, 190, 190, 32}, "Two ships", {Op::PairView, 1}, pairView, 14);
  if (pairView) {
    std::vector<std::size_t> alternatives;
    for (std::size_t i = 0; i < battle.participants.size(); ++i)
      if (i != selected)
        alternatives.push_back(i);
    if (alternatives.empty()) {
      Label({254, 252, 906, 60}, "This report has no second combatant to compare.", 20, kMuted);
      return;
    }
    auto found = std::find(alternatives.begin(), alternatives.end(), comparison);
    if (found == alternatives.end()) {
      found      = alternatives.begin();
      comparison = *found;
    }
    const auto  index = static_cast<std::size_t>(found - alternatives.begin());
    const auto& other = battle.participants[comparison];
    Label({254, 246, 430, 28}, Escape(p.name) + (p.local ? " / YOU" : ""), 20, kCyan);
    Label({254, 276, 430, 24}, Escape(p.ship_name), 14, kMuted);
    Label({730, 246, 334, 28}, Escape(other.name), 20, kGold);
    Label({730, 276, 334, 24}, Escape(other.ship_name), 14, kMuted);
    Button({1068, 246, 42, 32}, "<",
           {Op::Compare, alternatives[(index + alternatives.size() - 1) % alternatives.size()]});
    Button({1118, 246, 42, 32}, ">", {Op::Compare, alternatives[(index + 1) % alternatives.size()]});
    const auto row = [&](float y, std::string_view name, const std::string& a, const std::string& b) {
      Ledger(254, y, 430, name, a);
      Ledger(730, y, 430, name, b);
    };
    row(326, raw ? "Raw damage dealt" : "Damage landed", Number(Amount(p.outgoing)), Number(Amount(other.outgoing)));
    row(366, "Shots fired", fmt::format("{}", p.outgoing.shots), fmt::format("{}", other.outgoing.shots));
    row(406, "Average per shot", Number(p.outgoing.average(raw)), Number(other.outgoing.average(raw)));
    row(446, "Critical rate", fmt::format("{:.1f}%", p.outgoing.critical_percent()),
        fmt::format("{:.1f}%", other.outgoing.critical_percent()));
    row(486, raw ? "Raw incoming" : "Damage taken", Number(Amount(p.incoming)), Number(Amount(other.incoming)));
    row(526, "Hull repaired", Number(p.repairs), Number(other.repairs));
    row(566, "Rounds firing", fmt::format("{} / {}", p.active_rounds.size(), battle.rounds),
        fmt::format("{} / {}", other.active_rounds.size(), battle.rounds));
    Label({254, 639, 906, 54},
          "Select the left ship in the roster; use the arrows to choose its comparison. These are observed results "
          "against each ship's actual targets.",
          15, kMuted, true);
    return;
  }
  std::vector<std::size_t> order(battle.participants.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [](auto a, auto b) {
    return Amount(battle.participants[a].outgoing) > Amount(battle.participants[b].outgoing);
  });
  const int perPage = 8;
  page              = std::clamp(page, 0, std::max(0, (static_cast<int>(order.size()) - 1) / perPage));
  Label({254, 235, 360, 25}, "PARTICIPANT", 12, kMuted);
  Label({642, 235, 180, 25}, raw ? "RAW DAMAGE" : "LANDED", 12, kMuted);
  Label({840, 235, 108, 25}, "SHOTS", 12, kMuted);
  Label({962, 235, 94, 25}, "CRIT", 12, kMuted);
  Label({1060, 235, 100, 25}, "ACTIVE R", 12, kMuted);
  for (std::size_t j = page * perPage; j < std::min(order.size(), static_cast<std::size_t>((page + 1) * perPage));
       ++j) {
    const auto  index = order[j];
    const auto& a     = battle.participants[index];
    const float y     = 274 + (j - page * perPage) * 48.f;
    Button({254, y, 370, 39}, Escape(a.name) + (a.local ? " / YOU" : ""), {Op::Participant, index}, index == selected,
           14);
    Label({642, y + 6, 180, 30}, Number(Amount(a.outgoing)), 18);
    Label({840, y + 6, 108, 30}, fmt::format("{}", a.outgoing.shots), 17);
    Label({962, y + 6, 94, 30}, fmt::format("{:.0f}%", a.outgoing.critical_percent()), 17);
    Label({1060, y + 6, 100, 30}, fmt::format("{} / {}", a.active_rounds.size(), battle.rounds), 16, kMuted);
  }
  Label({254, 681, 906, 36}, "Each row represents one ship; allies and opponents remain separate combatants.", 13,
        kMuted);
  Pager(static_cast<int>(order.size()), perPage, 731);
}

bool CreateOverlay()
{
  overlay.clear();
  ++uiGeneration;
  auto* parent    = Get(view.get(), "get_transform");
  auto* transform = NewObject("CommunityMod_BattleAnalysis", parent, overlay.object);
  vec2  zero{0, 0}, one{1, 1}, min{12, 12}, max{-12, -12};
  Rect  rect{};
  if (!transform || !Set(transform, "set_anchorMin", &zero) || !Set(transform, "set_anchorMax", &one)
      || !Set(transform, "set_offsetMin", &min) || !Set(transform, "set_offsetMax", &max)
      || !Value(transform, "get_rect", rect) || rect.w < 600 || rect.h < 350)
    return false;
  overlay.sx              = rect.w / kWidth;
  overlay.sy              = rect.h / kHeight;
  static auto* imageClass = Class("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*        background =
      WithType(TypeMethod(overlay.object.get()->klass, "AddComponent"), overlay.object.get(), imageClass);
  bool yes = true;
  if (!background || !Set(background, "set_color", const_cast<color*>(&kPanel))
      || !Set(background, "set_raycastTarget", &yes))
    return false;
  Box({0, 0, kWidth, 3}, kCyan);
  Label({24, 18, 680, 22}, "AFTER-ACTION INTELLIGENCE", 13, kMuted);
  Label({24, 44, 680, 43}, "BATTLE ANALYSIS", 30);
  Button({888, 24, 82, 34}, "Landed", {Op::Raw, 0}, !raw, 14);
  Button({978, 24, 72, 34}, "Raw", {Op::Raw, 1}, raw, 14);
  Button({1060, 24, 116, 34}, "Close", {Op::Close}, false, 15);
  Rule(0, 97, kWidth);
  Box({0, 98, 228, kHeight - 98}, kBackground);
  Rule(228, 98, 1);
  Label({16, 112, 196, 24}, "COMBATANTS", 13, kMuted);
  std::vector<std::size_t> roster(battle.participants.size());
  std::iota(roster.begin(), roster.end(), 0);
  std::stable_sort(roster.begin(), roster.end(),
                   [](auto a, auto b) { return battle.participants[a].local > battle.participants[b].local; });
  constexpr int rosterSize = 8;
  rosterPage               = std::clamp(rosterPage, 0, std::max(0, (static_cast<int>(roster.size()) - 1) / rosterSize));
  for (std::size_t j = rosterPage * rosterSize;
       j < std::min(roster.size(), static_cast<std::size_t>((rosterPage + 1) * rosterSize)); ++j) {
    const auto  i = roster[j];
    const auto& p = battle.participants[i];
    const float y = 148 + (j - rosterPage * rosterSize) * 65.f;
    Button({12, y, 204, 55}, "", {Op::Participant, i}, selected == i);
    Label({23, y + 5, 182, 25}, Escape(p.name) + (p.local ? " / YOU" : ""), 14, selected == i ? kCyan : kText);
    Label({23, y + 30, 182, 22}, Escape(p.ship_name), 12, kMuted);
  }
  if (rosterPage > 0)
    Button({12, 701, 94, 30}, "Previous", {Op::Roster, 0}, false, 12);
  if ((rosterPage + 1) * rosterSize < static_cast<int>(roster.size()))
    Button({118, 701, 98, 30}, "Next", {Op::Roster, 1}, false, 12);
  const char* names[] = {"Overview", "Offense", "Defense", "Crew & effects", "Timeline", "Compare"};
  for (int i = 0; i < 6; ++i)
    Button({254 + i * 151.f, 108, 141, 32}, names[i], {Op::Tab, static_cast<std::size_t>(i)},
           tab == static_cast<Tab>(i), 14);
  Rule(254, 150, 906);
  if (!ready) {
    Label({254, 186, 906, 60}, fmt::format("Reading native battle data... {} events", battle.events.size()), 24,
          kMuted);
  } else if (battle.failed) {
    Label({254, 186, 906, 100},
          "Analysis unavailable: this report could not be read completely. The native Battle Log remains available.",
          22, kRed, true);
    Button({254, 302, 210, 38}, "Retry analysis", {Op::Retry});
  } else if (selected >= battle.participants.size()) {
    Label({254, 186, 906, 100}, "No local ship identified. Select a combatant to inspect its recorded battle data.", 22,
          kMuted, true);
  } else {
    const auto& p = battle.participants[selected];
    Label({254, 155, 906, 28},
          Escape(p.name) + "  /  " + Escape(p.ship_name)
              + (p.destroyed_round ? fmt::format("  /  Destroyed R{}", p.destroyed_round) : ""),
          17, p.destroyed_round ? kRed : kText);
    switch (tab) {
      case Tab::Overview:
        Overview(p);
        break;
      case Tab::Offense:
        Offense(p);
        break;
      case Tab::Defense:
        Defense(p);
        break;
      case Tab::Crew:
        Crew(p);
        break;
      case Tab::Timeline:
        Timeline(p);
        break;
      case Tab::Compare:
        Compare(p);
        break;
    }
  }
  Rule(0, 771, kWidth);
  Label({16, 748, 208, 22}, fmt::format("Report #{}", reportId), 11, kMuted);
  if (overlay.failed || !Set(overlay.object.get(), "SetActive", &yes))
    return false;
  dirty = false;
  return true;
}

bool CreateEntry()
{
  entry.clear();
  auto* native    = Field(view.get(), "_showDetailedLogButton");
  auto* transform = Get(native, "get_transform");
  auto* parent    = Get(transform, "get_parent");
  vec2  anchorMin{}, anchorMax{}, offsetMin{}, offsetMax{};
  Rect  bounds{};
  if (!Alive(native) || !parent || !Value(transform, "get_anchorMin", anchorMin)
      || !Value(transform, "get_anchorMax", anchorMax) || !Value(transform, "get_offsetMin", offsetMin)
      || !Value(transform, "get_offsetMax", offsetMax) || !Value(transform, "get_rect", bounds) || bounds.w < 50
      || bounds.h < 15)
    return false;
  auto* own = NewObject("CommunityMod_BattleAnalysisButton", parent, entry.object);
  offsetMin.x -= bounds.w + 12;
  offsetMax.x -= bounds.w + 12;
  if (!own || !Set(own, "set_anchorMin", &anchorMin) || !Set(own, "set_anchorMax", &anchorMax)
      || !Set(own, "set_offsetMin", &offsetMin) || !Set(own, "set_offsetMax", &offsetMax))
    return false;
  static auto* imageClass = Class("UnityEngine.UI", "UnityEngine.UI", "Image");
  auto*        image = WithType(TypeMethod(entry.object.get()->klass, "AddComponent"), entry.object.get(), imageClass);
  bool         yes   = true;
  if (!image || !Set(image, "set_color", const_cast<color*>(&kSelected)) || !Set(image, "set_raycastTarget", &yes))
    return false;
  entry.sx = entry.sy = 1;
  Label(entry, own, {4, 2, bounds.w - 8, bounds.h - 4}, "BATTLE ANALYSIS", std::min(25.f, bounds.h * .43f), kCyan,
        0x202);
  entry.hits.push_back({entry.keep(entry.object.get()), {Op::Open}});
  return !entry.failed && Set(entry.object.get(), "SetActive", &yes);
}

Il2CppObject* RaycastAt(vec2 point)
{
  static auto* eventSystem  = Class("UnityEngine.UI", "UnityEngine.EventSystems", "EventSystem");
  static auto* pointerClass = Class("UnityEngine.UI", "UnityEngine.EventSystems", "PointerEventData");
  if (!eventSystem || !pointerClass)
    return nullptr;
  Il2CppObject* current = nullptr;
  if (!TryInvoke(IL2CppClassHelper(eventSystem).GetMethodInfo("get_current", 0), nullptr, nullptr, &current))
    return nullptr;
  auto* method    = IL2CppClassHelper(eventSystem).GetMethodInfo("RaycastAll", 2);
  auto* listClass = method ? il2cpp_class_from_type(il2cpp_method_get_param(method, 1)) : nullptr;
  if (!current || !listClass)
    return nullptr;
  // A fresh, correctly inflated List<RaycastResult> avoids touching Unity's
  // shared raycast caches and works with both legacy and Input System modules.
  Root pointer, results;
  pointer.reset(il2cpp_object_new(pointerClass));
  results.reset(il2cpp_object_new(listClass));
  void*         ctor[] = {current};
  void*         args[] = {pointer.get(), results.get()};
  Il2CppObject* target = nullptr;
  if (pointer.get() && results.get()
      && TryInvoke(IL2CppClassHelper(pointerClass).GetMethodInfo(".ctor", 1), pointer.get(), ctor)
      && TryInvoke(IL2CppClassHelper(listClass).GetMethodInfo(".ctor", 0), results.get(), nullptr)
      && Set(pointer.get(), "set_position", &point) && TryInvoke(method, current, args) && Count(results.get()) > 0)
    target = Field(Item(results.get(), 0), "m_GameObject");
  results.reset();
  pointer.reset();
  return target;
}

bool Inside(Il2CppObject* object, vec2 point)
{
  static auto* canvasClass = Class("UnityEngine.UIModule", "UnityEngine", "Canvas");
  static auto* utility     = Class("UnityEngine.UIModule", "UnityEngine", "RectTransformUtility");
  if (!canvasClass || !utility)
    return false;
  bool          no           = false;
  void*         canvasArgs[] = {il2cpp_type_get_object(il2cpp_class_get_type(canvasClass)), &no};
  Il2CppObject* canvas       = nullptr;
  if (!TryInvoke(IL2CppClassHelper(object->klass).GetMethodInfo("GetComponentInParent", 2), object, canvasArgs,
                 &canvas))
    return false;
  void*         args[] = {Get(object, "get_transform"), &point, Get(canvas, "get_worldCamera")};
  Il2CppObject* inside = nullptr;
  return TryInvoke(IL2CppClassHelper(utility).GetMethodInfo("RectangleContainsScreenPoint", 3), nullptr, args, &inside)
         && inside && *static_cast<bool*>(il2cpp_object_unbox(inside));
}

void Apply(Action action)
{
  switch (action.op) {
    case Op::Open:
      overlayOpen = true;
      if (!ready && !reading)
        BeginRead();
      break;
    case Op::Close:
      overlayOpen = false;
      overlay.clear();
      ++uiGeneration;
      break;
    case Op::Retry: {
      Root binding;
      binding.reset(result.get());
      const auto id = reportId;
      ResetData();
      result.reset(binding.get());
      binding.reset();
      reportId = id;
      BeginRead();
      break;
    }
    case Op::Tab:
      tab  = static_cast<Tab>(action.value);
      page = 0;
      break;
    case Op::Participant:
      selected    = action.value;
      page        = 0;
      roundFilter = 0;
      inspected   = data::none;
      break;
    case Op::Raw:
      raw = action.value != 0;
      break;
    case Op::Round:
      roundFilter = static_cast<int>(action.value);
      page        = 0;
      break;
    case Op::Page:
      page = std::max(0, page + (action.value ? 1 : -1));
      if (tab == Tab::Offense)
        inspected = data::none;
      break;
    case Op::Pin:
      pinned = pinned == action.value ? data::none : action.value;
      break;
    case Op::AllEvents:
      allEvents = !allEvents;
      page      = 0;
      break;
    case Op::Roster:
      rosterPage = std::max(0, rosterPage + (action.value ? 1 : -1));
      break;
    case Op::Compare:
      comparison = action.value;
      break;
    case Op::PairView:
      pairView = action.value != 0;
      page     = 0;
      break;
    case Op::Inspect:
      if (action.value < battle.events.size()) {
        inspected     = action.value;
        const auto& e = battle.events[inspected];
        if (e.kind == data::Kind::Attack) {
          selected           = e.actor;
          tab                = Tab::Offense;
          roundFilter        = e.round;
          const auto indices = battle.attacks(selected, roundFilter);
          const auto i       = std::find(indices.begin(), indices.end(), inspected);
          page               = i == indices.end() ? 0 : static_cast<int>(i - indices.begin()) / 8;
        } else {
          tab                = Tab::Timeline;
          roundFilter        = e.round;
          allEvents          = false;
          const auto indices = battle.timeline(selected, roundFilter, false);
          const auto i       = std::find(indices.begin(), indices.end(), inspected);
          page               = i == indices.end() ? 0 : static_cast<int>(i - indices.begin()) / 8;
        }
      }
      break;
  }
  dirty = true;
}

void Input()
{
  static auto pressed  = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButtonDown(System.Int32)");
  static auto released = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButtonUp(System.Int32)");
  static auto focused  = il2cpp_resolve_icall_typed<bool()>("UnityEngine.Application::get_isFocused()");
  struct Vec3 {
    float x, y, z;
  };
  static auto position =
      il2cpp_resolve_icall_typed<void(Vec3*)>("UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
  if (!pressed || !released || !focused || !position || !focused()) {
    pendingClick = false;
    pressedObject.reset();
    return;
  }
  // Both press and release must raycast to our Image, so another modal or a
  // drag cannot activate a geometrically overlapping control behind it.
  if (pendingClick) {
    pendingClick = false;
    if (pendingGeneration == uiGeneration) {
      auto* target  = RaycastAt(pendingPoint);
      auto* origin  = pressedObject.get();
      auto& surface = overlayOpen ? overlay : entry;
      for (const auto& hit : surface.hits) {
        auto* object = il2cpp_gchandle_get_target(hit.object);
        if (object && object == target && object == origin && Alive(object) && Inside(object, pendingPoint)) {
          const auto action = hit.action;
          Apply(action);
          break;
        }
      }
    }
    pressedObject.reset();
  }
  if (pressed(0)) {
    Vec3 point{};
    position(&point);
    pressedObject.reset(RaycastAt({point.x, point.y}));
  }
  if (released(0)) {
    Vec3 point{};
    position(&point);
    pendingPoint      = {point.x, point.y};
    pendingGeneration = uiGeneration;
    pendingClick      = true;
  }
}

void Clear()
{
  overlay.clear();
  entry.clear();
  font.reset();
  view.reset();
  ResetData();
  overlayOpen  = false;
  pendingClick = false;
  pressedObject.reset();
  ++uiGeneration;
}
} // namespace

bool BattleAnalysisIsOpen()
{ return overlayOpen; }

void UpdateBattleAnalysis()
{
  if (!Config::Get().battle_log_summary_preview) {
    if (view.handle || entry.object.handle || overlay.object.handle)
      Clear();
    return;
  }
  if (Alive(view.get())) {
    bool active = false;
    if (!Value(view.get(), "get_isActiveAndEnabled", active) || !active) {
      Clear();
      return;
    }
    Input();
    AdvanceRead();
  }
  const auto now = Clock::now();
  if (now >= nextDiscovery) {
    nextDiscovery = now + std::chrono::milliseconds(250);
    if (!Alive(view.get())) {
      Clear();
      static auto* cls = Class("Assembly-CSharp", "Digit.Prime.Combat", "BattleReportViewController");
      view.reset(Find(cls));
    }
    bool active = false;
    if (!Alive(view.get()) || !Value(view.get(), "get_isActiveAndEnabled", active) || !active) {
      Clear();
      return;
    }
    auto*        source = Field(Get(view.get(), "get_CanvasContext"), "BattleResultData");
    std::int64_t id     = 0;
    if (!source || !Value(source, "get_ID", id))
      return;
    if (source != result.get() || id != reportId) {
      overlay.clear();
      entry.clear();
      ResetData();
      overlayOpen  = false;
      pendingClick = false;
      pressedObject.reset();
      ++uiGeneration;
      result.reset(source);
      reportId = id;
    }
    if (!font.get()) {
      static auto* tmp    = Class("Unity.TextMeshPro", "TMPro", "TextMeshProUGUI");
      auto*        native = Field(Field(view.get(), "_battleResultWidget"), "_victoryText");
      font.reset(Get(Component(native, tmp), "get_font"));
      if (!font.get())
        font.reset(Get(WithType(TypeMethod(view.get()->klass, "GetComponentInChildren"), view.get(), tmp), "get_font"));
    }
    if (font.get() && !Alive(entry.object.get()) && !CreateEntry()) {
      entry.clear();
      static bool warned = false;
      if (!warned) {
        spdlog::warn("Battle analysis: report entry layout unavailable");
        warned = true;
      }
    }
    if (overlayOpen && Alive(overlay.object.get())) {
      Rect rect{};
      if (Value(Get(overlay.object.get(), "get_transform"), "get_rect", rect)
          && (std::abs(rect.w - overlay.sx * kWidth) > .5f || std::abs(rect.h - overlay.sy * kHeight) > .5f))
        dirty = true;
    }
  }
  if (overlayOpen && font.get() && (dirty || !Alive(overlay.object.get()))) {
    if (!CreateOverlay()) {
      overlay.clear();
      overlayOpen = false;
      spdlog::warn("Battle analysis: native overlay layout unavailable; retaining report controls");
    }
  }
}
