/**
 * @file targeted_diagnostic_registry.cc
 * @brief Machine-enforced registry for temporary and permanent concerns.
 */
#include "targeted_diagnostic_registry.h"

#include "patches/fleet_notification_diagnostics.h"
#include "version.h"

#include <array>

namespace targeted_diagnostic_registry
{
namespace
{
  constexpr targeted_diagnostics::Version kCurrentVersion{
      VERSION_MAJOR,
      VERSION_MINOR,
      VERSION_REVISION,
  };

  constexpr std::array<const targeted_diagnostics::ConcernSpec*, 1> kSpecs{
      &fleet_notification_diagnostics::kConcernSpec,
  };

  static_assert(targeted_diagnostics::ValidateConcernSpecs(kSpecs, kCurrentVersion, true)
                    == targeted_diagnostics::RegistryValidationError::None,
                "A targeted diagnostic concern is invalid, duplicated, or past its required sunset version. "
                "Delete, revise, or promote it intentionally.");
} // namespace

std::span<targeted_diagnostics::Concern* const> Concerns()
{
  static std::array concerns{
      TARGET_DIAGNOSTIC_REGISTER(fleet_notification_diagnostics::Concern()),
  };
  return concerns;
}

std::span<const targeted_diagnostics::ConcernSpec* const> Specs()
{ return kSpecs; }

targeted_diagnostics::Version CurrentVersion()
{ return kCurrentVersion; }
} // namespace targeted_diagnostic_registry
