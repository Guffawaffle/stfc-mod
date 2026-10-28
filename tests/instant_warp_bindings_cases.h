#pragma once

// Included after the controlled runtime and fleet fragment. Exercises the
// production prompt, click-intent and label owner, not another policy model.
#include "../src/mod/features/instant_warp.cc"

namespace
{
struct InstantWarpPopupObject {
  Il2CppObject  header;
  Il2CppObject* widget = nullptr;
};
struct InstantWarpWidgetObject {
  Il2CppObject  header;
  Il2CppObject* context = nullptr;
};
struct InstantWarpContextObject {
  Il2CppObject  header;
  bool          instant = true;
  Il2CppObject* course  = nullptr;
};
struct InstantWarpTextObject {
  Il2CppObject  header;
  bool          overridden = false;
  Il2CppString* text       = nullptr;
};
std::vector<std::unique_ptr<std::byte[]>> instant_warp_strings;
Il2CppString*                             InstantWarpString(std::string_view text)
{
  auto  storage = std::make_unique<std::byte[]>(sizeof(Il2CppString) + text.size() * sizeof(Il2CppChar));
  auto* value   = reinterpret_cast<Il2CppString*>(storage.get());
  reinterpret_cast<Il2CppObject*>(value)->klass = &string_class.klass;
  value->length                                 = static_cast<int32_t>(text.size());
  std::copy(text.begin(), text.end(), value->chars);
  instant_warp_strings.push_back(std::move(storage));
  return value;
}
Il2CppObject* instant_warp_fleet = nullptr;
Il2CppObject  instant_warp_hull{};
Il2CppString* instant_warp_name    = nullptr;
int           instant_warp_regular = 0, instant_warp_jump = 0, instant_warp_overrides = 0, instant_warp_clears = 0;
bool          instant_warp_text_throws = false, instant_warp_set_throws = false;
std::function<void()> instant_warp_name_read;
Il2CppObject*         InstantWarpFleet(Il2CppObject*, const MethodInfo*)
{ return instant_warp_fleet; }
Il2CppObject  instant_warp_manager{};
Il2CppObject* InstantWarpManager(const MethodInfo*)
{ return &instant_warp_manager; }
int64_t InstantWarpNodeId(Il2CppObject*, const MethodInfo*)
{ return 9007199254740995LL; }
Il2CppObject* InstantWarpHull(Il2CppObject*, const MethodInfo*)
{ return &instant_warp_hull; }
int64_t InstantWarpLoca(Il2CppObject*, const MethodInfo*)
{ return 123; }
void          InstantWarpLocaleCtor(LocaleTextContext*, Il2CppString*, Il2CppString*, const MethodInfo*) {}
Il2CppString* InstantWarpLocalise(LocaleTextContext*, bool, int32_t, const MethodInfo*)
{
  if (instant_warp_name_read)
    instant_warp_name_read();
  return instant_warp_name;
}
CoursePromptPopupDataContext* InstantWarpContext(CoursePromptPopupWidget* widget, const MethodInfo*)
{ return reinterpret_cast<CoursePromptPopupDataContext*>(reinterpret_cast<InstantWarpWidgetObject*>(widget)->context); }
void InstantWarpRegular(CoursePromptPopupWidget*, const MethodInfo*)
{ ++instant_warp_regular; }
void InstantWarpJump(CoursePromptPopupWidget*, const MethodInfo*)
{ ++instant_warp_jump; }
Il2CppString* InstantWarpCurrentText(TextLocalizer* label, const MethodInfo*)
{
  if (instant_warp_text_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return reinterpret_cast<InstantWarpTextObject*>(label)->text;
}
void InstantWarpOverrideText(TextLocalizer* label, Il2CppString* text, const MethodInfo*)
{
  if (instant_warp_set_throws)
    throw Il2CppExceptionWrapper(&thrown);
  ++instant_warp_overrides;
  auto* value       = reinterpret_cast<InstantWarpTextObject*>(label);
  value->overridden = true;
  value->text       = text;
}
void InstantWarpClearText(TextLocalizer* label, const MethodInfo*)
{
  ++instant_warp_clears;
  auto* value       = reinterpret_cast<InstantWarpTextObject*>(label);
  value->overridden = false;
  value->text       = InstantWarpString("Set Course");
}

void InstantWarpNativeCases(std::string_view mode)
{
  auto& popup_class =
      Define("Digit.Prime.ObjectViewer", "CoursePromptPopupViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& widget_class = Define("Digit.Prime.Navigation", "CoursePromptPopupWidget", IL2CPP_TYPE_CLASS, &object_class);
  auto& context_class =
      Define("Digit.Prime.Navigation", "CoursePromptPopupDataContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& label_class     = Define("Digit.Client.UI", "TextLocalizer", IL2CPP_TYPE_CLASS, &unity_object_class);
  auto& hull_class      = Define("Digit.PrimeServer.Models", "HullSpec", IL2CPP_TYPE_CLASS, &object_class);
  auto& localizer_class = Define("Digit.Client.Localization", "Localizer", IL2CPP_TYPE_CLASS, &object_class);
  auto& fleets_class = Define("Digit.Prime.FleetManagement", "FleetsManager", IL2CPP_TYPE_CLASS, &unity_object_class);
  AddField(popup_class, "_coursePromptPopupWidget", widget_class, offsetof(InstantWarpPopupObject, widget));
  AddField(context_class, "HasInstantWarp", bool_class, offsetof(InstantWarpContextObject, instant));
  AddField(context_class, "CourseData", warp_course_class, offsetof(InstantWarpContextObject, course));
  AddField(label_class, "_textOverride", mode == "instant-warp-missing-override" ? int_class : bool_class,
           offsetof(InstantWarpTextObject, overridden));
  AddMethod(widget_class, "get_Context", reinterpret_cast<void*>(&InstantWarpContext), T(context_class), {});
  AddMethod(widget_class, "InitiateRegularWarp", reinterpret_cast<void*>(&InstantWarpRegular), T(void_class), {});
  AddMethod(widget_class, "OnInstantWarpButtonClick", reinterpret_cast<void*>(&InstantWarpJump), T(void_class), {});
  AddMethod(warp_course_class, "get_PlayerFleet", reinterpret_cast<void*>(&InstantWarpFleet), T(fleet_data_class), {});
  AddMethod(warp_node_class, "get_ID", reinterpret_cast<void*>(&InstantWarpNodeId), T(long_class), {});
  AddMethod(fleets_class, "get_Instance", reinterpret_cast<void*>(&InstantWarpManager), T(fleets_class), {},
            METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(fleets_class, "GetSelectedFleetData", reinterpret_cast<void*>(&InstantWarpFleet), T(fleet_data_class), {});
  AddMethod(fleet_data_class, "get_Hull", reinterpret_cast<void*>(&InstantWarpHull), T(hull_class), {});
  AddProperty(fleet_data_class, "Hull", fleet_data_class.methods.back());
  AddMethod(fleet_data_class, "GetLocaId", reinterpret_cast<void*>(&InstantWarpLoca), T(long_class), {});
  AddMethod(warp_locale_class, ".ctor", reinterpret_cast<void*>(&InstantWarpLocaleCtor), T(void_class),
            {T(string_class), T(string_class)});
  AddMethod(localizer_class, "Localise", reinterpret_cast<void*>(&InstantWarpLocalise), T(string_class),
            {T(warp_locale_class), T(bool_class), T(int_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddMethod(label_class, "CurrentText", reinterpret_cast<void*>(&InstantWarpCurrentText), T(string_class), {});
  AddMethod(label_class, "OverrideLocalizedText", reinterpret_cast<void*>(&InstantWarpOverrideText), T(void_class),
            {T(string_class)});
  AddMethod(label_class, "ClearTextOverride", reinterpret_cast<void*>(&InstantWarpClearText), T(void_class), {});
  instant_warp_hull.klass    = &hull_class.klass;
  instant_warp_manager.klass = &fleets_class.klass;
  instant_warp_fleet         = reinterpret_cast<Il2CppObject*>(&identity_fleet);
  instant_warp_name          = InstantWarpString("U.S.S. Enterprise-D");
  InstantWarpContextObject context{{&context_class.klass}, true, &warp_course};
  InstantWarpWidgetObject  widget{{&widget_class.klass}, &context.header};
  InstantWarpPopupObject   popup{{&popup_class.klass}, &widget.header};
  InstantWarpTextObject    label{{&label_class.klass}, false, InstantWarpString("Set Course")};
  auto*                    fleet        = reinterpret_cast<FleetPlayerData*>(instant_warp_fleet);
  auto*                    text         = reinterpret_cast<TextLocalizer*>(&label);
  auto&                    ui           = Config::Get().ui;
  const auto               reset_policy = [&] {
    ui.auto_confirm_instant_warp = InstantWarpConfirmation::kJump;
    ui.instant_warp_always_ask   = {};
    ui.instant_warp_auto_jump    = {};
    ui.instant_warp_auto_warp    = {};
  };
  reset_policy();
  const auto initial_roots = warp_roots.size();
  OwnedLabel owned;
  if (mode == "instant-warp-missing-override") {
    owned.Apply(text, "JUMP");
    Require(!owned.Get() && instant_warp_overrides == 0 && !label.overridden,
            "unavailable override ownership never becomes an observed external=false");
    Require(warp_roots.size() == initial_roots, "unavailable label storage releases temporary roots");
    return;
  }
  Require(mode == "instant-warp-native", "unknown instant warp fixture mode");
  Require(ResolveConfirmation(fleet) == InstantWarpConfirmation::kJump,
          "valid native ship uses the configured default");
  ui.instant_warp_auto_warp.names = {ShipNameWords("Enterprise-D")};
  Require(ResolveConfirmation(fleet) == InstantWarpConfirmation::kWarp,
          "localized hull words retain punctuation normalization and suffix matching");
  ui.instant_warp_auto_jump.names = {ShipNameWords("Enterprise-D")};
  Require(ResolveConfirmation(fleet) == InstantWarpConfirmation::kJump, "jump wins over overlapping warp filter");
  ui.instant_warp_always_ask.names = {ShipNameWords("Enterprise-D")};
  Require(ResolveConfirmation(fleet) == InstantWarpConfirmation::kNone, "always-ask wins over both automatic filters");
  instant_warp_name = nullptr;
  Require(ResolveConfirmation(fleet) == InstantWarpConfirmation::kNone,
          "unavailable localization cannot rule out a named always-ask exclusion");
  reset_policy();
  Require(ResolveConfirmation(fleet) == InstantWarpConfirmation::kJump,
          "default remains usable when no name-dependent policy needs localization");
  ui.instant_warp_auto_jump.all   = true;
  ui.instant_warp_auto_warp.names = {ShipNameWords("Enterprise-D")};
  Require(ResolveConfirmation(fleet) == InstantWarpConfirmation::kJump,
          "a known wildcard decision does not require irrelevant lower-priority name resolution");
  reset_policy();
  instant_warp_name       = InstantWarpString("U.S.S. Enterprise-D");
  auto*      native_popup = reinterpret_cast<CoursePromptPopupViewController*>(&popup);
  int        originals    = 0;
  const auto show         = [&] {
    CoursePromptPopupViewController_AboutToShow_Hook([&](auto*) { ++originals; }, native_popup);
  };
  show();
  Require(originals == 1 && instant_warp_jump == 1 && instant_warp_regular == 0,
          "production popup invokes original once before its qualified configured action");
  context.course = nullptr;
  show();
  Require(instant_warp_jump == 1, "missing course does not authorize configured jump");
  context.course     = &warp_course;
  instant_warp_fleet = nullptr;
  show();
  Require(instant_warp_jump == 1, "missing player fleet leaves the native prompt available");
  instant_warp_fleet = reinterpret_cast<Il2CppObject*>(&identity_fleet);
  context.instant    = false;
  show();
  Require(instant_warp_jump == 1, "ordinary non-instant prompt is not answered");
  context.instant        = true;
  instant_warp_name_read = [&] { context.course = nullptr; };
  show();
  Require(instant_warp_jump == 1, "course rebound during localization cannot receive the stale decision");
  instant_warp_name_read        = {};
  context.course                = &warp_course;
  constexpr int64_t exact_fleet = 9007199254740993LL;
  warp_click_intent.Begin(true, exact_fleet, 1);
  // The first popup has a different positively observed node: consume without
  // inheriting a manual choice; the next popup follows ordinary configuration.
  const auto node_id = reinterpret_cast<CourseNode*>(&warp_node)->TryId();
  Require(node_id.succeeded, "fixture node identity is populated");
  ++input_frame;
  any_held                                                   = true;
  held_keys[std::to_underlying(unity::KeyCode::LeftControl)] = true;
  held_keys[std::to_underlying(unity::KeyCode::LeftCommand)] = true;
  instant_mouse_held                                         = true;
  int        moves                                           = 0;
  const auto move                                            = [&](int64_t fleet_id, int64_t target) {
    NavigationManager_OnMoveFleetAction_ChoiceHook([&](auto*, auto, int64_t, int64_t, bool) { ++moves; },
                                                   reinterpret_cast<NavigationManager*>(&warp_navigation),
                                                   il2cpp::Nullable<unity::Vector3>{}, target, fleet_id, false);
  };
  move(exact_fleet, node_id.value);
  Require(moves == 1 && warp_click_intent.fleet == exact_fleet && warp_click_intent.destination == node_id.value,
          "actual move hook captures held modified-click exact identities before the original");
  instant_mouse_held     = false;
  instant_mouse_released = true;
  move(exact_fleet, node_id.value);
  Require(moves == 2 && warp_click_intent.fleet == exact_fleet,
          "mouse release edge also captures a modified native click");
  instant_mouse_released = false;
  ++input_frame;
  any_held = false;
  held_keys.fill(false);
  show();
  Require(instant_warp_jump == 1 && !warp_click_intent.fleet,
          "matching exact-ID manual request consumes once and retains native choice");
  show();
  Require(instant_warp_jump == 2, "consumed request cannot suppress another popup");
  warp_click_intent.Begin(true, exact_fleet, node_id.value);
  move(exact_fleet, node_id.value);
  Require(!warp_click_intent.fleet && moves == 3,
          "ordinary move clears an earlier modified-click intent before its native original");
  warp_click_intent.Begin(true, exact_fleet, node_id.value + 1);
  show();
  Require(instant_warp_jump == 3 && !warp_click_intent.fleet,
          "positively mismatched trip does not inherit the earlier request");
  warp_click_intent.Begin(true, 0, node_id.value);
  show();
  Require(instant_warp_jump == 3, "unresolved explicit manual request cannot auto-select a paid option");
  warp_root_failure = true;
  show();
  warp_root_failure = false;
  Require(instant_warp_jump == 3, "unavailable source custody leaves popup action native");

  owned.Apply(text, "JUMP");
  Require(label.overridden && owned.Get() == text && il2cpp::ToUtf8(label.text) == "JUMP" && instant_warp_overrides == 1
              && warp_roots.size() == initial_roots + 1,
          "production label owner retains only one weak handle between updates");
  owned.Apply(text, "JUMP");
  Require(instant_warp_overrides == 1, "unchanged frame does not rewrite localized label");
  label.text = InstantWarpString("External");
  owned.Clear();
  Require(label.overridden && il2cpp::ToUtf8(label.text) == "External" && instant_warp_clears == 0,
          "cleanup cannot erase another owner's replacement text");
  owned.Apply(text, "WARP");
  Require(!owned.Get() && instant_warp_overrides == 1, "existing external text override is not acquired");
  label.overridden = false;
  label.text       = InstantWarpString("Set Course");
  owned.Apply(text, "WARP");
  instant_warp_text_throws = true;
  owned.Apply(text, "JUMP");
  instant_warp_text_throws = false;
  Require(!owned.Get() && instant_warp_overrides == 2 && instant_warp_clears == 0,
          "failed current-text read neither overwrites nor clears unknown text");
  label.overridden = false;
  owned.Apply(text, "JUMP");
  owned.Clear();
  Require(!label.overridden && instant_warp_clears == 1 && warp_roots.size() == initial_roots,
          "known owned label clears and releases its handle on unbind");
  instant_warp_set_throws = true;
  owned.Apply(text, "WARP");
  instant_warp_set_throws = false;
  Require(!owned.Get() && owned.text.empty() && warp_roots.size() == initial_roots,
          "failed native setter cannot manufacture successful retained label ownership");
  std::cout << "Instant-warp production prompt, policy, intent and owned-label adapters passed\n";
}
} // namespace
