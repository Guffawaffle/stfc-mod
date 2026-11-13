#pragma once

#include "game/hud.h"
#include "game/ui.h"

#include <il2cpp/il2cpp.h>
#include <unity/ui.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace hyperthermic_decay_hud
{
// Main-thread custody of the passive panel. The caller confirms System view
// and calls Prepare at most once a second before reading buffs or calling Show.
class Hud
{
public:
  bool Prepare()
  {
    const auto now          = Clock::now();
    const bool owners_valid = OwnersValid();
    if ((!owners_valid || now >= next_registry_check_) && !RefreshOwners(now, owners_valid))
      return false;
    if (root_ && !PanelValid())
      ClearPanel();
    // Resolve custody without creating visible UI for an empty buff set.
    prepared_ = true;
    return true;
  }

  // Prepare already checked the owners during this main-thread observation.
  // Countdown ticks change column text only; geometry follows actual layout changes.
  void Show(std::string_view heading, std::string_view left, std::string_view right, size_t left_count,
            size_t right_count, bool anomaly_timer)
  {
    if (!prepared_ || (!root_ && !Create(navigation_.Get<HudNavigationViewController>())))
      return;
    if (right_count && !EnsureRightColumn()) {
      Hide();
      return;
    }
    SetText(heading_, heading, shown_heading_);
    SetText(left_, left, shown_left_);
    if (right_)
      SetText(right_, right, shown_right_);
    const auto heading_lines = 1 + static_cast<size_t>(std::count(heading.begin(), heading.end(), '\n'));
    if (!layout_valid_ || heading_lines != heading_lines_ || left_count != left_count_ || right_count != right_count_
        || anomaly_timer != anomaly_timer_) {
      Place(heading_lines, left_count, right_count, anomaly_timer);
      heading_lines_ = heading_lines;
      left_count_    = left_count;
      right_count_   = right_count;
      anomaly_timer_ = anomaly_timer;
      layout_valid_  = true;
    }
    if (!visible_) {
      root_.Get<unity::GameObject>()->SetActive(true);
      visible_ = true;
    }
  }

  void Hide()
  {
    if (visible_ && unity::Alive(root_.Get()))
      root_.Get<unity::GameObject>()->SetActive(false);
    visible_ = false;
  }

  void Clear()
  {
    ClearPanel();
    parent_object_.Reset();
    parent_.Reset();
    font_.Reset();
    font_donor_.Reset();
    faction_.Reset();
    chest_.Reset();
    promotion_.Reset();
    drawer_.Reset();
    navigation_.Reset();
    next_registry_check_ = {};
  }

private:
  using Clock = std::chrono::steady_clock;

  struct FontSource {
    il2cpp::GcHandle faction, donor, font;
  };

  bool OwnersValid() const
  {
    auto* nav           = navigation_.Get<HudNavigationViewController>();
    auto* drawer        = drawer_.Get<HudAllianceAndNewsViewController>();
    auto* promotion     = promotion_.Get<unity::Component>();
    auto* chest         = chest_.Get<unity::Transform>();
    auto* parent        = parent_.Get<unity::Transform>();
    auto* parent_object = parent_object_.Get<unity::GameObject>();
    auto* donor         = font_donor_.Get<unity::TextMeshProUGUI>();
    return unity::Alive(nav) && nav->isActiveAndEnabled() && unity::Alive(drawer) && unity::Alive(promotion)
           && unity::Alive(chest) && unity::Alive(parent) && unity::Alive(parent_object)
           && parent_object->activeInHierarchy() && unity::Alive(donor) && donor->isActiveAndEnabled()
           && unity::Alive(font_.Get()) && nav->factionInformationHelper() == faction_.Get()
           && drawer->chestPromotion() == promotion && promotion->transform() == chest && chest->parent() == parent
           && donor->font() == font_.Get();
  }

  bool RefreshOwners(Clock::time_point now, bool owners_valid)
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
    // A periodic identity check is cheap. Keep the existing font donor and UI
    // when the registered owners and their cached children are unchanged.
    if (owners_valid && navigation_.Get() == nav && drawer_.Get() == hud)
      return true;

    auto  promotion        = il2cpp::GcHandle::Strong(hud->chestPromotion());
    auto* button           = promotion.Get<unity::Component>();
    auto  chest            = il2cpp::GcHandle::Strong(unity::Alive(button) ? button->transform() : nullptr);
    auto* anchor           = chest.Get<unity::Transform>();
    auto  parent           = il2cpp::GcHandle::Strong(unity::Alive(anchor) ? anchor->parent() : nullptr);
    auto* parent_transform = parent.Get<unity::Transform>();
    auto  parent_object =
        il2cpp::GcHandle::Strong(unity::Alive(parent_transform) ? parent_transform->gameObject() : nullptr);
    auto* parent_owner = parent_object.Get<unity::GameObject>();
    auto  font         = FindFont(nav);
    if (!unity::Alive(parent_owner) || !parent_owner->activeInHierarchy() || !font.faction || !font.donor
        || !font.font) {
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
    faction_             = std::move(font.faction);
    font_donor_          = std::move(font.donor);
    font_                = std::move(font.font);
    next_registry_check_ = now + std::chrono::seconds(5);
    return true;
  }

  bool PanelValid() const
  {
    return unity::Alive(root_.Get()) && unity::Alive(heading_.Get()) && unity::Alive(left_.Get())
           && unity::Alive(root_rect_.Get()) && unity::Alive(heading_rect_.Get()) && unity::Alive(left_rect_.Get())
           && ((!right_ && !right_rect_) || (unity::Alive(right_.Get()) && unity::Alive(right_rect_.Get())));
  }

  void ClearPanel()
  {
    if (auto* object = root_.Get<unity::GameObject>(); unity::Alive(object)) {
      object->SetActive(false);
      unity::Destroy(object);
    }
    heading_.Reset();
    left_.Reset();
    right_.Reset();
    root_.Reset();
    root_rect_.Reset();
    heading_rect_.Reset();
    left_rect_.Reset();
    right_rect_.Reset();
    shown_heading_.clear();
    shown_left_.clear();
    shown_right_.clear();
    layout_valid_ = false;
    prepared_     = false;
    visible_      = false;
  }

  static FontSource FindFont(HudNavigationViewController* nav)
  {
    FontSource result;
    result.faction = il2cpp::GcHandle::Strong(nav->factionInformationHelper());
    auto* helper   = result.faction.Get<FactionInformation>();
    auto  labels   = il2cpp::GcHandle::Strong(helper ? helper->systemName() : nullptr);
    auto* names    = labels.Get<il2cpp::List<unity::Component*>>();
    if (!names)
      return {};
    const auto count = names->TryCall<"get_Count", int32_t>();
    if (!count || count.value < 0)
      return {};
    for (int32_t index = 0; index < count.value; ++index) {
      const auto item      = names->TryCall<"get_Item", unity::Component*>(index);
      auto       component = il2cpp::GcHandle::Strong(item ? item.value : nullptr);
      auto*      source    = component.Get<unity::Component>();
      auto       object    = il2cpp::GcHandle::Strong(unity::Alive(source) ? source->gameObject() : nullptr);
      auto*      owner     = object.Get<unity::GameObject>();
      auto       donor     = il2cpp::GcHandle::Strong(owner ? owner->GetComponent<unity::TextMeshProUGUI>() : nullptr);
      auto*      label     = donor.Get<unity::TextMeshProUGUI>();
      if (!unity::Alive(label) || !label->isActiveAndEnabled())
        continue;
      auto font = il2cpp::GcHandle::Strong(label->font());
      if (!unity::Alive(font.Get()))
        continue;
      result.donor = std::move(donor);
      result.font  = std::move(font);
      return result;
    }
    return {};
  }

  static unity::Sprite* PanelSprite(HudNavigationViewController* nav)
  {
    auto* button     = nav->galacticAnomaliesButtonWidget();
    auto* transform  = button ? button->transform() : nullptr;
    auto* parent     = transform ? transform->parent() : nullptr;
    auto* background = parent ? parent->Find("ShortcutKeybindHint/Background") : nullptr;
    auto* object     = background ? background->gameObject() : nullptr;
    auto* image      = object ? object->GetComponent<unity::Image>() : nullptr;
    if (!image)
      return nullptr;
    auto* sprite = image->overrideSprite();
    return sprite ? sprite : image->sprite();
  }

  static void SetText(const il2cpp::GcHandle& handle, std::string_view content, std::string& shown)
  {
    if (content != shown) {
      handle.Get<unity::TextMeshProUGUI>()->SetText(content);
      shown = content;
    }
  }

  void Place(size_t heading_lines, size_t left_count, size_t right_count, bool anomaly_timer)
  {
    constexpr float padding = 8, row_height = 20, gap = 12;
    const float     width          = right_count ? 388 : 224;
    const auto      heading_height = static_cast<float>(heading_lines) * row_height + 2;
    const auto      left_height    = static_cast<float>(left_count) * row_height + 4;
    const auto      right_height   = static_cast<float>(right_count) * row_height + 4;
    const auto      height =
        padding * 2 + heading_height + (left_count || right_count ? std::max(left_height, right_height) : 0);
    const auto column_width = right_count ? (width - padding * 2 - gap) / 2 : width - padding * 2;
    // Follow the native drawer's right edge so expanding to two columns grows
    // toward the game area rather than beyond the right edge of the screen.
    root_rect_.Get<unity::RectTransform>()->Place({1, 0}, {1, 0}, {1, 1}, {width, height},
                                                  {-4, anomaly_timer ? -84.0f : -40.0f});
    heading_rect_.Get<unity::RectTransform>()->Place({0, 1}, {0, 1}, {0, 1}, {width - padding * 2, heading_height},
                                                     {padding, -padding});
    left_rect_.Get<unity::RectTransform>()->Place({0, 1}, {0, 1}, {0, 1}, {column_width, left_height},
                                                  {padding, -padding - heading_height});
    if (right_) {
      right_rect_.Get<unity::RectTransform>()->Place({0, 1}, {0, 1}, {0, 1}, {column_width, right_height},
                                                     {padding + column_width + gap, -padding - heading_height});
      right_.Get<unity::TextMeshProUGUI>()->gameObject()->SetActive(right_count != 0);
    }
    heading_.Get<unity::TextMeshProUGUI>()->gameObject()->SetActive(true);
    left_.Get<unity::TextMeshProUGUI>()->gameObject()->SetActive(left_count != 0);
  }

  unity::TextMeshProUGUI* CreateLabel(const char* name, unity::RectTransform* parent, il2cpp::GcHandle& object,
                                      il2cpp::GcHandle& rect_root, unity::TMP_Text::Alignment alignment,
                                      bool summary = false)
  {
    auto* rect  = unity::CreateUiObject(name, parent, object);
    rect_root   = il2cpp::GcHandle::Strong(rect);
    auto* label = rect ? object.Get<unity::GameObject>()->AddComponent<unity::TextMeshProUGUI>() : nullptr;
    if (!rect_root || !label)
      return nullptr;
    label->SetFont(font_.Get<il2cpp::Object>());
    label->SetFontSize(16);
    label->SetColor(summary ? unity::Color{0.38f, 0.88f, 1.0f, 1} : unity::Color{0.86f, 0.91f, 0.96f, 1});
    label->SetAlignment(alignment);
    label->SetRaycastTarget(false);
    label->SetEnableWordWrapping(false);
    label->SetEnableAutoSizing(false);
    label->SetRichText(false);
    label->SetOverflowMode(unity::TMP_Text::Overflow::Ellipsis);
    // CreateUiObject leaves the child inactive. Show publishes it only after
    // text and geometry are ready, including a newly added right column.
    return label;
  }

  bool EnsureRightColumn()
  {
    if (right_)
      return true;
    il2cpp::GcHandle object, rect;
    auto*            label = CreateLabel("AmplifiersRight", root_rect_.Get<unity::RectTransform>(), object, rect,
                                         unity::TMP_Text::Alignment::Left);
    auto             root  = il2cpp::GcHandle::Strong(label);
    if (!rect || !root) {
      if (auto* child = object.Get<unity::GameObject>(); unity::Alive(child))
        unity::Destroy(child);
      return false;
    }
    right_        = std::move(root);
    right_rect_   = std::move(rect);
    layout_valid_ = false;
    return true;
  }

  bool Create(HudNavigationViewController* nav)
  {
    il2cpp::GcHandle panel, heading_object, left_object;
    il2cpp::GcHandle panel_rect, heading_rect, left_rect;
    auto* rect       = unity::CreateUiObject("CommunityMod_HyperthermicDecay", parent_.Get<unity::Transform>(), panel);
    panel_rect       = il2cpp::GcHandle::Strong(rect);
    auto* object     = panel.Get<unity::GameObject>();
    auto* background = rect && object ? object->AddComponent<unity::Image>() : nullptr;
    auto* heading    = background ? CreateLabel("Summary", rect, heading_object, heading_rect,
                                                unity::TMP_Text::Alignment::Center, true)
                                  : nullptr;
    auto* left = heading ? CreateLabel("AmplifiersLeft", rect, left_object, left_rect, unity::TMP_Text::Alignment::Left)
                         : nullptr;
    if (!panel_rect || !left) {
      if (unity::Alive(object))
        unity::Destroy(object);
      return false;
    }
    background->SetColor({0.04f, 0.08f, 0.12f, 0.94f});
    background->SetRaycastTarget(false);
    if (auto* sprite = PanelSprite(nav)) {
      background->SetSprite(sprite);
      background->SetType(unity::Image::Type::Sliced);
      if (sprite->pixelsPerUnit() > 0)
        background->SetPixelsPerUnitMultiplier(100 / sprite->pixelsPerUnit());
    }
    root_         = std::move(panel);
    root_rect_    = std::move(panel_rect);
    heading_rect_ = std::move(heading_rect);
    left_rect_    = std::move(left_rect);
    heading_      = il2cpp::GcHandle::Strong(heading);
    left_         = il2cpp::GcHandle::Strong(left);
    if (!heading_ || !left_) {
      ClearPanel();
      return false;
    }
    layout_valid_ = false;
    return true;
  }

  il2cpp::GcHandle  navigation_, drawer_, promotion_, chest_, parent_, parent_object_, faction_, font_donor_, font_;
  il2cpp::GcHandle  root_, heading_, left_, right_, root_rect_, heading_rect_, left_rect_, right_rect_;
  Clock::time_point next_registry_check_{};
  std::string       shown_heading_, shown_left_, shown_right_;
  size_t            heading_lines_ = 0, left_count_ = 0, right_count_ = 0;
  bool              layout_valid_ = false, anomaly_timer_ = false, prepared_ = false, visible_ = false;
};
} // namespace hyperthermic_decay_hud
