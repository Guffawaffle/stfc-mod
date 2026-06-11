/**
 * @file sidecar_local_dispatch_context.h
 * @brief Provenance metadata for copied payloads crossing the sidecar-local async boundary.
 */
#pragma once

#include "patches/gameplay_dispatch_context.h"

#include <string>
#include <string_view>

struct SidecarLocalDispatchContext {
  GameplayDispatchContext dispatch;
  std::string             evidence_kind;
  std::string             classification;
  std::string             boundary;
  std::string             validation;
};

inline SidecarLocalDispatchContext sidecar_local_dispatch_context(const GameplayDispatchContext& dispatch,
                                                                  std::string_view evidence_kind,
                                                                  std::string_view classification,
                                                                  std::string_view boundary,
                                                                  std::string_view validation)
{
  return SidecarLocalDispatchContext{
      dispatch,
      std::string(evidence_kind),
      std::string(classification),
      std::string(boundary),
      std::string(validation),
  };
}
