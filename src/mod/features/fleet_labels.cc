// [graphics] zoom_label_player_*, zoom_label_non_player_* and zoom_label_base_* control system label detail. The
// game picks each label's level of detail from the camera's zoom; these settings override that per label kind. The
// camera's own hooks in camera.cc report the zoom here.

#include "config.h"
#include "features/features.h"
#include "features/galaxy.h"
#include "features/ship_shortcut_badges.h"

#include "game/navigation.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
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
  { installed_ = fleets_installed_ = true; }

  void MarkBasesInstalled()
  { installed_ = bases_installed_ = true; }

  // Settings writers do no managed work. The next live system camera update owns the refresh.
  void Refresh()
  { refresh_pending_ = true; }

  uint64_t Revision(NavigationLOD* lod) const
  {
    const auto found = widgets_.find(lod);
    return found == widgets_.end() ? 0 : found->second.generation;
  }

  bool IsBase(NavigationLOD* lod) const
  {
    const auto found = widgets_.find(lod);
    return found != widgets_.end() && found->second.base && found->second.lod.Get() == lod;
  }

  uint64_t VisibilityRevision(NavigationZoomEventHandler* handler) const
  {
    const auto found = visibility_.find(handler);
    if (found == visibility_.end() || found->second.size() != 1)
      return 0;
    const auto [key, generation] = found->second.front();
    const auto binding           = widgets_.find(key);
    return binding != widgets_.end() && binding->second.generation == generation
                   && binding->second.visibility.Get() == handler
               ? generation
               : 0;
  }

  std::optional<ZoomLevels> VisibilityLevelFor(NavigationZoomEventHandler* handler, ZoomLevels native)
  {
    if (!camera_.zoom_known || Config::Get().graphics.zoom_label_base.detail == FleetLabelDetail::kNative)
      return std::nullopt;
    const auto found = visibility_.find(handler);
    if (found == visibility_.end() || found->second.size() != 1)
      return std::nullopt;
    const auto [key, generation] = found->second.front();
    const auto binding           = widgets_.find(key);
    if (binding == widgets_.end() || binding->second.generation != generation
        || binding->second.visibility.Get() != handler || !binding->second.base)
      return std::nullopt;
    const auto effective = LevelFor(key, native);
    const auto current   = widgets_.find(key);
    if (!effective || current == widgets_.end() || current->second.generation != generation
        || VisibilityRevision(handler) != generation)
      return std::nullopt;
    current->second.visibility_overridden = true;
    return effective;
  }

  static bool Enabled()
  {
    const auto& config = Config::Get();
    return config.graphics.zoom_label_player.detail != FleetLabelDetail::kNative
           || config.graphics.zoom_label_non_player.detail != FleetLabelDetail::kNative
           || config.graphics.zoom_label_base.detail != FleetLabelDetail::kNative;
  }

  // The level `lod` should show, including Native restoration, or nothing without a qualified tracked fleet.
  std::optional<ZoomLevels> LevelFor(NavigationLOD* lod, ZoomLevels native) const
  {
    const auto profile = Profile(lod);
    if (!profile)
      return std::nullopt;
    if (IsBase(lod) && !camera_.zoom_known)
      return native;
    // Expanded keeps Near where the game would show it, and lifts everything else to Middle.
    const auto expanded = native == ZoomLevels::Near ? ZoomLevels::Near : ZoomLevels::Middle;
    switch (profile->detail) {
      case FleetLabelDetail::kExpanded:
        return expanded;
      case FleetLabelDetail::kCompact:
        return ZoomLevels::Far;
      case FleetLabelDetail::kThreshold:
        if (!camera_.zoom_known || !camera_.normalized_known)
          return native;
        return ExpandedAt(profile->zoom_threshold) ? expanded : ZoomLevels::Far;
      default:
        return native;
    }
  }

  template <typename WidgetType> void Track(WidgetType* widget)
  {
    constexpr bool base = std::is_same_v<WidgetType, NavigationStarbaseWidget>;
    if (!(base ? bases_installed_ : fleets_installed_))
      return;
    // Fresh pooled widgets are enabled before BindDataContext assigns their context; forcing an LOD before then
    // fires OnLODChanged callbacks that dereference the unbound fleet data.
    auto* context = widget->Context();
    auto* lod     = context ? widget->lod() : nullptr;
    if (!lod)
      return;
    auto widget_ref = il2cpp::GcHandle::Weak(widget);
    auto lod_ref    = il2cpp::GcHandle::Weak(lod);
    if (!widget_ref || !lod_ref)
      return;
    Erase(lod);
    const auto generation = ++generation_;
    widgets_.insert_or_assign(lod, Binding{std::move(widget_ref), std::move(lod_ref), generation, base, {}});
    if constexpr (base) {
      auto             root     = il2cpp::GcHandle::Strong(widget);
      const auto       observed = widget->TryZoomVisibilityHandler();
      il2cpp::GcHandle handler =
          observed && observed.value ? il2cpp::GcHandle::Strong(observed.value) : il2cpp::GcHandle{};
      const bool qualified =
          root && handler && unity::Alive(widget) && widget->Context() == context && widget->lod() == lod;
      auto found = widgets_.find(lod);
      if (qualified && found != widgets_.end() && found->second.generation == generation) {
        auto weak = il2cpp::GcHandle::Weak(handler.Get());
        if (weak) {
          // A shared gate cannot safely inherit one of several independently controlled labels.
          found->second.visibility     = std::move(weak);
          found->second.visibility_key = handler.Get<NavigationZoomEventHandler>();
          auto& owners                 = visibility_[handler.Get<NavigationZoomEventHandler>()];
          owners.emplace_back(lod, generation);
          if (owners.size() > 1) {
            bool restore = false;
            for (const auto& [key, revision] : owners) {
              const auto owner = widgets_.find(key);
              if (owner != widgets_.end() && owner->second.generation == revision)
                restore = std::exchange(owner->second.visibility_overridden, false) || restore;
            }
            if (restore && camera_.zoom_known) {
              handler.Get<NavigationZoomEventHandler>()->OnZoomChanged(camera_.zoom_level);
              const auto current = visibility_.find(handler.Get<NavigationZoomEventHandler>());
              if (current != visibility_.end() && current->second.size() > 1
                  && unity::Alive(handler.Get<il2cpp::Object>()))
                handler.Get<NavigationZoomEventHandler>()->Evaluate();
            }
          }
        }
      }
    }
    if (camera_.zoom_known && Enabled() && Revision(lod) == generation) {
      lod->UpdateLOD(camera_.zoom_level);
      ApplyVisibility(lod, generation, camera_.zoom_level);
    }
  }

  template <typename WidgetType> void Untrack(WidgetType* widget)
  {
    constexpr bool base = std::is_same_v<WidgetType, NavigationStarbaseWidget>;
    if (!(base ? bases_installed_ : fleets_installed_))
      return;
    auto* lod = widget->lod();
    if (!lod)
      return;
    const auto found = widgets_.find(lod);
    if (found == widgets_.end() || found->second.widget.Get() != widget || found->second.base != base)
      return;
    auto             widget_root        = il2cpp::GcHandle::Strong(widget);
    auto             lod_root           = il2cpp::GcHandle::Strong(lod);
    il2cpp::GcHandle visibility_root    = il2cpp::GcHandle::Strong(found->second.visibility.Get());
    const bool       restore_visibility = found->second.visibility_overridden;
    const auto       camera_revision    = camera_revision_;
    const auto       level              = camera_.zoom_level;
    const bool       zoom_known         = camera_.zoom_known;
    Erase(lod);
    // Give a reused widget back to the game at the game's own level.
    if (widget_root && lod_root && unity::Alive(widget) && unity::Alive(lod) && widget->Context()
        && widget->lod() == lod && !widgets_.contains(lod) && zoom_known && camera_revision_ == camera_revision) {
      lod->SetTargetLevel(level);
      lod->UpdateLOD(level);
      // UpdateLOD can bind a replacement owner; do not overwrite its bookkeeping.
      if (unity::Alive(lod) && !widgets_.contains(lod) && camera_revision_ == camera_revision)
        lod->SetTargetLevel(level);
    }
    if (restore_visibility && visibility_root && zoom_known && camera_revision_ == camera_revision
        && !visibility_.contains(visibility_root.Get<NavigationZoomEventHandler>())
        && unity::Alive(visibility_root.Get<il2cpp::Object>())) {
      auto* handler = visibility_root.Get<NavigationZoomEventHandler>();
      handler->OnZoomChanged(level);
      if (!visibility_.contains(handler) && unity::Alive(handler))
        handler->Evaluate();
    }
  }

  // The camera `zoom` is about to show `depth`. Labels only apply in a solar system, and a new camera starts over.
  void OnDepth(NavigationZoom* zoom, NodeDepth depth)
  {
    if (!installed_)
      return;
    if (depth == NodeDepth::SolarSystem) {
      (void)Follow(zoom);
    } else if (camera_.zoom == zoom) {
      camera_ = {};
      ++camera_revision_;
    }
  }

  // After the camera's own update: picks up the zoom level and re-applies labels when they change.
  void OnZoomed(NavigationZoom* zoom)
  {
    if (!installed_)
      return;
    auto       owner = il2cpp::GcHandle::Strong(zoom);
    const auto depth = owner ? zoom->TryDepth() : il2cpp::CallResult<NodeDepth>{};
    if (!depth || depth.value != NodeDepth::SolarSystem) {
      if (camera_.zoom == zoom) {
        camera_ = {};
        ++camera_revision_;
      }
      return;
    }
    if (refreshing_) {
      // An unchanged nested update needs no second batch. A different camera retires the current one.
      if (camera_.zoom != zoom) {
        (void)Follow(zoom);
        refresh_pending_ = true;
      }
      return;
    }
    if (!Follow(zoom))
      return;
    // Copy profiles, including values changed by a TOML reload. No Config reference survives native callbacks.
    const auto profiles        = CurrentProfiles();
    const bool changed         = !profiles_known_ || !SameProfiles(profiles_, profiles);
    const bool refresh         = std::exchange(refresh_pending_, false) || changed;
    profiles_                  = profiles;
    profiles_known_            = true;
    const bool was_known       = camera_.zoom_known;
    const auto camera_revision = camera_revision_;
    const auto level           = zoom->TryZoomLevel();
    if (!level || level.value < ZoomLevels::Near || level.value > ZoomLevels::Far) {
      camera_.zoom_known = camera_.normalized_known = camera_.threshold_known = false;
      return;
    }
    camera_.zoom_level = level.value;
    camera_.zoom_known = true;
    if (refresh)
      camera_.threshold_known = false;
    if (UsesThreshold()) {
      const auto normalized = zoom->TryNormalizedZoom();
      if (camera_revision_ != camera_revision || !SameProfiles(profiles, CurrentProfiles())) {
        refresh_pending_ = true;
        return;
      }
      camera_.normalized_known =
          normalized && std::isfinite(normalized.value) && normalized.value >= 0.f && normalized.value <= 1.f;
      if (!camera_.normalized_known) {
        camera_.threshold_known = false;
        ApplyAll();
        return;
      }
      camera_.normalized_zoom = normalized.value;
      UpdateThreshold();
    } else {
      camera_.threshold_known = false;
      if (!was_known || refresh)
        ApplyAll();
    }
  }

private:
  struct Binding {
    il2cpp::GcHandle            widget, lod;
    uint64_t                    generation{};
    bool                        base = false;
    il2cpp::GcHandle            visibility;
    bool                        visibility_overridden = false;
    NavigationZoomEventHandler* visibility_key        = nullptr; // Identity only, including after weak collection.
  };

  void Erase(NavigationLOD* lod)
  {
    const auto found = widgets_.find(lod);
    if (found == widgets_.end())
      return;
    auto*      handler = found->second.visibility_key;
    const auto entry   = visibility_.find(handler);
    if (entry != visibility_.end()) {
      const auto previous_size = entry->second.size();
      std::erase(entry->second, std::pair{lod, found->second.generation});
      if (entry->second.empty())
        visibility_.erase(entry);
      else if (previous_size > 1 && entry->second.size() == 1)
        refresh_pending_ = true;
    }
    widgets_.erase(found);
  }

  void ApplyVisibility(NavigationLOD* lod, uint64_t generation, ZoomLevels native)
  {
    const auto found = widgets_.find(lod);
    if (found == widgets_.end() || found->second.generation != generation || !found->second.base)
      return;
    const bool is_native = Config::Get().graphics.zoom_label_base.detail == FleetLabelDetail::kNative;
    const bool restore   = is_native && std::exchange(found->second.visibility_overridden, false);
    if (is_native && !restore)
      return;
    auto handler = il2cpp::GcHandle::Strong(found->second.visibility.Get());
    if (handler && unity::Alive(handler.Get<il2cpp::Object>())) {
      handler.Get<NavigationZoomEventHandler>()->OnZoomChanged(native);
      if (restore && Revision(lod) == generation && unity::Alive(handler.Get<il2cpp::Object>())
          && Config::Get().graphics.zoom_label_base.detail == FleetLabelDetail::kNative)
        handler.Get<NavigationZoomEventHandler>()->Evaluate();
    }
  }

  static NavigationLOD* BoundLod(il2cpp::Object* widget, bool base)
  {
    return base ? static_cast<NavigationStarbaseWidget*>(widget)->lod()
                : static_cast<NavigationFleetWidget*>(widget)->lod();
  }

  static il2cpp::Object* BoundContext(il2cpp::Object* widget, bool base)
  {
    return base ? static_cast<il2cpp::Object*>(static_cast<NavigationStarbaseWidget*>(widget)->Context())
                : static_cast<il2cpp::Object*>(static_cast<NavigationFleetWidget*>(widget)->Context());
  }

  std::optional<FleetLabelProfile> Profile(NavigationLOD* lod) const
  {
    const auto it = widgets_.find(lod);
    if (it == widgets_.end() || it->second.lod.Get() != lod)
      return std::nullopt;
    const auto generation  = it->second.generation;
    const bool base        = it->second.base;
    auto       widget_root = il2cpp::GcHandle::Strong(it->second.widget.Get());
    auto*      widget      = widget_root.Get<il2cpp::Object>();
    if (!widget || !unity::Alive(widget) || BoundLod(widget, base) != lod)
      return std::nullopt;
    auto       context_root = il2cpp::GcHandle::Strong(BoundContext(widget, base));
    const auto current      = widgets_.find(lod);
    if (current == widgets_.end() || current->second.generation != generation)
      return std::nullopt;
    if (!context_root)
      return std::nullopt;
    if (base)
      return Config::Get().graphics.zoom_label_base;
    const auto kind = context_root.Get<FleetDeployedData>()->TryCall<"get_FleetType", DeployedFleetType>();
    if (!kind || Revision(lod) != generation)
      return std::nullopt;
    const auto& config = Config::Get();
    return kind.value == DeployedFleetType::Player ? config.graphics.zoom_label_player
                                                   : config.graphics.zoom_label_non_player;
  }

  using Profiles = std::array<FleetLabelProfile, 3>;
  static Profiles CurrentProfiles()
  {
    const auto& graphics = Config::Get().graphics;
    return {graphics.zoom_label_player, graphics.zoom_label_non_player, graphics.zoom_label_base};
  }
  static bool SameProfiles(const Profiles& left, const Profiles& right)
  {
    for (size_t i = 0; i < left.size(); ++i)
      if (left[i].detail != right[i].detail
          || !(left[i].zoom_threshold == right[i].zoom_threshold
               || (std::isnan(left[i].zoom_threshold) && std::isnan(right[i].zoom_threshold))))
        return false;
    return true;
  }

  bool UsesThreshold() const
  {
    return std::ranges::any_of(profiles_,
                               [](const auto& profile) { return profile.detail == FleetLabelDetail::kThreshold; });
  }

  // Labels expand when zoomed in at least as far as the threshold. 0 never expands, 1 always does.
  bool ExpandedAt(float threshold) const
  { return threshold > 0.0f && (threshold >= 1.0f || camera_.normalized_zoom <= threshold); }

  bool Follow(NavigationZoom* zoom)
  {
    if (camera_.zoom != zoom || camera_.owner.Get() != zoom) {
      auto weak = il2cpp::GcHandle::Weak(zoom);
      camera_   = {};
      ++camera_revision_;
      if (!weak)
        return false;
      camera_.zoom  = zoom;
      camera_.owner = std::move(weak);
    }
    return true;
  }

  // UpdateLOD goes through the hook, which picks each label's level.
  void ApplyAll()
  {
    if (refreshing_) {
      refresh_pending_ = true;
      return;
    }
    refreshing_ = true;
    struct Done {
      bool& flag;
      ~Done()
      { flag = false; }
    } done{refreshing_};
    const auto camera_revision = camera_revision_;
    const auto level           = camera_.zoom_level;
    // Native updates can release/rebind any widget, invalidating map iterators and old receiver identities.
    std::vector<std::pair<NavigationLOD*, uint64_t>> pending;
    pending.reserve(widgets_.size());
    for (const auto& [key, binding] : widgets_)
      pending.emplace_back(key, binding.generation);
    for (const auto& [key, generation] : pending) {
      // A nested camera transition or preference edit retires this batch. Reapply from the next live update.
      if (camera_revision_ != camera_revision || refresh_pending_ || !SameProfiles(profiles_, CurrentProfiles())) {
        refresh_pending_ = true;
        return;
      }
      auto found = widgets_.find(key);
      if (found == widgets_.end() || found->second.generation != generation)
        continue;
      const bool base        = found->second.base;
      auto       widget_root = il2cpp::GcHandle::Strong(found->second.widget.Get());
      auto       lod_root    = il2cpp::GcHandle::Strong(found->second.lod.Get());
      auto*      widget      = widget_root.Get<il2cpp::Object>();
      auto*      lod         = lod_root.Get<NavigationLOD>();
      if (!widget || !lod || !unity::Alive(widget) || !unity::Alive(lod)) {
        Erase(key);
        continue;
      }
      auto* context = BoundContext(widget, base);
      found         = widgets_.find(key);
      if (context && BoundLod(widget, base) == lod && found != widgets_.end() && found->second.generation == generation
          && camera_revision_ == camera_revision) {
        lod->UpdateLOD(level);
        if (camera_revision_ == camera_revision)
          ApplyVisibility(lod, generation, level);
      }
    }
  }

  // Re-applies labels only when a threshold profile crosses its threshold, not on every zoom step.
  void UpdateThreshold()
  {
    const auto profiles = profiles_;
    const auto expanded = [this](const FleetLabelProfile& profile) {
      return profile.detail == FleetLabelDetail::kThreshold && ExpandedAt(profile.zoom_threshold);
    };
    const bool player     = expanded(profiles[0]);
    const bool non_player = expanded(profiles[1]);
    const bool base       = expanded(profiles[2]);
    if (camera_.threshold_known && player == camera_.player_expanded && non_player == camera_.non_player_expanded
        && base == camera_.base_expanded)
      return;
    camera_.threshold_known     = true;
    camera_.player_expanded     = player;
    camera_.non_player_expanded = non_player;
    camera_.base_expanded       = base;
    ApplyAll();
    spdlog::debug("[FleetLabels] threshold crossed at normalized zoom {:.4f}", camera_.normalized_zoom);
  }

  // What is known of the system camera's zoom. A new camera starts over.
  struct Camera {
    NavigationZoom*  zoom = nullptr;
    il2cpp::GcHandle owner;
    bool             zoom_known       = false;
    ZoomLevels       zoom_level       = ZoomLevels::None;
    float            normalized_zoom  = 0.0f;
    bool             normalized_known = false;
    // Whether player and non-player threshold labels were last expanded, once threshold_known.
    bool threshold_known     = false;
    bool player_expanded     = false;
    bool non_player_expanded = false;
    bool base_expanded       = false;
  };

  bool                                        installed_ = false, fleets_installed_ = false, bases_installed_ = false;
  bool                                        refresh_pending_ = false, refreshing_ = false, profiles_known_ = false;
  uint64_t                                    generation_{}, camera_revision_{};
  Profiles                                    profiles_{};
  std::unordered_map<NavigationLOD*, Binding> widgets_;
  std::unordered_map<NavigationZoomEventHandler*, std::vector<std::pair<NavigationLOD*, uint64_t>>> visibility_;
  Camera                                                                                            camera_;
};
// Weak handles must be released while IL2CPP is alive, never by static destruction after its teardown.
auto& fleet_labels = *new FleetLabels;

// --- Hooks ---

void NavigationZoomEventHandler_OnZoomChanged_Hook(auto original, NavigationZoomEventHandler* _this, ZoomLevels level)
{
  auto keep = il2cpp::GcHandle::Strong(_this);
  if (!keep)
    return original(_this, level);
  const auto revision  = fleet_labels.VisibilityRevision(_this);
  const auto effective = fleet_labels.VisibilityLevelFor(_this, level);
  if (!effective)
    return original(_this, galaxy_controls::VisibilityLevel(_this, level));
  original(_this, *effective);
  // Native OnZoomChanged may skip its visibility evaluation when authored LOD groups compare equal. The zoom
  // gate must still follow a live base preference; Evaluate retains every other native dependency, including culling.
  if (revision && unity::Alive(_this) && fleet_labels.VisibilityRevision(_this) == revision
      && fleet_labels.VisibilityLevelFor(_this, level) == effective)
    _this->Evaluate();
}

void NavigationLOD_UpdateLOD_Hook(auto original, NavigationLOD* _this, ZoomLevels level)
{
  const auto revision  = fleet_labels.Revision(_this);
  const auto effective = fleet_labels.LevelFor(_this, level);
  if (!effective || fleet_labels.Revision(_this) != revision) {
    const auto galaxy = galaxy_controls::LodLevel(_this, level);
    if (galaxy != level)
      _this->SetTargetLevel(galaxy);
    original(_this, galaxy);
    if (galaxy != level && !fleet_labels.Revision(_this) && unity::Alive(_this))
      _this->SetTargetLevel(galaxy);
    return;
  }
  auto owner = il2cpp::GcHandle::Strong(_this);
  if (!owner || fleet_labels.Revision(_this) != revision)
    return original(_this, level);
  // Qualified tracked fleet rows include Native profiles. Reset their target on both sides too, so a previous
  // forced detail cannot survive the last override being disabled. Foreign POI LODs retain the path above.
  _this->SetTargetLevel(*effective);
  original(_this, *effective);
  if (revision && unity::Alive(_this) && fleet_labels.Revision(_this) == revision)
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
  ship_shortcut_badges::Release(_this);
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

void NavigationStarbaseWidget_OnDidBindContext_Hook(auto original, NavigationStarbaseWidget* _this)
{
  original(_this);
  fleet_labels.Track(_this);
}

void NavigationStarbaseWidget_OnAboutToReleaseContext_Hook(auto original, NavigationStarbaseWidget* _this)
{
  fleet_labels.Untrack(_this);
  original(_this);
}

bool InstallBaseLabels()
{
  // The base lifecycle is independent: unsupported base hooks must not disable ship labels.
  // Both label kinds use the single camera observer and NavigationLOD.UpdateLOD owner.
  if (!EnsureNavigationVisibilityUpdates() || !EnsureNavigationZoomUpdates() || !EnsureNavigationLodUpdates())
    return false;
  il2cpp::HookSet hooks("BaseLabels");
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(NavigationStarbaseWidget::Class::Method("OnDidBindContext")),
                  NavigationStarbaseWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(NavigationStarbaseWidget::Class::Method("OnAboutToReleaseContext")),
                  NavigationStarbaseWidget_OnAboutToReleaseContext_Hook);
  if (!hooks.Commit())
    return false;
  fleet_labels.MarkBasesInstalled();
  return true;
}

bool InstallFleetLabels()
{
  // Install independently of the current profiles so they can change live. Unqualified fleet/camera observations
  // retain native detail. Zoom reports come from camera.cc's shared NavigationZoom.Update owner.
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

void* NavigationVisibilityEntry()
{
  // Qualify original instructions once, before the shared owner patches them. Galaxy's incompatible-entry check
  // uses this same identity rather than trying to qualify a detour's replacement jump on macOS.
  static auto* entry =
      il2cpp::CheckedEntry(NavigationZoomEventHandler::Class::Method<void(ZoomLevels)>("OnZoomChanged"));
  return entry;
}

bool EnsureNavigationVisibilityUpdates()
{
  static const bool installed = [] {
    if (!galaxy_controls::VisibilityHookCompatible()) {
      spdlog::warn(
          "[NavigationVisibilityUpdates] incompatible callbacks share a native entry; retaining native visibility");
      return false;
    }
    il2cpp::HookSet hooks("NavigationVisibilityUpdates");
    IL2CPP_STAGE_AT(hooks, NavigationVisibilityEntry(), NavigationZoomEventHandler_OnZoomChanged_Hook);
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

void RefreshFleetLabelControls()
{ fleet_labels.Refresh(); }

const Feature kFleetLabels{"FleetLabels", &Config::Patches::zoomhooks, InstallFleetLabels};
const Feature kBaseLabels{"BaseLabels", &Config::Patches::zoomhooks, InstallBaseLabels};
