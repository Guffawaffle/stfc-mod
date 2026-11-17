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
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace hyperthermic_decay_hud
{
using BuffRoots = std::array<il2cpp::GcHandle, hyperthermic_decay::kBuffDefinitions.size()>;

// Own the mount, cloned native widgets and change-only labels. The game owns
// the countdown rings and click tooltip. All work stays on the main thread.
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
    auto* font           = AcquireFont();
    bool  layout_changed = !layout_valid_ || wanted != shown_ || anomaly_timer != anomaly_timer_;
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
      bool        rebound     = false;
      if (dot.context.Get() != context || !dot.observation
          || !hyperthermic_decay::SameNativeObservation(*dot.observation, observation)) {
        Deactivate();
        // Native OnDidBindContext caches expiry/duration. Rebind on a refresh,
        // even if the service updated the same BuffData object in place.
        if (!dot.Bind(context, observation)) {
          Hide();
          return false;
        }
        rebound = true;
      }
      if (font && !dot.Decorate(font, *rows.entries[index], dot_width_, rebound)) {
        Hide();
        return false;
      }
    }
    if (layout_changed) {
      constexpr size_t columns = 8;
      constexpr float  gap     = 4;
      const float      height  = dot_width_ + 22;
      const auto       cols    = std::min(count, columns);
      const auto       lines   = (count + columns - 1) / columns;
      rect_.Get<unity::RectTransform>()->Place(
          {1, 0}, {1, 0}, {1, 1},
          {static_cast<float>(cols) * dot_width_ + static_cast<float>(cols - 1) * gap,
           static_cast<float>(lines) * height + static_cast<float>(lines - 1) * gap},
          {-4, anomaly_timer ? -84.0f : -40.0f});
      size_t position = 0;
      for (size_t index = 0; index < dots_.size(); ++index) {
        if (!wanted[index])
          continue;
        dots_[index].rect.Get<unity::RectTransform>()->Place(
            {0, 1}, {0, 1}, {0, 1}, {dot_width_, dot_width_},
            {static_cast<float>(position % columns) * (dot_width_ + gap),
             -static_cast<float>(position / columns) * (height + gap)});
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
    font_.Reset();
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
    il2cpp::GcHandle                               level_object, level_label, percent_object, percent_label, font;
    std::optional<hyperthermic_decay::Observation> observation;
    std::optional<double>                          shown_value;
    int32_t                                        shown_level = -1;
    bool                                           decorated   = false;
    bool                                           visible     = false;

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

    bool LabelsValid() const
    {
      return unity::Alive(level_object.Get()) && unity::Alive(level_label.Get()) && unity::Alive(percent_object.Get())
             && unity::Alive(percent_label.Get());
    }

    bool CreateLabels(il2cpp::Object* native_font, float width)
    {
      DestroyLabels();
      font              = il2cpp::GcHandle::Strong(native_font);
      const auto create = [&](const char* name, il2cpp::GcHandle& child, il2cpp::GcHandle& label, bool center) {
        auto* transform = unity::CreateUiObject(name, rect.Get<unity::RectTransform>(), child);
        auto* owner     = child.Get<unity::GameObject>();
        label = il2cpp::GcHandle::Strong(transform && owner ? owner->AddComponent<unity::TextMeshProUGUI>() : nullptr);
        auto* text   = label.Get<unity::TextMeshProUGUI>();
        auto* layout = text ? owner->AddComponent<unity::LayoutElement>() : nullptr;
        if (!text || !layout || !layout->ExcludeFromLayout() || !text->TryCall<"set_raycastTarget", void(bool)>(false))
          return false;
        if (center)
          transform->Place({0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {width - 4, width - 4}, {0, 0});
        else
          transform->Place({0.5f, 0}, {0.5f, 0}, {0.5f, 1}, {width + 4, 20}, {0, -2});
        text->SetFont(native_font);
        text->SetFontSize(center ? std::clamp(width * 0.5f, 18.0f, 28.0f) : 16.0f);
        text->SetAlignment(unity::TMP_Text::Alignment::Center);
        text->SetColor({0.92f, 0.98f, 0.94f, 1});
        text->SetEnableWordWrapping(false);
        text->SetEnableAutoSizing(false);
        text->SetRichText(false);
        text->SetOverflowMode(unity::TMP_Text::Overflow::Ellipsis);
        return true;
      };
      if (!font || !create("HostileLevel", level_object, level_label, true)
          || !create("AmplifierPercent", percent_object, percent_label, false)) {
        DestroyLabels();
        return false;
      }
      return true;
    }

    bool Decorate(il2cpp::Object* native_font, const hyperthermic_decay::Row& row, float width, bool rebound)
    {
      if ((!LabelsValid() || font.Get() != native_font) && !CreateLabels(native_font, width))
        return false;
      auto* level   = level_label.Get<unity::TextMeshProUGUI>();
      auto* percent = percent_label.Get<unity::TextMeshProUGUI>();
      if (shown_level != row.hostileLevel) {
        level->SetText(std::to_string(row.hostileLevel));
        shown_level = row.hostileLevel;
      }
      if (shown_value != row.observation.value) {
        percent->SetText(std::format("{:.3g}%", row.observation.value * 100));
        percent->SetColor(row.observation.value > 0 ? unity::Color{0.35f, 0.95f, 0.55f, 1}
                                                    : unity::Color{0.72f, 0.78f, 0.82f, 1});
        shown_value = row.observation.value;
      }
      // Native binding can refresh the icon; apply our copy's decoration only
      // afterwards, without altering its timer image or click target.
      if ((!decorated || rebound) && !widget.Get<hyperthermic_decay_binding::AutoBuffWidget>()->TryMuteCenterIcon())
        return false;
      if (!decorated) {
        level_object.Get<unity::GameObject>()->SetActive(true);
        percent_object.Get<unity::GameObject>()->SetActive(true);
        decorated = true;
      }
      return true;
    }

    void DestroyLabels()
    {
      if (unity::Alive(level_object.Get()))
        unity::Destroy(level_object.Get());
      if (unity::Alive(percent_object.Get()))
        unity::Destroy(percent_object.Get());
      level_label.Reset();
      percent_label.Reset();
      level_object.Reset();
      percent_object.Reset();
      font.Reset();
      shown_value.reset();
      shown_level = -1;
      decorated   = false;
    }

    void Release()
    {
      if (visible && unity::Alive(object.Get()))
        object.Get<unity::GameObject>()->SetActive(false);
      visible = false;
      if (context && unity::Alive(widget.Get())
          && !widget.Get<hyperthermic_decay_binding::AutoBuffWidget>()->TryRelease()) {
        // A failed native unbind is not a reusable pool entry.
        DestroyLabels();
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
      DestroyLabels();
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

  il2cpp::Object* AcquireFont()
  {
    if (unity::Alive(font_.Get()))
      return font_.Get<il2cpp::Object>();
    auto*      navigation  = navigation_.Get<HudNavigationViewController>();
    auto       helper      = il2cpp::GcHandle::Strong(navigation->factionInformationHelper());
    auto*      information = helper.Get<FactionInformation>();
    auto       names       = il2cpp::GcHandle::Strong(information ? information->systemName() : nullptr);
    auto*      labels      = names.Get<il2cpp::List<unity::Component*>>();
    const auto count       = labels ? labels->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
    if (!count || count.value < 0)
      return nullptr;
    // Reuse a HUD font through its known owner. No scene-wide text discovery.
    for (int32_t index = 0; index < count.value; ++index) {
      const auto item      = labels->TryCall<"get_Item", unity::Component*>(index);
      auto       name      = il2cpp::GcHandle::Strong(item ? item.value : nullptr);
      auto*      component = name.Get<unity::Component>();
      auto       object    = il2cpp::GcHandle::Strong(unity::Alive(component) ? component->gameObject() : nullptr);
      auto*      owner     = object.Get<unity::GameObject>();
      auto       label     = il2cpp::GcHandle::Strong(owner ? owner->GetComponent<unity::TextMeshProUGUI>() : nullptr);
      auto*      text      = label.Get<unity::TextMeshProUGUI>();
      auto       font      = il2cpp::GcHandle::Strong(unity::Alive(text) ? text->font() : nullptr);
      if (unity::Alive(font.Get())) {
        font_ = std::move(font);
        return font_.Get<il2cpp::Object>();
      }
    }
    return nullptr;
  }

  il2cpp::GcHandle navigation_, drawer_, promotion_, chest_, parent_, parent_object_, prefab_, root_, rect_, font_;
  std::array<Dot, hyperthermic_decay::kBuffDefinitions.size()>  dots_{};
  std::array<bool, hyperthermic_decay::kBuffDefinitions.size()> shown_{};
  Clock::time_point                                             next_registry_check_{};
  float                                                         dot_width_ = 48;
  bool layout_valid_ = false, anomaly_timer_ = false, visible_ = false;
};
} // namespace hyperthermic_decay_hud
