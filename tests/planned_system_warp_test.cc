#include "planned_system_warp_policy.h"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
using namespace planned_system_warp;
void Check(bool value, const std::string& message)
{
  if (!value)
    throw std::runtime_error(message);
}
Evidence Valid()
{
  Evidence value;
  value.primary = value.galaxy = true;
  value.selected_fleet_id = value.course_fleet_id = 9007199254740993LL;
  value.start_system = value.fleet_system = 42;
  value.target_node_id                    = 73;
  value.course_type                       = 0;
  value.target_action                     = -1;
  value.path_result                       = 1;
  value.target_depth                      = 2;
  value.start_instance = value.fleet_instance = 0;
  value.native_can_navigate = value.is_warp = true;
  value.in_use = value.is_recall = value.is_wormhole = value.has_toll = false;
  value.system_position                                               = Vec3{};
  value.projected_context_position = value.target_galaxy_position = Vec3{10.0f, -20.0f, 0.0f};
  value.fleet_identity                                            = {11, 11};
  value.context_identity                                          = {22, 22};
  value.path_identity                                             = {33, 33};
  value.tree_node                                                 = 0;
  return value;
}
void OrdinaryAndMissing()
{
  const auto valid   = Valid();
  const auto request = Prepare(valid);
  Check(request && request->system_position == Vec3{} && request->node_id == 73
            && request->fleet_id == 9007199254740993LL,
        "valid ordinary course lost exact IDs or finite zero position");
  auto value          = valid;
  value.target_action = 1;
  Check(Prepare(value).has_value(), "ordinary Orbit target action was rejected");
  value.selected_fleet_id = value.course_fleet_id = 0;
  Check(Prepare(value).has_value(), "an observed fleet ID zero became unavailable");
  value.start_instance = value.fleet_instance = -3;
  Check(Prepare(value).has_value(), "matching instance identity acquired an invented positivity rule");

  struct Missing {
    const char* name;
    void (*clear)(Evidence&);
  };
  const std::array missing{
      Missing{"selected fleet", [](auto& v) { v.selected_fleet_id.reset(); }},
      Missing{"course fleet", [](auto& v) { v.course_fleet_id.reset(); }},
      Missing{"native validator", [](auto& v) { v.native_can_navigate.reset(); }},
      Missing{"course type", [](auto& v) { v.course_type.reset(); }},
      Missing{"target action", [](auto& v) { v.target_action.reset(); }},
      Missing{"in use", [](auto& v) { v.in_use.reset(); }},
      Missing{"warp", [](auto& v) { v.is_warp.reset(); }},
      Missing{"recall", [](auto& v) { v.is_recall.reset(); }},
      Missing{"wormhole", [](auto& v) { v.is_wormhole.reset(); }},
      Missing{"toll", [](auto& v) { v.has_toll.reset(); }},
      Missing{"path result", [](auto& v) { v.path_result.reset(); }},
      Missing{"target depth", [](auto& v) { v.target_depth.reset(); }},
      Missing{"start system", [](auto& v) { v.start_system.reset(); }},
      Missing{"fleet system", [](auto& v) { v.fleet_system.reset(); }},
      Missing{"start instance", [](auto& v) { v.start_instance.reset(); }},
      Missing{"fleet instance", [](auto& v) { v.fleet_instance.reset(); }},
      Missing{"target node", [](auto& v) { v.target_node_id.reset(); }},
      Missing{"system position", [](auto& v) { v.system_position.reset(); }},
      Missing{"projected context position", [](auto& v) { v.projected_context_position.reset(); }},
      Missing{"target galaxy position", [](auto& v) { v.target_galaxy_position.reset(); }},
      Missing{"TreeNode observation", [](auto& v) { v.tree_node.reset(); }},
      Missing{"expected fleet source", [](auto& v) { v.fleet_identity.expected.reset(); }},
      Missing{"current fleet source", [](auto& v) { v.fleet_identity.current.reset(); }},
      Missing{"expected context source", [](auto& v) { v.context_identity.expected.reset(); }},
      Missing{"current context source", [](auto& v) { v.context_identity.current.reset(); }},
      Missing{"course path reference", [](auto& v) { v.path_identity.expected.reset(); }},
      Missing{"context path reference", [](auto& v) { v.path_identity.current.reset(); }},
  };
  for (const auto& field : missing) {
    value = valid;
    field.clear(value);
    Check(!Prepare(value), std::string("missing observation authorized movement: ") + field.name);
  }
  value         = valid;
  value.primary = false;
  Check(!Prepare(value), "qualification authorized movement without primary input");
  value        = valid;
  value.galaxy = false;
  Check(!Prepare(value), "retained course was authorized outside galaxy navigation");
}
void RefusalsAndIdentity()
{
  auto value = Valid();
  for (auto member : {&Evidence::in_use, &Evidence::is_recall, &Evidence::is_wormhole, &Evidence::has_toll}) {
    value         = Valid();
    value.*member = true;
    Check(!Prepare(value), "an in-use or special travel course was authorized");
  }
  for (auto member : {&Evidence::native_can_navigate, &Evidence::is_warp}) {
    value         = Valid();
    value.*member = false;
    Check(!Prepare(value), "a native-refused or non-warp course was authorized");
  }
  for (auto action : {-2, 0, 2, 100}) {
    value               = Valid();
    value.target_action = action;
    Check(!Prepare(value), "a special target action was authorized");
  }
  value             = Valid();
  value.course_type = 1;
  Check(!Prepare(value), "non-planned course type was authorized");
  value             = Valid();
  value.path_result = 0;
  Check(!Prepare(value), "invalid path result was authorized");
  value              = Valid();
  value.target_depth = 1;
  Check(!Prepare(value), "non-system target depth was authorized");
  value                 = Valid();
  value.course_fleet_id = *value.selected_fleet_id + 1;
  Check(!Prepare(value), "course for another fleet was authorized");
  value              = Valid();
  value.fleet_system = 43;
  Check(!Prepare(value), "course start and current fleet system mismatch was accepted");
  value                = Valid();
  value.fleet_instance = 1;
  Check(!Prepare(value), "course start and current fleet instance mismatch was accepted");
  for (auto id : {std::int64_t{0}, std::int64_t{-1}}) {
    value                = Valid();
    value.target_node_id = id;
    Check(!Prepare(value), "nonpositive target node was authorized");
    value              = Valid();
    value.start_system = value.fleet_system = id;
    Check(!Prepare(value), "nonpositive start system was authorized");
  }
  for (auto member : {&Evidence::fleet_identity, &Evidence::context_identity, &Evidence::path_identity}) {
    value                   = Valid();
    (value.*member).current = 44;
    Check(!Prepare(value), "mismatched native reference identities were accepted");
    value         = Valid();
    value.*member = {0, 0};
    Check(!Prepare(value), "known null native reference identity authorized movement");
  }
  value           = Valid();
  value.tree_node = 77;
  value.tree_id   = value.target_node_id;
  value.tree_type = 0;
  Check(Prepare(value).has_value(), "matching observed system TreeNode was refused");
  value.tree_id.reset();
  Check(!Prepare(value), "present TreeNode with unknown ID was accepted");
  value.tree_id = 74;
  Check(!Prepare(value), "another TreeNode ID was accepted");
  value.tree_id = value.target_node_id;
  value.tree_type.reset();
  Check(!Prepare(value), "present TreeNode with unknown type was accepted");
  value.tree_type = 1;
  Check(!Prepare(value), "a nonsystem TreeNode was accepted");
}
void Positions()
{
  auto value                       = Valid();
  value.target_galaxy_position     = Vec3{};
  value.projected_context_position = Vec3{std::nextafter(0.01f, 0.0f), -0.009f, 0.0f};
  Check(Prepare(value).has_value(), "finite projection roundoff below the exact old tolerance was refused");
  for (auto axis : {&Vec3::x, &Vec3::y, &Vec3::z}) {
    value                        = Valid();
    value.target_galaxy_position = value.projected_context_position = Vec3{};
    (*value.projected_context_position).*axis                       = 0.01f;
    Check(!Prepare(value), "projection at the exclusive tolerance boundary was accepted");
    (*value.projected_context_position).*axis = -0.01f;
    Check(!Prepare(value), "negative projection tolerance boundary was accepted");
  }
  for (auto member :
       {&Evidence::system_position, &Evidence::projected_context_position, &Evidence::target_galaxy_position})
    for (auto axis : {&Vec3::x, &Vec3::y, &Vec3::z})
      for (float invalid : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()}) {
        value                    = Valid();
        (*(value.*member)).*axis = invalid;
        Check(!Prepare(value), "nonfinite position component authorized movement");
      }
}
void Routing()
{
  std::size_t combinations = 0, ineligible = 0, eligible_secondary = 0;
  for (bool primary : {false, true})
    for (bool secondary : {false, true})
      for (bool galaxy : {false, true})
        for (auto visibility : {Visibility::Unavailable, Visibility::Hidden, Visibility::Visible})
          for (bool require_visible : {false, true}) {
            ++combinations;
            const bool eligible = ViewerEligible(primary, galaxy, visibility, require_visible);
            if (primary && require_visible && !galaxy && visibility != Visibility::Visible) {
              Check(!eligible, "a simultaneous secondary bypassed the nongalaxy primary visibility admission guard");
              ++ineligible;
            } else {
              Check(eligible, "viewer admission rejected secondary-only, native-visible or galaxy-recovery input");
            }
            // The native consumer may honor secondary priority only after this
            // shared admission decision; it falls through when ineligible.
            if (eligible && secondary)
              ++eligible_secondary;
            const auto route = RoutePrimary(primary, secondary, galaxy, visibility, require_visible);
            if (!eligible)
              Check(route == PrimaryRoute::Ignore, "ineligible viewer input still selected a primary action");
            if (!primary || secondary) {
              Check(route == PrimaryRoute::Ignore && !Consumes(route),
                    "nonprimary or secondary-owned input dispatched a primary action");
            } else if (visibility == Visibility::Visible) {
              Check(route == PrimaryRoute::Native && Consumes(route), "visible target lost its native primary flow");
            } else if (galaxy) {
              Check(route == PrimaryRoute::Retained && Consumes(route),
                    "hidden galaxy branch depended on visibility preference");
              auto refused                = Valid();
              refused.native_can_navigate = false;
              Check(!Prepare(refused) && Consumes(route),
                    "refused recovery allowed fallthrough to a hidden native handler");
              Check(Prepare(Valid()).has_value() && Consumes(route),
                    "qualified hidden recovery did not consume its input");
            } else {
              const auto expected = require_visible ? PrimaryRoute::Ignore : PrimaryRoute::Native;
              Check(route == expected && Consumes(route) == !require_visible, "nongalaxy visibility behavior changed");
            }
          }
  Check(combinations == 48, "primary/secondary routing matrix did not cover every input combination");
  Check(ineligible == 4 && eligible_secondary == 22,
        "viewer admission was not tested separately before secondary priority");
}
} // namespace
int main()
{
  try {
    OrdinaryAndMissing();
    RefusalsAndIdentity();
    Positions();
    Routing();
    std::cout << "Planned system warp qualification and hidden-action routing fixtures passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
