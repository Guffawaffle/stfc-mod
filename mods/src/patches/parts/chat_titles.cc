#include "prime/ChatMessageWidget.h"
#include "prime/GameObject.h"
#include "prime/UserProfileWidget.h"

#include "config.h"
#include "errormsg.h"
#include "patches/native_hook_extent.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>

#include <optional>

#include <spdlog/spdlog.h>
#include "patches/spud_install.h"

namespace
{
struct ProfileRoot {
  Il2CppGCHandle handle;
  explicit ProfileRoot(Il2CppObject* object)
      : handle(object ? il2cpp_gchandle_new(object, false) : nullptr)
  {
  }
  ~ProfileRoot()
  {
    if (handle)
      il2cpp_gchandle_free(handle);
  }
  ProfileRoot(const ProfileRoot&) = delete;
  Il2CppObject* Get() const
  { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }
};

// Use Unity's scoped native parent lookup when the client's non-generic API
// matches. No row/owner cache survives pooling or reparenting. A missing API
// retains the existing bounded walk rather than adding a scene-wide search.
std::optional<Il2CppObject*> ParentComponent(Il2CppObject* object, Il2CppClass* type)
{
  static const auto* method = method_contract::Resolve(
      il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component").get_cls(), "GetComponentInParent",
      false, "UnityEngine.Component", {"System.Type", "System.Boolean"});
  if (!method || !object || !type)
    return std::nullopt;
  ProfileRoot owner(object);
  if (!owner.Get())
    return std::nullopt;
  auto*         reflection_type  = il2cpp_type_get_object(il2cpp_class_get_type(type));
  bool          include_inactive = true;
  void*         args[]{reflection_type, &include_inactive};
  Il2CppObject* result = nullptr;
  if (!reflection_type || !Il2CppRuntime::TryInvoke(method, owner.Get(), args, &result))
    return std::nullopt;
  return result;
}

Il2CppObject* InvokeSelf(Il2CppObject* object, const char* method_name)
{
  auto* method = il2cpp_class_get_method_from_name(object ? il2cpp_object_get_class(object) : nullptr, method_name, 0);
  if (!method) {
    return nullptr;
  }
  Il2CppException* exception = nullptr;
  return (Il2CppObject*)il2cpp_runtime_invoke(method, object, nullptr, &exception);
}
bool IsChatRow(void* profile_widget)
{
  auto* mb = (Il2CppObject*)profile_widget;
  if (!mb) {
    return false;
  }
  if (const auto chat = ParentComponent(mb, ChatMessageWidget::get_class_helper().get_cls()))
    return *chat != nullptr;

  auto* transform = InvokeSelf(mb, "get_transform");
  if (!transform) {
    return false;
  }

  for (int hops = 0; hops < 32 && transform; ++hops) {
    auto* gameObject = InvokeSelf(transform, "get_gameObject");
    if (!gameObject) {
      return false;
    }
    if (reinterpret_cast<GameObject*>(gameObject)->GetComponentFastPath2<ChatMessageWidget>()) {
      return true;
    }
    transform = InvokeSelf(transform, "get_parent");
  }
  return false;
}
void HideTitleRow(void* title)
{
  auto* game_object = reinterpret_cast<GameObject*>(InvokeSelf(static_cast<Il2CppObject*>(title), "get_gameObject"));
  if (!game_object) {
    return;
  }
  game_object->SetActive(false);
}

void ApplyTitleDisplay(void* profile_widget)
{
  auto* widget = static_cast<UserProfileWidget*>(profile_widget);
  if (!widget) {
    return;
  }

  const auto show  = Config::Get().show_player_title;
  auto*      title = widget->_playerTitleWidget;
  if (show || title == nullptr) {
    return;
  }

  if (!IsChatRow(widget)) {
    return;
  }

  HideTitleRow(title);
}
} // namespace

void UserProfileWidget_UpdatePlayerTitle_Hook(auto original, void* _this)
{
  original(_this);
  try {
    ApplyTitleDisplay(_this);
  } catch (...) {
  }
}
void PlayerTitleWidget_SetIdentifiers_Hook(auto original, void* _this)
{
  original(_this);
  try {
    if (Config::Get().show_player_title) {
      return;
    }
    if (const auto owner =
            ParentComponent(static_cast<Il2CppObject*>(_this), UserProfileWidget::get_class_helper().get_cls())) {
      ProfileRoot root(*owner);
      if (root.Get())
        ApplyTitleDisplay(root.Get());
      return;
    }
    UserProfileWidget* owner     = nullptr;
    auto*              transform = InvokeSelf((Il2CppObject*)_this, "get_transform");
    for (int hops = 0; hops < 32 && transform && !owner; ++hops) {
      auto* gameObject = reinterpret_cast<GameObject*>(InvokeSelf(transform, "get_gameObject"));
      if (gameObject) {
        owner = gameObject->GetComponentFastPath2<UserProfileWidget>();
      }
      transform = InvokeSelf(transform, "get_parent");
    }
    if (owner) {
      ApplyTitleDisplay(owner);
    }
  } catch (...) {
  }
}

namespace
{
#if __APPLE__
void* ValidateDetourTarget(void* entry, const char* name)
{
  if (native_hooks::MacHookFits(entry, 16)) {
    return entry;
  }
#if defined(__aarch64__)
  if (const auto* downstream = native_hooks::MacFollowDirectBranch(entry)) {
    if (native_hooks::MacHookFits(downstream, 16)) {
      return const_cast<void*>(downstream);
    }
  }
#endif
  spdlog::warn("[ChatTitleDisplay] {} does not fit the macOS hook window; target skipped", name);
  return nullptr;
}
#else
void* ValidateDetourTarget(void* entry, const char* name)
{
  (void)name;
  return entry;
}
#endif
} // namespace

void InstallChatTitleDisplayHooks()
{
  void* update_player_title = nullptr;
  if (auto profile_widget =
          il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlayerProfile", "UserProfileWidget");
      !profile_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.PlayerProfile", "UserProfileWidget");
  } else if ((update_player_title = profile_widget.GetMethod("UpdatePlayerTitle", 0)) == nullptr) {
    ErrorMsg::MissingMethod("UserProfileWidget", "UpdatePlayerTitle");
  }

  void* set_identifiers = nullptr;
  if (auto title_widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlayerAvatars", "PlayerTitleWidget");
      !title_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.PlayerAvatars", "PlayerTitleWidget");
  } else if ((set_identifiers = title_widget.GetMethod("SetIdentifiers", 0)) == nullptr) {
    ErrorMsg::MissingMethod("PlayerTitleWidget", "SetIdentifiers");
  }

  if (!update_player_title && !set_identifiers) {
    return;
  }

  if (update_player_title) {
    update_player_title = ValidateDetourTarget(update_player_title, "UpdatePlayerTitle");
  }
  if (set_identifiers) {
    set_identifiers = ValidateDetourTarget(set_identifiers, "SetIdentifiers");
  }

  if (!update_player_title && !set_identifiers) {
    spdlog::warn("Skipping ChatTitleDisplayHooks: no detour target fits; no hooks installed.");
    return;
  }

  // Both title-bind callbacks tail-call SetIdentifiers. On Windows client 272
  // their shared wrapper is only 16 bytes, less than SPUD's 24-byte overwrite,
  // so detouring it also corrupts the adjacent title-clear callback. Hook the
  // substantive implementation once instead.
  if (set_identifiers && set_identifiers == update_player_title) {
    spdlog::warn("[ChatTitleDisplay] SetIdentifiers shares its target with UpdatePlayerTitle; keeping one detour");
    set_identifiers = nullptr;
  }

  if (update_player_title) {
    SPUD_STATIC_DETOUR(update_player_title, UserProfileWidget_UpdatePlayerTitle_Hook);
  }
  if (set_identifiers) {
    SPUD_STATIC_DETOUR(set_identifiers, PlayerTitleWidget_SetIdentifiers_Hook);
  }

  spdlog::info("Chat title display: chat player title rows {}", Config::Get().show_player_title ? "shown" : "hidden");
}
