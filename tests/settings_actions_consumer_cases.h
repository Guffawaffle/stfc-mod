#pragma once

// Exercise the actual post-bind command callback; publication and Unity rendering
// are controlled boundaries, not a native game reproduction.
#include "../src/mod/features/settings_actions.cc"
#include "game/player_profile.h"

namespace
{
bool streamer_fixture_frame_ready = false;
}

bool FrameDeliveryAvailable() noexcept
{ return streamer_fixture_frame_ready; }

namespace mod_settings::native
{
bool AllowsSettingsSearchAction(std::string_view)
{ return false; }
} // namespace mod_settings::native

namespace
{
struct StreamerProfileFixtureUser {
  Il2CppObject header;
  bool         local, fails;
};

struct StreamerProfileFixtureView {
  Il2CppObject header;
  UserProfile* user;
};

bool StreamerProfileFixtureIsLocal(StreamerProfileFixtureUser* user, const MethodInfo*)
{
  if (user->fails)
    throw Il2CppExceptionWrapper(&thrown);
  return user->local;
}

void StreamerModeSelfProfileCases()
{
  auto& user_class = ValuesClass("Digit.PrimeServer.Models", "UserProfile");
  auto& view_class = ValuesClass("Digit.Prime.PlayerProfile", "PlayerProfileViewController", &unity_object_class);
  ValuesField(view_class, "_userProfile", user_class, offsetof(StreamerProfileFixtureView, user));
  AddMethod(user_class, "get_IsLocalPlayer", reinterpret_cast<void*>(&StreamerProfileFixtureIsLocal), T(bool_class),
            {});
  StreamerProfileFixtureUser local{{&user_class.klass}, true, false}, other{{&user_class.klass}, false, false};
  StreamerProfileFixtureView view{{&view_class.klass}, reinterpret_cast<UserProfile*>(&local)};
  auto*                      native_view = reinterpret_cast<PlayerProfileViewController*>(&view);
  const auto                 self        = native_view->TryIsLocalProfile();
  Require(self && self.value, "only positive native local ownership admits the self-profile mask");
  view.user          = reinterpret_cast<UserProfile*>(&other);
  const auto foreign = native_view->TryIsLocalProfile();
  Require(foreign && !foreign.value, "an inspected player keeps native profile presentation");
  other.fails = true;
  Require(!native_view->TryIsLocalProfile(), "an unreadable ownership check cannot qualify a profile");
  view.user = nullptr;
  Require(!native_view->TryIsLocalProfile(), "a pooled profile without its bound player cannot qualify");
  std::cout << "Streamer Mode: native self-profile ownership fixtures passed\n";
}

void StreamerModePlayerIdCases()
{
  auto&      state         = Actions();
  auto&      enabled       = Config::Get().ui.streamer_mode;
  const auto saved_enabled = enabled;
  const auto saved_thread  = state.thread;
  state.active             = true;
  state.thread             = std::this_thread::get_id();
  const auto saved_patch_enabled          = Config::Get().patches.streamermodehooks;
  Config::Get().patches.streamermodehooks = false;
  enabled                                 = true;
  Require(!InstallStreamerModeHooks(), "the startup patch switch declines the General Settings ID hook");
  Config::Get().patches.streamermodehooks = saved_patch_enabled;
  state.streamer_hooks     = true;
  Require(!streamer_mode::GeneralAccountIdsAvailable(), "account masking requires the pre-render refresh path");
  streamer_fixture_frame_ready = true;
  Require(streamer_mode::GeneralAccountIdsAvailable(),
          "the installed Player ID adapter is available with frame delivery");

  Il2CppObject receiver{&object_class.klass};
  auto*        director = reinterpret_cast<SettingsSectionDirector*>(&receiver);
  auto*        fake_id  = il2cpp::NewString("fixture-private-player-id");
  int          reads    = 0;
  const auto   get_id   = [&](auto*) {
    ++reads;
    return fake_id;
  };

  enabled = false;
  Require(SettingsSectionDirector_GetUserId_Hook(get_id, director) == fake_id,
          "inactive Streamer Mode uses the native value");
  enabled = true;
  Require(il2cpp::ToUtf8(SettingsSectionDirector_GetUserId_Hook(get_id, director)) == "Hidden" && reads == 1,
          "the first protected value binds as Hidden before evaluating the native getter");

  enabled = false;
  Require(SettingsSectionDirector_GetUserId_Hook(get_id, director) == fake_id && reads == 2,
          "disabling Streamer Mode restores native reads");
  enabled                      = saved_enabled;
  state.thread                 = saved_thread;
  state.active                 = false;
  state.streamer_hooks         = false;
  streamer_fixture_frame_ready = false;
  std::cout << "Streamer Mode: first Player ID display read and restoration fixtures passed\n";
}

struct StreamerCoverFixtureGroup {
  Il2CppObject header;
  float        alpha        = 1.0f;
  bool         fail_restore = false;
  bool         destroyed    = false;
};
struct StreamerCoverFixtureObject {
  Il2CppObject               header;
  StreamerCoverFixtureGroup* next = nullptr;
};

void StreamerModeCoverCases(SettingsActionWidget* receiver)
{
  auto& group_class = ValuesClass("UnityEngine", "CanvasGroup", &component_class);
  AddMethod(group_class, "set_alpha",
            reinterpret_cast<void*>(+[](StreamerCoverFixtureGroup* group, float alpha, const MethodInfo*) {
              if (alpha == 1.0f && group->fail_restore)
                throw Il2CppExceptionWrapper(&thrown);
              group->alpha = alpha;
            }),
            T(void_class), {T(float_class)});
  AddMethod(game_object_class, "AddComponent",
            reinterpret_cast<void*>(+[](StreamerCoverFixtureObject* object, Il2CppObject*, const MethodInfo*) {
              return &object->next->header;
            }),
            T(group_class), {T(type_class)});
  AddMethod(unity_object_class, "Destroy",
            reinterpret_cast<void*>(+[](StreamerCoverFixtureGroup* group, const MethodInfo*) {
              Require(group->alpha == 1.0f, "a retired group is neutral before Unity destroys it");
              group->destroyed = true;
            }),
            T(void_class), {T(unity_object_class)}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);

  auto& state  = Actions();
  state.active = state.streamer_hooks = streamer_fixture_frame_ready = true;
  state.thread                                                       = std::this_thread::get_id();
  Config::Get().ui.streamer_mode                                     = true;
  StreamerCoverFixtureGroup  old_group{{&group_class.klass}}, new_group{{&group_class.klass}};
  StreamerCoverFixtureObject label{{&game_object_class.klass}, &old_group};
  auto*                      object = reinterpret_cast<unity::GameObject*>(&label);
  auto                       view     = std::make_shared<StreamerAccountIdView>();
  state.account_id_views.emplace(receiver, view);
  Require(view->cover.Hide(object) && old_group.alpha == 0.0f, "the enrolled label is covered");
  warp_root_failure = true;
  TrackAccountIdView(receiver);
  warp_root_failure = false;
  Require(state.account_id_views.at(receiver) == view && !view->complete && old_group.alpha == 0.0f,
          "an unreadable native context preserves its existing mask and degrades status");
  old_group.fail_restore = true;
  ReleaseAccountIdView(receiver);
  Require(state.retired_account_id_views.size() == 1 && old_group.alpha == 0.0f && !old_group.destroyed,
          "a pooled row retains ownership of its failed restore");
  Require(streamer_mode::GeneralAccountIdsAvailable() && streamer_mode::GeneralAccountIdsDegraded(),
          "degraded account privacy preserves an editable installed control");

  streamer_mode::SurfaceCover rebound;
  label.next = &new_group;
  Require(rebound.Hide(object) && new_group.alpha == 0.0f, "a replacement bind owns its display group");
  old_group.fail_restore = false;
  RefreshAccountIdViews(true);
  Require(state.retired_account_id_views.empty() && old_group.destroyed && new_group.alpha == 0.0f,
          "retired cleanup leaves the replacement mask in place on the reused label");
  Require(!streamer_mode::GeneralAccountIdsDegraded(), "successful retry clears degraded status");
  Require(rebound.Restore() && new_group.destroyed, "the replacement can restore its own label");
  Config::Get().ui.streamer_mode = false;
  state.active = state.streamer_hooks = streamer_fixture_frame_ready = false;
  std::cout << "Streamer Mode: pooled label restore and recoverable account health passed\n";
}

struct SettingsActionConsumerContext {
  Il2CppObject              header;
  SettingsContainerContext* parent;
  Il2CppObject *            query, *callback;
  LocaleTextContext*        label = nullptr;
};
struct SettingsAccountIdFixtureLabel {
  Il2CppObject  header;
  Il2CppString* identifier;
};
SettingsContainerContext* ActionConsumerParent(SettingsActionConsumerContext* context, const MethodInfo*)
{ return context->parent; }
LocaleTextContext* ActionConsumerLabel(SettingsActionConsumerContext* context, const MethodInfo*)
{ return context->label; }
Il2CppString* ActionConsumerLabelIdentifier(SettingsAccountIdFixtureLabel* label, const MethodInfo*)
{ return label->identifier; }
settings_binding::ReferenceDelegate* ActionConsumerQuery(SettingsActionConsumerContext* context, const MethodInfo*)
{ return reinterpret_cast<settings_binding::ReferenceDelegate*>(context->query); }
settings_binding::ReferenceObject* ActionConsumerCallback(SettingsActionConsumerContext* context, const MethodInfo*)
{ return reinterpret_cast<settings_binding::ReferenceObject*>(context->callback); }

void SettingsActionConsumerCases()
{
  using namespace mod_settings;
  auto& option        = ValuesClass("Digit.Prime.GameSettings", "OptionContext");
  auto& container     = ValuesClass("Digit.Prime.GameSettings", "ContainerOptionContext", &option);
  auto& context_class = ValuesClass("Digit.Prime.GameSettings", "ButtonAndTextOptionContext", &option);
  auto& widget_class  = ValuesClass("Digit.Prime.GameSettings", "ButtonAndTextOptionWidget");
  auto& safe_class    = ValuesClass("Fixture.SettingsActions", "SafeAction");
  auto& list_class    = ValuesClass("Fixture.SettingsActions", "CallbackList");
  ValuesField(safe_class, "_callbacks", list_class, offsetof(ValuesRefObject, reference));
  AddMethod(list_class, "get_Count", reinterpret_cast<void*>(&ValuesCount), T(int_class), {});
  AddMethod(list_class, "get_Item", reinterpret_cast<void*>(&ValuesItem), T(delegate_base), {T(int_class)});
  AddMethod(option, "get_Parent", reinterpret_cast<void*>(&ActionConsumerParent), T(container), {});
  const auto& label_context =
      AddMethod(option, "get_LabelContext", reinterpret_cast<void*>(&ActionConsumerLabel), T(warp_locale_class), {});
  AddProperty(option, "LabelContext", &label_context);
  const auto& identifier = AddMethod(warp_locale_class, "get_Identifier",
                                     reinterpret_cast<void*>(&ActionConsumerLabelIdentifier), T(string_class), {});
  AddProperty(warp_locale_class, "Identifier", &identifier);
  AddMethod(option, "get_QueryOptionState", reinterpret_cast<void*>(&ActionConsumerQuery), T(delegate_base), {});
  AddMethod(context_class, "get_Callback", reinterpret_cast<void*>(&ActionConsumerCallback), T(safe_class), {});
  AddMethod(widget_class, "get_Context", reinterpret_cast<void*>(&SettingsConsumerGetContext), T(context_class), {});
  auto&      state = Actions();
  MethodInfo click_schema{}, query_schema{};
  click_schema.return_type = T(void_class);
  query_schema.return_type = T(int_class);
  Require(state.click.Initialize(&click_schema, InvokeAction) && state.query.Initialize(&query_schema, ActionQuery),
          "actual action ownership uses private matching callback schemas");
  Il2CppObject   token{&object_class.klass}, foreign_token{&object_class.klass}, parent{&container.klass};
  Il2CppDelegate click{}, query{}, foreign{};
  const auto     delegate = [&](Il2CppDelegate& value, const MethodInfo* method) {
    reinterpret_cast<Il2CppObject*>(&value)->klass = &delegate_base.klass;
    value.target                                   = &token;
    value.method                                   = method;
    value.method_ptr = value.invoke_impl = method->methodPointer;
    value.invoke_impl_this               = value.target;
  };
  delegate(click, state.click.method());
  delegate(query, state.query.method());
  ValuesListObject callbacks;
  callbacks.header.klass = &list_class.klass;
  callbacks.entries      = {reinterpret_cast<Il2CppObject*>(&click), nullptr};
  ValuesRefObject               safe{{&safe_class.klass}, &callbacks.header};
  SettingsActionConsumerContext context{{&context_class.klass},
                                        reinterpret_cast<SettingsContainerContext*>(&parent),
                                        reinterpret_cast<Il2CppObject*>(&query),
                                        &safe.header};
  SettingsActionConsumerContext replacement = context;
  auto*                         row         = reinterpret_cast<SettingsActionContext*>(&context);
  SettingsAccountIdFixtureLabel account_label{{&warp_locale_class.klass}, il2cpp::NewString("game_settings_option_14")};
  context.label = reinterpret_cast<LocaleTextContext*>(&account_label);
  Require(AccountIdRow(row) == AccountIdRowKind::Scopely, "the Scopely row uses the value-label-only mask");
  account_label.identifier = il2cpp::NewString("game_settings_option_0");
  Require(AccountIdRow(row) == AccountIdRowKind::Player, "the normal Player ID row keeps its placeholder path");
  account_label.identifier = il2cpp::NewString("game_settings_option_1");
  Require(AccountIdRow(row) == AccountIdRowKind::None, "ordinary action rows remain outside the account value mask");
  context.label = nullptr;
  SettingsConsumerWidget        widget{{&widget_class.klass}, row};
  auto*                         native_widget = reinterpret_cast<SettingsActionWidget*>(&widget);
  int                           invoked       = 0;
  std::function<void()>         on_read;
  ActionSetting                 action;
  action.identity = "fixture.defaults.details";
  action.label    = "Defaults and Help";
  action.read     = [&](size_t) {
    if (on_read)
      std::exchange(on_read, {})();
    return ActionSetting::Presentation{"Setting details", "Details", "Saved"};
  };
  action.invoke = [&](size_t index) {
    Require(index == 0 && native::InvokingActionWidget() == native_widget,
            "actual command retains its index and synchronous invoking-widget custody");
    ++invoked;
    InvokeAction(&token, nullptr); // The same command cannot invoke itself reentrantly.
  };
  const auto roots_before = warp_roots.size();
  state.active            = true;
  state.thread            = std::this_thread::get_id();
  auto record             = std::make_shared<SettingsActionRow>();
  record->context         = il2cpp::GcHandle::Weak(row);
  record->parent          = il2cpp::GcHandle::Weak(&parent);
  record->click           = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&click));
  record->query           = il2cpp::GcHandle::Weak(reinterpret_cast<Il2CppObject*>(&query));
  record->token           = il2cpp::GcHandle::Weak(&token);
  record->action          = &action;
  state.rows.emplace(row, record);
  auto view     = std::make_shared<SettingsActionView>();
  view->widget  = il2cpp::GcHandle::Weak(native_widget);
  view->context = il2cpp::GcHandle::Weak(row);
  view->token   = il2cpp::GcHandle::Weak(&token);
  view->action  = &action;
  view->visit   = settings_consumer_visit;
  view->ready   = true;
  view->applied = {"Setting details", "Details", "Saved"};
  state.views.emplace(native_widget, view);
  InvokeAction(&token, nullptr);
  Require(invoked == 1 && !native::InvokingActionWidget() && !view->invoking && settings_consumer_refreshes == 1,
          "one owned command invokes once, restores synchronous custody and requests deferred refresh");
  InvokeAction(&foreign_token, nullptr);
  settings_consumer_page_refresh = true;
  InvokeAction(&token, nullptr);
  settings_consumer_page_refresh = false;
  fleet_native_capture           = true;
  InvokeAction(&token, nullptr);
  fleet_native_capture = false;
  Require(invoked == 1, "foreign token, page refresh and shortcut capture cannot authorize a command");
  on_read = [&] { settings_values_on_count = [&] { ++settings_consumer_visit; }; };
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "page departure during the final ownership read must not invoke the departed action");
  view->visit = settings_consumer_visit;
  on_read     = [&] {
    settings_values_on_count = [&] { widget.context = reinterpret_cast<SettingsActionContext*>(&replacement); };
  };
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "context replacement during final ownership reads must not invoke the old action");
  widget.context = row;
  on_read        = [&] { settings_values_on_count = [&] { state.views.erase(native_widget); }; };
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "retired view cannot invoke through a retained shared record");
  state.views.emplace(native_widget, view);
  on_read = [&] { settings_values_on_count = [&] { settings_consumer_on_context = [&] { state.rows.erase(row); }; }; };
  InvokeAction(&token, nullptr);
  Require(invoked == 1 && state.rows.empty(), "row retirement during final context observation blocks invocation");
  state.rows.emplace(row, record);
  view->applied[2] = "Saving...";
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "changed save-status presentation requires a fresh render before invoking");
  view->applied[2]     = "Saved";
  callbacks.entries[0] = reinterpret_cast<Il2CppObject*>(&foreign);
  InvokeAction(&token, nullptr);
  Require(invoked == 1, "foreign callback ownership cannot invoke a mod command");
  callbacks.entries[0] = reinterpret_cast<Il2CppObject*>(&click);
  InvokeAction(&token, nullptr);
  Require(invoked == 2 && !native::InvokingActionWidget(), "original owned row remains usable after refused inputs");
  state.views.clear();
  state.rows.clear();
  view.reset();
  record.reset();
  state.active = false;
  Require(warp_roots.size() == roots_before, "action consumer releases every admitted managed root");
  std::cout << "Settings action consumer: invocation, token/page/context/presentation guards and reentry passed\n";
  StreamerModePlayerIdCases();
  StreamerModeSelfProfileCases();
  StreamerModeCoverCases(native_widget);
}
} // namespace
