#pragma once

// Ordinary retained galaxy-course qualification. All observations are owned
// values; reference tokens are compared only and are never dereferenced here.
#include <cmath>
#include <cstdint>
#include <optional>

namespace planned_system_warp
{
struct Vec3 {
  float       x{}, y{}, z{};
  friend bool operator==(const Vec3&, const Vec3&) = default;
};
struct ReferencePair {
  std::optional<std::uintptr_t> expected, current;
};
inline bool Matches(const ReferencePair& reference)
{
  return reference.expected && reference.current && *reference.expected != 0 && reference.expected == reference.current;
}
struct Evidence {
  bool                        primary = false, galaxy = false;
  std::optional<std::int64_t> selected_fleet_id, course_fleet_id, start_system, fleet_system, target_node_id, tree_id;
  std::optional<std::int32_t> course_type, target_action, path_result, target_depth, start_instance, fleet_instance,
      tree_type;
  std::optional<bool> native_can_navigate, in_use, is_warp, is_recall, is_wormhole, has_toll;
  std::optional<Vec3> system_position, projected_context_position, target_galaxy_position;
  // Fleet/context compare rooted source identity before and after collection.
  // Path compares CourseData.PathFinderResult with context.PathFindResult.
  ReferencePair fleet_identity, context_identity, path_identity;
  // An observed zero is a known null TreeNode. An unavailable read is nullopt.
  std::optional<std::uintptr_t> tree_node;
};
struct Request {
  Vec3         system_position;
  std::int64_t node_id, fleet_id;
  friend bool  operator==(const Request&, const Request&) = default;
};
inline bool Finite(const Vec3& value)
{ return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
inline bool MatchesPosition(const Vec3& projected, const Vec3& target)
{
  return Finite(projected) && Finite(target) && std::abs(projected.x - target.x) < 0.01f
         && std::abs(projected.y - target.y) < 0.01f && std::abs(projected.z - target.z) < 0.01f;
}

inline std::optional<Request> Prepare(const Evidence& value)
{
  if (!value.primary || !value.galaxy || value.native_can_navigate != true)
    return {};
  if (!Matches(value.fleet_identity) || !Matches(value.context_identity) || !Matches(value.path_identity))
    return {};
  if (!value.selected_fleet_id || value.selected_fleet_id != value.course_fleet_id)
    return {};
  // Planned=0; preview TargetAction Null=-1 or Orbit=1; valid path=1.
  // Recall, wormhole, toll and other special travel retain their native flow.
  if (value.course_type != 0 || (value.target_action != -1 && value.target_action != 1) || value.in_use != false
      || value.is_warp != true || value.is_recall != false || value.is_wormhole != false || value.has_toll != false
      || value.path_result != 1)
    return {};
  if (value.target_depth != 2 || !value.start_system || *value.start_system <= 0
      || value.start_system != value.fleet_system || !value.start_instance
      || value.start_instance != value.fleet_instance)
    return {};
  if (!value.target_node_id || *value.target_node_id <= 0 || !value.system_position || !Finite(*value.system_position)
      || !value.projected_context_position || !value.target_galaxy_position
      || !MatchesPosition(*value.projected_context_position, *value.target_galaxy_position))
    return {};
  if (!value.tree_node)
    return {};
  // Dismissal may clear TreeNode. A present node must still be this system.
  if (*value.tree_node != 0 && (value.tree_id != value.target_node_id || value.tree_type != 0))
    return {};
  return Request{*value.system_position, *value.target_node_id, *value.selected_fleet_id};
}

enum class Visibility { Unavailable, Hidden, Visible };
enum class PrimaryRoute { Ignore, Native, Retained };
inline constexpr bool ViewerEligible(bool primary, bool galaxy, Visibility visibility, bool require_visible)
{
  // Admission precedes secondary priority. Galaxy primary recovery is the
  // deliberate exception to the configured primary-target visibility guard.
  return !primary || !require_visible || visibility == Visibility::Visible || galaxy;
}
inline constexpr PrimaryRoute RoutePrimary(bool primary, bool secondary, bool galaxy, Visibility visibility,
                                           bool require_visible)
{
  // The existing secondary viewer action owns a simultaneous press. This
  // function routes only the primary action, so it must not also request travel.
  if (!primary || secondary)
    return PrimaryRoute::Ignore;
  if (visibility == Visibility::Visible)
    return PrimaryRoute::Native;
  // A hidden/unknown galaxy target never falls through to the hidden native
  // handler, even if retained-course qualification or invocation later refuses.
  if (galaxy)
    return PrimaryRoute::Retained;
  return require_visible ? PrimaryRoute::Ignore : PrimaryRoute::Native;
}
inline constexpr bool Consumes(PrimaryRoute route)
{ return route != PrimaryRoute::Ignore; }
} // namespace planned_system_warp
