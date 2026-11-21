#pragma once

#include <unity/ui.h>

namespace streamer_mode
{
// Own a render-only group on one qualified presentation object. Restore before releasing a live pooled view.
class SurfaceCover
{
public:
  enum class GroupRetention { kWeak, kStrong };

  SurfaceCover() = default;
  explicit SurfaceCover(GroupRetention retention)
      : retention_(retention)
  {
  }

  bool Restore()
  {
    if (auto* existing = group_.Get()) {
      auto       root  = il2cpp::GcHandle::Strong(existing);
      const auto alive = root ? unity::TryAlive(root.Get()) : il2cpp::CallResult<bool>{};
      if (!alive)
        return false;
      if (alive.value) {
        if (!root.Get<unity::CanvasGroup>()->TryCall<"set_alpha", void(float)>(1.0f))
          return false;
        unity::Destroy(root.Get());
      }
    }
    group_.Reset();
    target_.Reset();
    return true;
  }

  bool Set(unity::GameObject* target, bool hidden)
  {
    if (!hidden)
      return Restore();
    if (!target || !unity::Alive(target))
      return false;
    if (target_.Get() != target && !Restore())
      return false;

    auto*      existing = group_.Get();
    auto       group    = il2cpp::GcHandle::Strong(existing);
    const auto alive    = group ? unity::TryAlive(group.Get()) : il2cpp::CallResult<bool>{};
    if (existing && !alive)
      return false;
    if (!existing || !alive.value) {
      auto target_root = il2cpp::GcHandle::Strong(target);
      if (!target_root)
        return false;
      group = il2cpp::GcHandle::Strong(target->AddComponent<unity::CanvasGroup>());
      if (!unity::Alive(group.Get()))
        return false;
      target_ = il2cpp::GcHandle::Weak(target_root.Get());
      // A pooled target can outlive its added component's managed wrapper. Keep that wrapper until restoration.
      group_ = retention_ == GroupRetention::kStrong ? il2cpp::GcHandle::Strong(group.Get())
                                                     : il2cpp::GcHandle::Weak(group.Get());
      if (!target_ || !group_) {
        unity::Destroy(group.Get());
        return false;
      }
    }
    return group.Get<unity::CanvasGroup>()->TryCall<"set_alpha", void(float)>(0.0f).succeeded;
  }

private:
  il2cpp::GcHandle target_, group_;
  GroupRetention   retention_ = GroupRetention::kWeak;
};
} // namespace streamer_mode
