/**
 * @file targeted_diagnostic_registry.h
 * @brief Compile-time inventory for targeted diagnostic concerns.
 */
#pragma once

#include "targeted_diagnostics.h"

#include <span>

namespace targeted_diagnostic_registry
{
[[nodiscard]] std::span<targeted_diagnostics::Concern* const>           Concerns();
[[nodiscard]] std::span<const targeted_diagnostics::ConcernSpec* const> Specs();
[[nodiscard]] targeted_diagnostics::Version                             CurrentVersion();
} // namespace targeted_diagnostic_registry
