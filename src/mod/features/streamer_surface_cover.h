#pragma once

#include <unity/ui.h>

#include <algorithm>
#include <map>
#include <span>
#include <utility>

namespace streamer_mode
{
// Own a render-only group on one qualified presentation object. Restore before releasing a live pooled view.
class SurfaceCover
{
public:
  bool Restore()
  { return Change(nullptr); }

  bool Hide(unity::GameObject* target)
  { return target && Change(target); }

private:
  bool Change(unity::GameObject* target)
  {
    if (std::exchange(changing_, true))
      return false;
    struct Done {
      bool& changing;
      ~Done()
      { changing = false; }
    } done{changing_};

    if (!target)
      return RestoreOwned();
    auto target_root = il2cpp::GcHandle::Strong(target);
    if (!unity::Alive(target_root.Get()))
      return false;
    if (target_.Get() != target && !RestoreOwned())
      return false;

    const auto alive = unity::TryAlive(group_.Get());
    if (!alive)
      return false;
    if (!alive.value) {
      auto weak_target = il2cpp::GcHandle::Weak(target_root.Get());
      if (!weak_target)
        return false;
      auto* group = target->AddComponent<unity::CanvasGroup>();
      if (!group)
        return false;
      group_ = il2cpp::GcHandle::Strong(group);
      if (!group_) {
        unity::Destroy(group);
        return false;
      }
      // Hold the added group until its alpha is neutral. A pooled label can outlive its managed wrapper.
      target_ = std::move(weak_target);
    }
    return group_.Get<unity::CanvasGroup>()->SetAlpha(0.0f);
  }

  bool RestoreOwned()
  {
    auto*      group = group_.Get<unity::CanvasGroup>();
    const auto alive = unity::TryAlive(group);
    if (!alive || (alive.value && !group->SetAlpha(1.0f)))
      return false;
    if (alive.value)
      unity::Destroy(group);
    group_.Reset();
    target_.Reset();
    return true;
  }

  il2cpp::GcHandle target_, group_;
  bool             changing_ = false;
};

// Reconcile exact label objects. Restore failed covers again even after a native view changes labels.
class SurfaceCovers
{
public:
  bool Apply(std::span<unity::GameObject* const> targets = {})
  {
    if (std::exchange(applying_, true))
      return false;
    struct Done {
      bool& applying;
      ~Done()
      { applying = false; }
    } done{applying_};

    bool complete = true;
    for (auto* target : targets)
      if (target && !covers_[target].Hide(target))
        complete = false;
    for (auto cover = covers_.begin(); cover != covers_.end();) {
      if (std::ranges::find(targets, cover->first) != targets.end()) {
        ++cover;
      } else if (cover->second.Restore()) {
        cover = covers_.erase(cover);
      } else {
        complete = false;
        ++cover;
      }
    }
    return complete;
  }

private:
  std::map<unity::GameObject*, SurfaceCover> covers_;
  bool                                       applying_ = false;
};
} // namespace streamer_mode
