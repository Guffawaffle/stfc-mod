#pragma once

#include "features/hyperthermic_decay_state.h"
#include "game/hud.h"
#include "game/hyperthermic_decay.h"
#include "game/ui.h"

#include <il2cpp/il2cpp.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <utility>

namespace hyperthermic_decay_hud
{
using BuffRoots = std::array<il2cpp::GcHandle, hyperthermic_decay::kBuffDefinitions.size()>;

// Own the mount and cloned native widgets. The game owns their art, countdown,
// localization and click tooltip. Context discovery stays on the main thread.
class Hud
{
public:
  bool Prepare()
  {
    const auto now   = Clock::now();
    const bool valid = OwnersValid();
    if ((!valid || now >= next_registry_check_) && !RefreshOwners(now, valid))
      return false;
    if (root_ && (!unity::Alive(root_.Get()) || !unity::Alive(rect_.Get())))
      ClearDots();
    return true;
  }

  bool Show(const hyperthermic_decay::Rows& rows, const BuffRoots& sources, bool anomaly_timer)
  {
    std::array<bool, hyperthermic_decay::kBuffDefinitions.size()> wanted{};
    size_t                                                        count = 0;
    for (size_t index = 0; index < rows.entries.size(); ++index) {
      wanted[index] = rows.entries[index] && hyperthermic_decay::IsActive(rows.entries[index]->observation);
      count += wanted[index];
    }
    if (!count) {
      Hide();
      return true;
    }
    if (!CreateRoot() || !AcquireTemplate()) {
      Hide();
      return false;
    }
    bool layout_changed = !layout_valid_ || wanted != shown_ || anomaly_timer != anomaly_timer_;
    for (size_t index = 0; index < dots_.size(); ++index) {
      auto& dot = dots_[index];
      if (!wanted[index]) {
        dot.Release();
        continue;
      }
      auto* context = sources[index].Get<hyperthermic_decay_binding::BuffData>();
      if (!context) {
        Hide();
        return false;
      }
      if (!dot.Valid()) {
        // Instantiate under an inactive owned parent. The native Update must
        // never run on a clone before its data context has been bound.
        Deactivate();
        dot.Destroy();
        auto* copy = unity::Instantiate(prefab_.Get<unity::GameObject>(), rect_.Get<unity::RectTransform>());
        dot.object = il2cpp::GcHandle::Strong(copy);
        if (unity::Alive(copy)) {
          copy->SetActive(false);
          dot.widget   = il2cpp::GcHandle::Strong(copy->GetComponent<hyperthermic_decay_binding::AutoBuffWidget>());
          auto* widget = dot.widget.Get<hyperthermic_decay_binding::AutoBuffWidget>();
          dot.rect     = il2cpp::GcHandle::Strong(widget ? widget->Rect() : nullptr);
        }
        if (!dot.Valid()) {
          dot.Destroy();
          Hide();
          return false;
        }
        layout_changed = true;
      }
      const auto& observation = rows.entries[index]->observation;
      if (dot.context.Get() != context || !dot.observation
          || !hyperthermic_decay::SameNativeObservation(*dot.observation, observation)) {
        Deactivate();
        // Native OnDidBindContext caches expiry/duration. Rebind on a refresh,
        // even if the service updated the same BuffData object in place.
        if (!dot.Bind(context, observation)) {
          Hide();
          return false;
        }
      }
    }
    if (layout_changed) {
      constexpr size_t columns = 8;
      constexpr float  gap     = 4;
      const auto       cols    = std::min(count, columns);
      const auto       lines   = (count + columns - 1) / columns;
      rect_.Get<unity::RectTransform>()->Place(
          {1, 0}, {1, 0}, {1, 1},
          {static_cast<float>(cols) * dot_width_ + static_cast<float>(cols - 1) * gap,
           static_cast<float>(lines) * dot_width_ + static_cast<float>(lines - 1) * gap},
          {-4, anomaly_timer ? -84.0f : -40.0f});
      size_t position = 0;
      for (size_t index = 0; index < dots_.size(); ++index) {
        if (!wanted[index])
          continue;
        dots_[index].rect.Get<unity::RectTransform>()->Place(
            {0, 1}, {0, 1}, {0, 1}, {dot_width_, dot_width_},
            {static_cast<float>(position % columns) * (dot_width_ + gap),
             -static_cast<float>(position / columns) * (dot_width_ + gap)});
        ++position;
      }
      shown_         = wanted;
      anomaly_timer_ = anomaly_timer;
      layout_valid_  = true;
    }
    for (size_t index = 0; index < dots_.size(); ++index)
      if (wanted[index] && !dots_[index].visible) {
        dots_[index].object.Get<unity::GameObject>()->SetActive(true);
        dots_[index].visible = true;
      }
    if (!visible_) {
      root_.Get<unity::GameObject>()->SetActive(true);
      visible_ = true;
    }
    return true;
  }

  void Hide()
  {
    Deactivate();
    for (auto& dot : dots_)
      dot.Release();
  }

  void Clear()
  {
    ClearDots();
    prefab_.Reset();
    parent_object_.Reset();
    parent_.Reset();
    chest_.Reset();
    promotion_.Reset();
    drawer_.Reset();
    navigation_.Reset();
    next_registry_check_ = {};
  }

private:
  using Clock = std::chrono::steady_clock;
  struct Dot {
    il2cpp::GcHandle                               object, widget, rect, context;
    std::optional<hyperthermic_decay::Observation> observation;
    bool                                           visible = false;

    bool Valid() const
    { return unity::Alive(object.Get()) && unity::Alive(widget.Get()) && unity::Alive(rect.Get()); }

    bool Bind(hyperthermic_decay_binding::BuffData* data, const hyperthermic_decay::Observation& observed)
    {
      // Keep both old and replacement contexts rooted across the native bind.
      auto  next = il2cpp::GcHandle::Strong(data);
      auto* view = widget.Get<hyperthermic_decay_binding::AutoBuffWidget>();
      if (!next)
        return false;
      auto old = std::move(context);
      context  = std::move(next);
      if (!view->TryBind(data)) {
        Release();
        return false;
      }
      observation = observed;
      return true;
    }

    void Release()
    {
      if (visible && unity::Alive(object.Get()))
        object.Get<unity::GameObject>()->SetActive(false);
      visible = false;
      if (context && unity::Alive(widget.Get())
          && !widget.Get<hyperthermic_decay_binding::AutoBuffWidget>()->TryRelease()) {
        // A failed native unbind is not a reusable pool entry.
        if (unity::Alive(object.Get()))
          unity::Destroy(object.Get());
        rect.Reset();
        widget.Reset();
        object.Reset();
      }
      context.Reset();
      observation.reset();
    }

    void Destroy()
    {
      Release();
      if (unity::Alive(object.Get()))
        unity::Destroy(object.Get());
      rect.Reset();
      widget.Reset();
      object.Reset();
    }
  };

  void Deactivate()
  {
    if (visible_ && unity::Alive(root_.Get()))
      root_.Get<unity::GameObject>()->SetActive(false);
    visible_ = false;
  }

  void ClearDots()
  {
    Deactivate();
    for (auto& dot : dots_)
      dot.Destroy();
    if (unity::Alive(root_.Get()))
      unity::Destroy(root_.Get());
    rect_.Reset();
    root_.Reset();
    shown_        = {};
    layout_valid_ = false;
  }

  bool OwnersValid() const
  {
    auto* nav       = navigation_.Get<HudNavigationViewController>();
    auto* drawer    = drawer_.Get<HudAllianceAndNewsViewController>();
    auto* promotion = promotion_.Get<unity::Component>();
    auto* chest     = chest_.Get<unity::Transform>();
    auto* parent    = parent_.Get<unity::Transform>();
    auto* object    = parent_object_.Get<unity::GameObject>();
    return unity::Alive(nav) && nav->isActiveAndEnabled() && unity::Alive(drawer) && unity::Alive(promotion)
           && unity::Alive(chest) && unity::Alive(parent) && unity::Alive(object) && object->activeInHierarchy()
           && drawer->chestPromotion() == promotion && promotion->transform() == chest && chest->parent() == parent;
  }

  bool RefreshOwners(Clock::time_point now, bool valid)
  {
    auto navigation = il2cpp::GcHandle::Strong(
        ScreenManager::RegisteredController<HudNavigationViewController, NavigationViewData>());
    auto drawer =
        il2cpp::GcHandle::Strong(ScreenManager::RegisteredController<HudAllianceAndNewsViewController,
                                                                     HudAllianceAndNewsViewControllerContext>());
    auto* nav = navigation.Get<HudNavigationViewController>();
    auto* hud = drawer.Get<HudAllianceAndNewsViewController>();
    if (!unity::Alive(nav) || !nav->isActiveAndEnabled() || !unity::Alive(hud)) {
      Clear();
      return false;
    }
    next_registry_check_ = now + std::chrono::seconds(5);
    if (valid && navigation_.Get() == nav && drawer_.Get() == hud)
      return true;
    auto  promotion     = il2cpp::GcHandle::Strong(hud->chestPromotion());
    auto* button        = promotion.Get<unity::Component>();
    auto  chest         = il2cpp::GcHandle::Strong(unity::Alive(button) ? button->transform() : nullptr);
    auto* anchor        = chest.Get<unity::Transform>();
    auto  parent        = il2cpp::GcHandle::Strong(unity::Alive(anchor) ? anchor->parent() : nullptr);
    auto* transform     = parent.Get<unity::Transform>();
    auto  parent_object = il2cpp::GcHandle::Strong(unity::Alive(transform) ? transform->gameObject() : nullptr);
    if (!unity::Alive(parent_object.Get()) || !parent_object.Get<unity::GameObject>()->activeInHierarchy()) {
      Clear();
      return false;
    }
    Clear();
    navigation_          = std::move(navigation);
    drawer_              = std::move(drawer);
    promotion_           = std::move(promotion);
    chest_               = std::move(chest);
    parent_              = std::move(parent);
    parent_object_       = std::move(parent_object);
    next_registry_check_ = now + std::chrono::seconds(5);
    return true;
  }

  bool CreateRoot()
  {
    if (root_)
      return true;
    auto* rect = unity::CreateUiObject("CommunityMod_HyperthermicDecay", parent_.Get<unity::Transform>(), root_);
    rect_      = il2cpp::GcHandle::Strong(rect);
    if (!rect_) {
      ClearDots();
      return false;
    }
    return true;
  }

  bool AcquireTemplate()
  {
    if (unity::Alive(prefab_.Get()))
      return true;
    const auto source = hyperthermic_decay_binding::TryAutoBuffTemplate();
    if (!source || !unity::Alive(source.value.object))
      return false;
    prefab_       = il2cpp::GcHandle::Strong(source.value.object);
    dot_width_    = std::isfinite(source.value.width) && source.value.width > 0 ? source.value.width : 48;
    layout_valid_ = false;
    return static_cast<bool>(prefab_);
  }

  il2cpp::GcHandle navigation_, drawer_, promotion_, chest_, parent_, parent_object_, prefab_, root_, rect_;
  std::array<Dot, hyperthermic_decay::kBuffDefinitions.size()>  dots_{};
  std::array<bool, hyperthermic_decay::kBuffDefinitions.size()> shown_{};
  Clock::time_point                                             next_registry_check_{};
  float                                                         dot_width_ = 48;
  bool layout_valid_ = false, anomaly_timer_ = false, visible_ = false;
};
} // namespace hyperthermic_decay_hud
