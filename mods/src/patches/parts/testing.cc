/**
 * @file testing.cc
 * @brief Development and testing hooks.
 *
 * Contains hooks primarily used during mod development and testing:
 *   - Cursor override: replaces Unity's custom cursor with the OS default arrow.
 *   - Config URL override: injects custom platform settings and asset URLs.
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
#include <spud/detour.h>
#include <spud/signature.h>

// ─── Hook Functions ──────────────────────────────────────────────────────────

/**
 * @brief Hook: UnityEngine.Cursor::SetCursor_Injected
 *
 * Intercepts cursor rendering to force the OS default arrow.
 * Original method: sets a custom cursor texture, hotspot, and mode.
 * Our modification: when allow_cursor is false (Windows only), replaces the
 *   cursor with IDC_ARROW and releases any Unity cursor clipping.
 */
void Cursor_SetCursor(auto original, void* _this, ptrdiff_t texture, Vector2* hotspot, int cursorMode)
{
#if _WIN32
  if (!Config::Get().allow_cursor) {
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    ClipCursor(nullptr); // free cursor from any Unity clipping
    return;
  }
#endif

  return original(_this, texture, hotspot, cursorMode);
}

/**
 * @brief Hook: ActionQueueManager::IsQueueUnlocked
 *
 * Allows disabling the action queue feature entirely.
 * Original method: returns whether the action queue is unlocked for the player.
 * Our modification: returns false when queue_enabled config is off,
 *   effectively hiding the queue UI.
 */
bool IsQueueEnabled(auto original, void* _this)
{
  if (Config::Get().queue_enabled) {
    return original(_this);
  }

  return false;
}

#if !defined(STFC_ENABLE_DEV_SCIENCE_TOOLS) || STFC_ENABLE_DEV_SCIENCE_TOOLS
void InstallTestingConfigOverrideHooks();
#else
static void InstallTestingConfigOverrideHooks()
{}
#endif

// ─── Hook Installation ───────────────────────────────────────────────────────

/**
 * @brief Installs development/testing hooks.
 *
 * Hooks:
 *   - Cursor::SetCursor_Injected (cursor override)
 *   - Model::LoadConfigs (config URL injection; dev-only helper)
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
      SPUD_STATIC_DETOUR(cursorMethod, Cursor_SetCursor);
    }
  }

  InstallTestingConfigOverrideHooks();

// this method is not accessible in v48084
//   auto battle_target_data =
//       il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "BattleTargetData");
//   if (!battle_target_data.isValidHelper()) {
//     ErrorMsg::MissingHelper("Models", "BattleTargetData");
//   } else {
//     static auto SetActive =
//         il2cpp_resolve_icall_typed<void(void*, bool)>("UnityEngine.GameObject::SetActive(System.Boolean)");
//     if (SetActive == nullptr) {
//       ErrorMsg::MissingStaticMethod("GameObject", "SetActive");
//     } else {
//       SPUD_STATIC_DETOUR(SetActive, SetActive_hook);
//     }
//   }

  auto queue_manager = il2cpp_get_class_helper("Assembly-CSharp", "Prime.ActionQueue", "ActionQueueManager");
  if (!queue_manager.isValidHelper()) {
    ErrorMsg::MissingHelper("ActionQueue", "ActionQueueManager");
  } else {

    auto is_queue_unlocked = queue_manager.GetMethod("IsQueueUnlocked");
    if (is_queue_unlocked == nullptr) {
      ErrorMsg::MissingStaticMethod("GameObject", "IsQueueUnlocked");
    } else {
      SPUD_STATIC_DETOUR(is_queue_unlocked, IsQueueEnabled);
    }
  }
}
