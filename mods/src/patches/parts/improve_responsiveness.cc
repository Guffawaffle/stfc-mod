/**
 * @file improve_responsiveness.cc
 * @brief Reduces screen-transition blur duration for snappier UI.
 *
 * The game plays a blur animation during scene transitions (e.g. opening menus).
 * The default duration feels sluggish. This patch overrides the blur time in
 * TransitionManager::Awake with a user-configurable value, clamped to a safe
 * range to avoid visual glitches.
 */
#include "config.h"
#include "errormsg.h"
#include "prime/TransitionManager.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <hook/hook.h>

/**
 * @brief Hook: TransitionManager::Awake
 *
 * Intercepts transition manager initialization to override blur duration.
 * Original method: initializes the TransitionManager and its blur controller.
 * Our modification: after calling original, overwrites _blurTime with the
 *   user's configured transition_time (clamped to [0.02, 1.0] seconds).
 */
typedef int64_t (*TransitionManager_Awake_fn)(TransitionManager*);
static TransitionManager_Awake_fn TransitionManager_Awake_original = nullptr;

int64_t TransitionManager_Awake(TransitionManager* a1)
{
  spdlog::debug("Adjusting screen transitions to {}", Config::Get().transition_time);
  auto r                         = TransitionManager_Awake_original(a1);
  a1->SBlurController->_blurTime = std::clamp(Config::Get().transition_time, 0.02f, 1.0f);
  return r;
}

/**
 * @brief Hook: TransitionManager::OnEnable
 *
 * Intercepts OnEnable to suppress additional transition re-init.
 * Original method: performs setup when the TransitionManager becomes active.
 * Our modification: returns immediately (no-op) to prevent the manager from
 *   resetting blur state after Awake already configured it.
 */
int64_t TransitionManager_OnEnable(TransitionManager* a1)
{
  return 0;
}

/**
 * @brief Installs the screen-transition speed hook.
 *
 * Hooks TransitionManager::Awake to apply the user's transition_time setting.
 * Note: OnEnable hook is defined but not installed here (unused).
 */
void InstallImproveResponsivenessHooks()
{
  auto transition_manager_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionManager");
  if (!transition_manager_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("LoadingScreen", "TransitionManager");
  } else {
    auto awake = transition_manager_helper.GetMethod("Awake");
    if (awake == nullptr) {
      ErrorMsg::MissingMethod("TransitionManager", "Awake");
    } else {
      MH_INSTALL(awake, TransitionManager_Awake, TransitionManager_Awake_original);
    }
  }
}
