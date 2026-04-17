/**
 * @file buff_fixes.cc
 * @brief Corrects buff condition evaluation for out-of-dock power display.
 *
 * The game's BuffService checks various conditions to decide which buffs apply.
 * One condition (CondSelfAtStation) incorrectly gates certain power calculations,
 * causing displayed power to differ from actual combat power when undocked.
 * This patch forces that condition to return false so buff totals reflect
 * true out-of-dock strength.
 */
#include "config.h"
#include "errormsg.h"
#include "prime_types.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/IBuffComparer.h>
#include <prime/IBuffData.h>

#include "hook/detour.h"

/**
 * @brief Hook: BuffService::IsBuffConditionMet
 *
 * Intercepts buff condition checks to fix out-of-dock power display.
 * Original method: evaluates whether a buff condition is satisfied for the
 *   current game state (e.g. "is the player docked at a station?").
 * Our modification: forces CondSelfAtStation to always return false, so buffs
 *   that would only apply when docked are excluded from power calculations,
 *   giving an accurate undocked power reading.
 */
MH_HOOK(bool, BuffService_IsBuffConditionMet, void* _this, BuffCondition currentCondition,
                                           IBuffComparer *buffComparer, IBuffData *buffToCompare,
                                           bool excludeFactionBuffs, bool isAllianceLoyalty)
{
  switch (currentCondition) {
    case BuffCondition::CondSelfAtStation:
      return false;

    default:
      break;
  }

  return BuffService_IsBuffConditionMet_original(_this, currentCondition, buffComparer, buffToCompare, excludeFactionBuffs, isAllianceLoyalty);
}

/**
 * @brief Installs the buff condition fix hook.
 *
 * Only active when `use_out_of_dock_power` is enabled in config.
 * Hooks BuffService::IsBuffConditionMet to correct power display.
 */
void InstallBuffFixHooks()
{
  if (Config::Get().use_out_of_dock_power) {
    auto buffHelper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Services", "BuffService");
    if (!buffHelper.isValidHelper()) {
      ErrorMsg::MissingHelper("Services", "BuffService");
    } else {
      if (const auto ptr = buffHelper.GetMethod("IsBuffConditionMet"); ptr == nullptr) {
        ErrorMsg::MissingMethod("BuffService", "IsBuffConditionMet");
      } else {
        MH_ATTACH(ptr, BuffService_IsBuffConditionMet);
      }
    }
  }
}
