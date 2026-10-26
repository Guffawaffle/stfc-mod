// [graphics] zoom_label_player_* and zoom_label_non_player_*: how much detail fleet labels in system view show. The
// game picks each label's level of detail from the camera's zoom; these settings override that per fleet kind. The
// camera's own hooks in camera.cc report the zoom here.

#include "config.h"
#include "features/features.h"
#include "features/galaxy.h"
#include "features/ship_shortcut_badges.h"

#include "game/navigation.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <unordered_map>
#include <vector>

namespace
{
// The tracked fleet labels and the zoom state they follow. Each fleet kind is always expanded, always compact, or
// expanded below a zoom threshold.
class FleetLabels
{
public:
  // Set once every hook is in: an untracked OnDisable would leave a freed widget in the map.
  void MarkInstalled()
  { installed_ = true; }

  uint64_t Revision(NavigationLOD* lod) const
  {
    const auto found = widgets_.find(lod);
    return found == widgets_.end() ? 0 : found->second.generation;
  }

  static bool Enabled()
  {
    const auto& config = Config::Get();
    return config.graphics.zoom_label_player.detail != FleetLabelDetail::kNative
           || config.graphics.zoom_label_non_player.detail != FleetLabelDetail::kNative;
  }

  // The level `lod` should show instead of `native`, or nothing when its fleet is not tracked or labels are native.
  std::optional<ZoomLevels> LevelFor(NavigationLOD* lod, ZoomLevels native) const
  {
    const auto* profile = Enabled() ? Profile(lod) : nullptr;
    if (!profile)
      return std::nullopt;
    // Expanded keeps Near where the game would show it, and lifts everything else to Middle.
    const auto expanded = native == ZoomLevels::Near ? ZoomLevels::Near : ZoomLevels::Middle;
    switch (profile->detail) {
      case FleetLabelDetail::kExpanded:
        return expanded;
      case FleetLabelDetail::kCompact:
        return ZoomLevels::Far;
      case FleetLabelDetail::kThreshold:
        if (!camera_.zoom_known)
          return native;
        return ExpandedAt(profile->zoom_threshold) ? expanded : ZoomLevels::Far;
      default:
        return native;
    }
  }

  void Track(NavigationFleetWidget* widget)
  {
    if (!installed_)
      return;
    // Fresh pooled widgets are enabled before BindDataContext assigns their context; forcing an LOD before then
    // fires OnLODChanged callbacks that dereference the unbound fleet data.
    auto* lod = widget->Context() ? widget->lod() : nullptr;
    if (!lod)
      return;
    auto widget_ref = il2cpp::GcHandle::Weak(widget);
    auto lod_ref    = il2cpp::GcHandle::Weak(lod);
    if (!widget_ref || !lod_ref)
      return;
    widgets_.insert_or_assign(lod, Binding{std::move(widget_ref), std::move(lod_ref), ++generation_});
    if (camera_.zoom_known && Enabled())
      lod->UpdateLOD(camera_.zoom_level);
  }

  void Untrack(NavigationFleetWidget* widget)
  {
    if (!installed_)
      return;
    auto* lod = widget->lod();
    if (!lod)
      return;
    const auto found = widgets_.find(lod);
    if (found == widgets_.end() || found->second.widget.Get() != widget)
      return;
    auto widget_root = il2cpp::GcHandle::Strong(widget);
    auto lod_root    = il2cpp::GcHandle::Strong(lod);
    widgets_.erase(found);
    // Give a reused widget back to the game at the game's own level.
    if (widget->Context() && camera_.zoom_known && Enabled()) {
      lod->SetTargetLevel(camera_.zoom_level);
      lod->UpdateLOD(camera_.zoom_level);
      // UpdateLOD can bind a replacement owner; do not overwrite its bookkeeping.
      if (!widgets_.contains(lod))
        lod->SetTargetLevel(camera_.zoom_level);
    }
  }

  // The camera `zoom` is about to show `depth`. Labels only apply in a solar system, and a new camera starts over.
  void OnDepth(NavigationZoom* zoom, NodeDepth depth)
  {
    if (!installed_)
      return;
    if (depth == NodeDepth::SolarSystem)
      Follow(zoom);
    else if (camera_.zoom == zoom)
      camera_ = {};
  }

  // After the camera's own update: picks up the zoom level and re-applies labels when they change.
  void OnZoomed(NavigationZoom* zoom)
  {
    if (!installed_ || zoom->depth() != NodeDepth::SolarSystem)
      return;
    Follow(zoom);
    const bool was_known = camera_.zoom_known;
    camera_.zoom_level   = zoom->zoomLevel();
    camera_.zoom_known   = true;
    if (UsesThreshold()) {
      camera_.normalized_zoom = zoom->NormalizedZoom();
      UpdateThreshold();
    } else if (!was_known) {
      ApplyAll();
    }
  }

private:
  struct Binding {
    il2cpp::GcHandle widget, lod;
    uint64_t         generation{};
  };

  const FleetLabelProfile* Profile(NavigationLOD* lod) const
  {
    const auto it = widgets_.find(lod);
    if (it == widgets_.end())
      return nullptr;
    const auto generation  = it->second.generation;
    auto       widget_root = il2cpp::GcHandle::Strong(it->second.widget.Get());
    auto*      widget      = widget_root.Get<NavigationFleetWidget>();
    if (!widget || !unity::Alive(widget))
      return nullptr;
    auto       fleet_root = il2cpp::GcHandle::Strong(widget->Context());
    auto*      fleet      = fleet_root.Get<FleetDeployedData>();
    const auto current    = widgets_.find(lod);
    if (current == widgets_.end() || current->second.generation != generation)
      return nullptr;
    if (!fleet)
      return nullptr;
    const auto kind = fleet->TryCall<"get_FleetType", DeployedFleetType>();
    if (!kind || Revision(lod) != generation)
      return nullptr;
    const auto& config = Config::Get();
    return kind.value == DeployedFleetType::Player ? &config.graphics.zoom_label_player
                                                   : &config.graphics.zoom_label_non_player;
  }

  static bool UsesThreshold()
  {
    const auto& config = Config::Get();
    return config.graphics.zoom_label_player.detail == FleetLabelDetail::kThreshold
           || config.graphics.zoom_label_non_player.detail == FleetLabelDetail::kThreshold;
  }

  // Labels expand when zoomed in at least as far as the threshold. 0 never expands, 1 always does.
  bool ExpandedAt(float threshold) const
  { return threshold > 0.0f && (threshold >= 1.0f || camera_.normalized_zoom <= threshold); }

  void Follow(NavigationZoom* zoom)
  {
    if (camera_.zoom != zoom)
      camera_ = {.zoom = zoom};
  }

  // UpdateLOD goes through the hook, which picks each label's level.
  void ApplyAll()
  {
    if (!Enabled())
      return;
    // Native updates can release/rebind any widget, invalidating map iterators and old receiver identities.
    std::vector<std::pair<NavigationLOD*, uint64_t>> pending;
    pending.reserve(widgets_.size());
    for (const auto& [key, binding] : widgets_)
      pending.emplace_back(key, binding.generation);
    for (const auto& [key, generation] : pending) {
      auto found = widgets_.find(key);
      if (found == widgets_.end() || found->second.generation != generation)
        continue;
      auto  widget_root = il2cpp::GcHandle::Strong(found->second.widget.Get());
      auto  lod_root    = il2cpp::GcHandle::Strong(found->second.lod.Get());
      auto* widget      = widget_root.Get<NavigationFleetWidget>();
      auto* lod         = lod_root.Get<NavigationLOD>();
      if (!widget || !lod || !unity::Alive(widget) || !unity::Alive(lod)) {
        widgets_.erase(key);
        continue;
      }
      auto* context = widget->Context();
      found         = widgets_.find(key);
      if (context && found != widgets_.end() && found->second.generation == generation)
        lod->UpdateLOD(camera_.zoom_level);
    }
  }

  // Re-applies labels only when a threshold profile crosses its threshold, not on every zoom step.
  void UpdateThreshold()
  {
    const auto& graphics = Config::Get().graphics;
    const auto  expanded = [this](const FleetLabelProfile& profile) {
      return profile.detail == FleetLabelDetail::kThreshold && ExpandedAt(profile.zoom_threshold);
    };
    const bool player     = expanded(graphics.zoom_label_player);
    const bool non_player = expanded(graphics.zoom_label_non_player);
    if (camera_.threshold_known && player == camera_.player_expanded && non_player == camera_.non_player_expanded)
      return;
    camera_.threshold_known     = true;
    camera_.player_expanded     = player;
    camera_.non_player_expanded = non_player;
    ApplyAll();
    spdlog::debug("[FleetLabels] threshold crossed at normalized zoom {:.4f}", camera_.normalized_zoom);
  }

  // What is known of the system camera's zoom. A new camera starts over.
  struct Camera {
    NavigationZoom* zoom            = nullptr;
    bool            zoom_known      = false;
    ZoomLevels      zoom_level      = ZoomLevels::None;
    float           normalized_zoom = 0.0f;
    // Whether player and non-player threshold labels were last expanded, once threshold_known.
    bool threshold_known     = false;
    bool player_expanded     = false;
    bool non_player_expanded = false;
  };

  bool                                        installed_ = false;
  uint64_t                                    generation_{};
  std::unordered_map<NavigationLOD*, Binding> widgets_;
  Camera                                      camera_;
};
// Weak handles must be released while IL2CPP is alive, never by static destruction after its teardown.
auto& fleet_labels = *new FleetLabels;

// --- Hooks ---

void NavigationLOD_UpdateLOD_Hook(auto original, NavigationLOD* _this, ZoomLevels level)
{
  const auto revision  = fleet_labels.Revision(_this);
  const auto effective = fleet_labels.LevelFor(_this, level);
  if (!effective || fleet_labels.Revision(_this) != revision) {
    const auto galaxy = galaxy_controls::LodLevel(_this, level);
    if (galaxy != level)
      _this->SetTargetLevel(galaxy);
    return original(_this, galaxy);
  }
  // The target level is set on both sides of the update so the game's own bookkeeping agrees with ours.
  _this->SetTargetLevel(*effective);
  original(_this, *effective);
  if (revision && fleet_labels.Revision(_this) == revision)
    _this->SetTargetLevel(*effective);
}

void NavigationFleetWidget_OnEnable_Hook(auto original, NavigationFleetWidget* _this)
{
  original(_this);
  fleet_labels.Track(_this);
  ship_shortcut_badges::Bind(_this);
}

void NavigationFleetWidget_OnDidBindContext_Hook(auto original, NavigationFleetWidget* _this)
{
  original(_this);
  fleet_labels.Track(_this);
  ship_shortcut_badges::Bind(_this);
}

void NavigationFleetWidget_OnDisable_Hook(auto original, NavigationFleetWidget* _this)
{
  ship_shortcut_badges::Release(_this);
  fleet_labels.Untrack(_this);
  original(_this);
}

void NavigationFleetWidget_OnAboutToReleaseContext_Hook(auto original, NavigationFleetWidget* _this)
{
  ship_shortcut_badges::Release(_this);
  fleet_labels.Untrack(_this);
  original(_this);
}

bool InstallFleetLabels()
{
  // Installed whether or not zoom_label_* is set, so labels can be switched at runtime. Missing fields and
  // properties (_lod, Context, FleetType) read as null or zero, which the hooks treat as native labels. The zoom
  // reports come from camera.cc's NavigationZoom.Update hook, so that method has to exist too.
  if (!EnsureNavigationZoomUpdates() || !EnsureNavigationFleetLifecycle())
    return false;
  // camera.cc's zoom hooks report here whether or not this installed; they only count once it has.
  if (!EnsureNavigationLodUpdates())
    return false;
  fleet_labels.MarkInstalled();
  return true;
}
} // namespace

bool EnsureNavigationLodUpdates()
{
  static const bool installed = [] {
    il2cpp::HookSet hooks("NavigationLodUpdates");
    IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(NavigationLOD::Class::Method<void(ZoomLevels)>("UpdateLOD")),
                    NavigationLOD_UpdateLOD_Hook);
    return hooks.Commit();
  }();
  return installed;
}

bool EnsureNavigationFleetLifecycle()
{
  // The label and shortcut-badge features share the same pooled widget lifecycle.
  // One terminal owner serves both, independently of either feature's display setting.
  static const bool installed = [] {
    il2cpp::HookSet hooks("NavigationFleetLifecycle");
    IL2CPP_STAGE(hooks, NavigationFleetWidget::Class::Method("OnEnable"), NavigationFleetWidget_OnEnable_Hook);
    IL2CPP_STAGE(hooks, NavigationFleetWidget::Class::Method("OnDisable"), NavigationFleetWidget_OnDisable_Hook);
    IL2CPP_STAGE(hooks, NavigationFleetWidget::Class::Method("OnDidBindContext"),
                 NavigationFleetWidget_OnDidBindContext_Hook);
    IL2CPP_STAGE(hooks, NavigationFleetWidget::Class::Method("OnAboutToReleaseContext"),
                 NavigationFleetWidget_OnAboutToReleaseContext_Hook);
    return hooks.Commit();
  }();
  return installed;
}

void FleetLabelsFollowDepth(NavigationZoom* zoom, NodeDepth depth)
{ fleet_labels.OnDepth(zoom, depth); }

void FleetLabelsAfterZoom(NavigationZoom* zoom)
{ fleet_labels.OnZoomed(zoom); }

const Feature kFleetLabels{"FleetLabels", &Config::Patches::zoomhooks, InstallFleetLabels};
