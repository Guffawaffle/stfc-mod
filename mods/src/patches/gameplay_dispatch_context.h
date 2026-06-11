/**
 * @file gameplay_dispatch_context.h
 * @brief Lightweight provenance metadata for gameplay-adjacent dispatches.
 */
#pragma once

#include <string>
#include <string_view>

struct GameplayDispatchContext {
  std::string source;
  std::string owner;
  std::string seam;
  std::string reason;
  std::string effect;
};

inline GameplayDispatchContext gameplay_dispatch_context(std::string_view source,
                                                         std::string_view owner,
                                                         std::string_view seam,
                                                         std::string_view reason,
                                                         std::string_view effect)
{
  return GameplayDispatchContext{std::string(source),
                                 std::string(owner),
                                 std::string(seam),
                                 std::string(reason),
                                 std::string(effect)};
}
