#pragma once

// Actual reveal hook with counted native lookup/action bodies. No ability is
// activated and no game popup is created by this controlled runtime.
namespace
{
Il2CppObject          reveal_manager;
bool                  reveal_manager_present = true, reveal_type_throws = false, reveal_action_throws = false;
int32_t               reveal_type    = ActivatedAbilityManager::kRevealHostileVger;
int                   reveal_queries = 0, reveal_activations = 0;
std::function<void()> reveal_on_type;
Il2CppObject*         RevealManager(const MethodInfo*)
{ return reveal_manager_present ? &reveal_manager : nullptr; }
int32_t RevealType(Il2CppObject*, int64_t id, const MethodInfo*)
{
  Require(id == 9007199254740993LL, "reveal lookup retains the complete native ability ID");
  ++reveal_queries;
  const auto before = reveal_type;
  if (reveal_on_type)
    std::exchange(reveal_on_type, {})();
  if (reveal_type_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return before;
}
void RevealActivate(Il2CppObject*, const MethodInfo*)
{
  if (reveal_action_throws)
    throw Il2CppExceptionWrapper(&thrown);
  ++reveal_activations;
}
void RevealStatic(const MethodInfo*)
{ ++reveal_activations; }
void RevealConfirmationCases()
{
  auto&       manager_class = ValuesClass("Digit.Prime.ActivatedAbilities", "ActivatedAbilityManager");
  auto&       action_class  = ValuesClass("Fixture.Reveal", "Activation");
  const auto& instance      = AddMethod(manager_class, "get_Instance", reinterpret_cast<void*>(&RevealManager),
                                        T(manager_class), {}, METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  AddProperty(manager_class, "Instance", &instance);
  AddMethod(manager_class, "GetActivatedAbilityType", reinterpret_cast<void*>(&RevealType), T(int_class),
            {T(long_class)});
  auto& activate = AddMethod(action_class, "Activate", reinterpret_cast<void*>(&RevealActivate), T(void_class), {});
  auto& other    = AddMethod(action_class, "Other", reinterpret_cast<void*>(&RevealActivate), T(void_class), {});
  auto& wrong_arity =
      AddMethod(action_class, "WrongArity", reinterpret_cast<void*>(&RevealActivate), T(void_class), {T(int_class)});
  auto& static_action  = AddMethod(action_class, "Static", reinterpret_cast<void*>(&RevealStatic), T(void_class), {},
                                   METHOD_ATTRIBUTE_PUBLIC | METHOD_ATTRIBUTE_STATIC);
  reveal_manager.klass = &manager_class.klass;
  Il2CppObject   target{&action_class.klass};
  Il2CppDelegate action{};
  reinterpret_cast<Il2CppObject*>(&action)->klass = &delegate_base.klass;
  action.method                                   = &activate;
  action.target                                   = &target;
  auto& ui                                        = Config::Get().ui;
  ui.auto_confirm_reveal_ability                  = true;
  const auto roots_before                         = warp_roots.size();
  int        originals                            = 0;
  const auto show                                 = [&](Il2CppDelegate* callback = nullptr) {
    MessageUtility_ShowFreeAbilityConfirmationPopup_Hook(
        [&](int64_t id, unity::TimeSpan cooldown, Il2CppDelegate* passed) {
          Require(id == 9007199254740993LL && cooldown.ticks == 0x123456789abcdefLL && passed == callback,
                  "native reveal fallback retains ability, full TimeSpan and original callback");
          ++originals;
        },
        9007199254740993LL, unity::TimeSpan{0x123456789abcdefLL}, callback);
  };
  show(&action);
  Require(reveal_activations == 1 && originals == 0, "qualified Excelsior action bypasses once");
  reveal_on_type = [&] { ui.auto_confirm_reveal_ability = false; };
  show(&action);
  Require(reveal_activations == 1 && originals == 1,
          "reveal policy disabled during ability lookup must retain the native confirmation");
  ui.auto_confirm_reveal_ability = true;
  reveal_on_type                 = [&] { action.method = &other; };
  show(&action);
  Require(reveal_activations == 1 && originals == 2,
          "lookup cannot replace the action behind an earlier qualification");
  action.method = &wrong_arity;
  show(&action);
  Require(reveal_activations == 1 && originals == 3, "nonzero-argument callback retains native handling");
  action.method = &activate;
  action.target = nullptr;
  show(&action);
  Require(reveal_activations == 1 && originals == 4, "instance callback without its receiver retains native handling");
  action.target = &target;
  reveal_type   = 86003;
  show(&action);
  reveal_type            = ActivatedAbilityManager::kRevealHostileVger;
  reveal_manager_present = false;
  show(&action);
  reveal_manager_present = true;
  reveal_type_throws     = true;
  show(&action);
  reveal_type_throws = false;
  Require(reveal_activations == 1 && originals == 7, "other, missing and unknown ability observations stay native");
  action.method = nullptr;
  show(&action);
  show();
  Require(reveal_activations == 1 && originals == 9, "missing method/action stays native");
  action.method  = &activate;
  reveal_on_type = [&] {
    const auto rooted = [](Il2CppObject* object) {
      return std::ranges::any_of(warp_roots, [object](const auto& root) { return root.second == object; });
    };
    Require(rooted(reinterpret_cast<Il2CppObject*>(&action)) && rooted(&reveal_manager),
            "reveal lookup retains operation-local callback and manager custody");
  };
  show(&action);
  Require(reveal_activations == 2 && originals == 9, "qualified retry still invokes once");
  reveal_action_throws = true;
  show(&action);
  reveal_action_throws = false;
  Require(reveal_activations == 2 && originals == 10, "callback failure preserves the existing native popup fallback");
  action.method = &static_action;
  action.target = nullptr;
  show(&action);
  Require(reveal_activations == 3 && originals == 10, "valid static Action remains supported");
  ui.auto_confirm_reveal_ability = false;
  const auto queries_before      = reveal_queries;
  show(&action);
  Require(reveal_queries == queries_before && originals == 11, "disabled bypass never resolves or invokes an ability");
  Require(warp_roots.size() == roots_before, "reveal success and fallback release every managed root");
  std::cout << "Reveal confirmation: live policy, callback identity/shape, exact fallback and roots passed\n";
}
} // namespace
