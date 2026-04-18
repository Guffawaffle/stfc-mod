/**
 * @file testing.cc
 * @brief Development and testing hooks.
 *
 * Contains hooks primarily used during mod development and testing:
 *   - Cursor override: replaces Unity's custom cursor with the OS default arrow.
 *   - Config URL override: injects custom platform settings and asset URLs.
 *   - SetActive deduplication: guards against redundant GameObject activation calls.
 *   - Action queue toggle: allows disabling the game's action queue feature.
 *
 * These patches are gated behind the installTestPatches config flag.
 */
#include "config.h"
#include "errormsg.h"

#include "prime/ActionRequirement.h"
#include "prime/BlurController.h"
#include "prime/BookmarksManager.h"
#include "prime/CallbackContainer.h"
#include "prime/ChatManager.h"
#include "prime/ChatMessageListLocalViewController.h"
#include "prime/ClientModifierType.h"
#include "prime/DeploymentManager.h"
#include "prime/FleetLocalViewController.h"
#include "prime/FleetsManager.h"
#include "prime/FullScreenChatViewController.h"
#include "prime/Hub.h"
#include "prime/InventoryForPopup.h"
#include "prime/KeyCode.h"
#include "prime/NavigationSectionManager.h"
#include "prime/ScanTargetViewController.h"
#include "prime/SceneManager.h"
#include "prime/ScreenManager.h"
#include <prime/UIBehaviour.h>

#include <il2cpp/il2cpp_helper.h>
#include <hook/hook.h>

// ─── IL2CPP Wrapper Classes ──────────────────────────────────────────────────

/// Wrapper for Digit.Client.Core.AppConfig — exposes platform URL properties.
class AppConfig
{
public:
  __declspec(property(get = __get_PlatformSettingsUrl, put = __set_PlatformSettingsUrl))
  Il2CppString*                                                                                  PlatformSettingsUrl;
  __declspec(property(get = __get_PlatformApiKey, put = __set_PlatformApiKey)) Il2CppString*     PlatformApiKey;
  __declspec(property(get = __get_AssetUrlOverride, put = __set_AssetUrlOverride)) Il2CppString* AssetUrlOverride;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "AppConfig");
    return class_helper;
  }

public:
  Il2CppString* __get_PlatformSettingsUrl()
  {
    static auto prop = get_class_helper().GetProperty("PlatformSettingsUrl");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_PlatformSettingsUrl(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("PlatformSettingsUrl");
    return prop.SetRaw((void*)this, *v);
  }

  Il2CppString* __get_PlatformApiKey()
  {
    static auto prop = get_class_helper().GetProperty("PlatformApiKey");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_PlatformApiKey(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("PlatformApiKey");
    return prop.SetRaw((void*)this, *v);
  }

  Il2CppString* __get_AssetUrlOverride()
  {
    static auto prop = get_class_helper().GetProperty("AssetUrlOverride");
    return prop.GetRaw<Il2CppString>((void*)this);
  }

  void __set_AssetUrlOverride(Il2CppString* v)
  {
    static auto prop = get_class_helper().GetProperty("AssetUrlOverride");
    return prop.SetRaw((void*)this, *v);
  }
};

/// Wrapper for Digit.Client.Core.Model — provides access to AppConfig.
class Model
{
public:
  __declspec(property(get = __get_AppConfig)) AppConfig* AppConfig_;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Model");
    return class_helper;
  }

public:
  AppConfig* __get_AppConfig()
  {
    static auto field = get_class_helper().GetField("_appConfig");
    return *(AppConfig**)((ptrdiff_t)this + field.offset());
  }
};

// ─── Hook Functions ──────────────────────────────────────────────────────────

/**
 * @brief Hook: UnityEngine.Cursor::SetCursor_Injected
 *
 * Intercepts cursor rendering to force the OS default arrow.
 * Original method: sets a custom cursor texture, hotspot, and mode.
 * Our modification: when allow_cursor is false (Windows only), replaces the
 *   cursor with IDC_ARROW and releases any Unity cursor clipping.
 */
typedef void (*Cursor_SetCursor_fn)(void*, ptrdiff_t, Vector2*, int);
static Cursor_SetCursor_fn Cursor_SetCursor_original = nullptr;

void Cursor_SetCursor(void* _this, ptrdiff_t texture, Vector2* hotspot, int cursorMode)
{
#if _WIN32
  if (!Config::Get().allow_cursor) {
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    ClipCursor(nullptr); // free cursor from any Unity clipping
    return;
  }
#endif

  return Cursor_SetCursor_original(_this, texture, hotspot, cursorMode);
}

/**
 * @brief Hook: Model::LoadConfigs
 *
 * Intercepts app config loading to inject custom platform/asset URLs.
 * Original method: reads and returns the game's AppConfig.
 * Our modification: after calling original, overwrites PlatformSettingsUrl
 *   and/or AssetUrlOverride if the user has configured custom values.
 */
typedef AppConfig* (*Model_LoadConfigs_fn)(Model*);
static Model_LoadConfigs_fn Model_LoadConfigs_original = nullptr;

AppConfig* Model_LoadConfigs(Model* _this)
{
  Model_LoadConfigs_original(_this);
  auto config = _this->AppConfig_;

  if (!Config::Get().config_settings_url.empty()) {
    auto new_settings_url       = il2cpp_string_new(Config::Get().config_settings_url.c_str());
    config->PlatformSettingsUrl = new_settings_url;
  }

  if (!Config::Get().config_assets_url_override.empty()) {
    auto new_url             = il2cpp_string_new(Config::Get().config_assets_url_override.c_str());
    config->AssetUrlOverride = new_url;
  }

  return config;
}

/**
 * @brief Hook: UnityEngine.GameObject::SetActive
 *
 * Guards against redundant activation calls that can cause issues.
 * Original method: activates or deactivates a GameObject.
 * Our modification: skips the call if the object is already in the
 *   requested active state (avoids re-triggering OnEnable cascades).
 */
typedef void (*SetActive_hook_fn)(void*, bool);
static SetActive_hook_fn SetActive_hook_original = nullptr;

void SetActive_hook(void* _this, bool active)
{
  static auto IsActiveSelf = il2cpp_resolve_icall_typed<bool(void*)>("UnityEngine.GameObject::get_activeSelf()");

  if (active && IsActiveSelf(_this)) {
    return;
    // __debugbreak();
  }
  return SetActive_hook_original(_this, active);
}

/**
 * @brief Hook: ActionQueueManager::IsQueueUnlocked
 *
 * Allows disabling the action queue feature entirely.
 * Original method: returns whether the action queue is unlocked for the player.
 * Our modification: returns false when queue_enabled config is off,
 *   effectively hiding the queue UI.
 */
typedef bool (*IsQueueEnabled_fn)(void*);
static IsQueueEnabled_fn IsQueueEnabled_original = nullptr;

bool IsQueueEnabled(void* _this)
{
  if (Config::Get().queue_enabled) {
    return IsQueueEnabled_original(_this);
  }

  return false;
}

// ─── Hook Installation ───────────────────────────────────────────────────────

/**
 * @brief Installs development/testing hooks.
 *
 * Hooks:
 *   - Cursor::SetCursor_Injected (cursor override)
 *   - Model::LoadConfigs (config URL injection)
 *   - GameObject::SetActive (activation dedup guard)
 *   - ActionQueueManager::IsQueueUnlocked (queue toggle)
 */
void InstallTestPatches()
{
  auto cursorManager = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Cursor");
  if (!cursorManager.isValidHelper()) {
    ErrorMsg::MissingHelper("UnityEngine", "Cursor");
  } else {
    auto cursorMethod = cursorManager.GetMethod("SetCursor_Injected");
    if (cursorMethod == nullptr) {
      ErrorMsg::MissingMethod("Cursor", "SetCursor_Injected");
    } else {
      MH_INSTALL(cursorMethod, Cursor_SetCursor, Cursor_SetCursor_original);
    }
  }

  auto model = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Model");
  if (!model.isValidHelper()) {
    ErrorMsg::MissingHelper("Core", "Model");
  } else {
    auto load_configs_ptr = model.GetMethod("LoadConfigs");
    if (load_configs_ptr == nullptr) {
      ErrorMsg::MissingMethod("Model", "LoadConfigs");
    } else {
      MH_INSTALL(load_configs_ptr, Model_LoadConfigs, Model_LoadConfigs_original);
    }
  }

  auto battle_target_data =
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BattleTargetData");
  if (!battle_target_data.isValidHelper()) {
    ErrorMsg::MissingHelper("Models", "BattleTargetData");
  } else {
    static auto SetActive =
        il2cpp_resolve_icall_typed<void(void*, bool)>("UnityEngine.GameObject::SetActive(System.Boolean)");
    if (SetActive == nullptr) {
      ErrorMsg::MissingStaticMethod("GameObject", "SetActive");
    } else {
      MH_INSTALL(SetActive, SetActive_hook, SetActive_hook_original);
    }
  }

  auto queue_manager = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!queue_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueManager");
  } else {

    auto is_queue_unlocked = queue_manager.GetMethod("IsQueueUnlocked");
    if (is_queue_unlocked == nullptr) {
      ErrorMsg::MissingStaticMethod("GameObject", "IsQueueUnlocked");
    } else {
      MH_INSTALL(is_queue_unlocked, IsQueueEnabled, IsQueueEnabled_original);
    }
  }
}
