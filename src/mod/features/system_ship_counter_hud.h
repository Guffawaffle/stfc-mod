#pragma once

#include "system_ship_counter_state.h"

#include "game/app.h"
#include "game/hud.h"
#include "game/ui.h"

#include <il2cpp/il2cpp.h>
#include <unity/ui.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace system_ship_counter_hud
{
using Scope = system_ship_counter::Scope;

// Main-thread UI custody only. The feature owns the observations and count.
// As with other native HUD helpers, the owner must Clear before runtime teardown.
class Hud
{
public:
  std::optional<Scope> Poll()
  {
    const auto now = Clock::now();
    if (now < next_poll_)
      return scope_;
    next_poll_ = now + std::chrono::milliseconds(250);

    if (Hub::CurrentSection() != SectionID::Navigation_System)
      return Unavailable();
    auto navigation = il2cpp::GcHandle::Strong(
        ScreenManager::RegisteredController<HudNavigationViewController, NavigationViewData>());
    auto* nav = navigation.Get<HudNavigationViewController>();
    if (!unity::Alive(nav) || !nav->isActiveAndEnabled())
      return Unavailable();

    auto  context = il2cpp::GcHandle::Strong(nav->CanvasContext());
    auto* data    = context.Get<NavigationViewData>();
    auto  address = il2cpp::GcHandle::Strong(data ? data->ViewingAddress() : nullptr);
    auto* viewed  = address.Get<NodeAddress>();
    if (!viewed)
      return Unavailable();
    const auto galaxy   = viewed->TryCall<"get_Galaxy", int64_t>();
    const auto system   = viewed->TrySystem();
    const auto instance = viewed->TryInstance();
    if (!galaxy || !system || system.value <= 0 || !instance)
      return Unavailable();
    const Scope scope{galaxy.value, system.value, instance.value};

    auto  helper      = il2cpp::GcHandle::Strong(nav->factionInformationHelper());
    auto* information = helper.Get<FactionInformation>();
    auto  names       = il2cpp::GcHandle::Strong(information ? information->systemName() : nullptr);
    auto* labels      = names.Get<il2cpp::List<unity::Component*>>();
    if (!labels)
      return Unavailable();
    const auto count = labels->TryCall<"get_Count", int32_t>();
    if (!count || count.value < 0)
      return Unavailable();

    // Native HUD variants have separate localized system-name labels. Follow
    // the active one; an inactive variant is not a usable counter parent.
    for (int32_t index = 0; index < count.value; ++index) {
      const auto item      = labels->TryCall<"get_Item", unity::Component*>(index);
      auto       name      = il2cpp::GcHandle::Strong(item ? item.value : nullptr);
      auto*      component = name.Get<unity::Component>();
      if (!unity::Alive(component))
        continue;
      auto       object      = il2cpp::GcHandle::Strong(component->gameObject());
      auto*      game_object = object.Get<unity::GameObject>();
      const auto active      = game_object ? game_object->TryActiveInHierarchy() : il2cpp::CallResult<bool>{};
      if (!active || !active.value)
        continue;
      auto  donor = il2cpp::GcHandle::Strong(game_object->GetComponent<unity::TextMeshProUGUI>());
      auto* label = donor.Get<unity::TextMeshProUGUI>();
      if (!unity::Alive(label) || !label->isActiveAndEnabled())
        continue;
      auto  parent    = il2cpp::GcHandle::Strong(label->transform());
      auto* transform = parent.Get<unity::Transform>();
      auto  ancestor  = il2cpp::GcHandle::Strong(transform ? transform->parent() : nullptr);
      auto  font      = il2cpp::GcHandle::Strong(label->font());
      if (!unity::Alive(transform) || !unity::Alive(ancestor.Get()) || !unity::Alive(font.Get()))
        continue;

      if (navigation_.Get() != navigation.Get() || donor_.Get() != donor.Get() || parent_.Get() != parent.Get()
          || ancestor_.Get() != ancestor.Get() || font_.Get() != font.Get()) {
        DestroyLabel();
        navigation_ = std::move(navigation);
        donor_      = std::move(donor);
        parent_     = std::move(parent);
        ancestor_   = std::move(ancestor);
        font_       = std::move(font);
      }
      if (scope_ != scope) {
        Hide();
        shown_text_.clear();
      }
      scope_ = scope;
      if (root_ && !unity::Alive(root_.Get()))
        DestroyLabel();
      return scope_;
    }
    return Unavailable();
  }

  void Show(std::string_view text)
  {
    if (!scope_ || text.empty()) {
      Hide();
      return;
    }
    auto* label = label_.Get<unity::TextMeshProUGUI>();
    if (!label && !(label = Create()))
      return;
    if (shown_text_ != text) {
      label->SetText(text);
      shown_text_ = text;
    }
    if (!shown_) {
      root_.Get<unity::GameObject>()->SetActive(true);
      shown_ = true;
    }
  }

  void Clear()
  {
    ClearBinding();
    next_poll_ = {};
  }

private:
  using Clock = std::chrono::steady_clock;

  std::optional<Scope> Unavailable()
  {
    ClearBinding();
    return std::nullopt;
  }

  void Hide()
  {
    if (shown_) {
      if (auto* object = root_.Get<unity::GameObject>(); unity::Alive(object))
        object->SetActive(false);
      shown_ = false;
    }
  }

  void DestroyLabel()
  {
    Hide();
    if (auto* object = root_.Get<unity::GameObject>(); unity::Alive(object))
      unity::Destroy(object);
    label_.Reset();
    root_.Reset();
    shown_text_.clear();
  }

  void ClearBinding()
  {
    DestroyLabel();
    navigation_.Reset();
    donor_.Reset();
    parent_.Reset();
    ancestor_.Reset();
    font_.Reset();
    scope_.reset();
  }

  unity::TextMeshProUGUI* Create()
  {
    auto* parent = parent_.Get<unity::Transform>();
    auto* font   = font_.Get<il2cpp::Object>();
    if (!unity::Alive(parent) || !unity::Alive(font))
      return nullptr;
    il2cpp::GcHandle root;
    auto*            rect   = unity::CreateUiObject("CommunityMod_KnownSystemPlayerShips", parent, root);
    auto*            object = root.Get<unity::GameObject>();
    auto  label  = il2cpp::GcHandle::Strong(rect && object ? object->AddComponent<unity::TextMeshProUGUI>() : nullptr);
    auto* text   = label.Get<unity::TextMeshProUGUI>();
    auto* layout = text ? object->AddComponent<unity::LayoutElement>() : nullptr;
    if (!text || !layout || !layout->ExcludeFromLayout() || !text->TryCall<"set_raycastTarget", void(bool)>(false)) {
      if (unity::Alive(object))
        unity::Destroy(object);
      return nullptr;
    }
    // Independent child geometry leaves the localized title and its layout intact.
    rect->Place({0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {320, 24}, {0, -42});
    text->SetFont(font);
    text->SetFontSize(16);
    text->SetAlignment(unity::TMP_Text::Alignment::Center);
    text->SetColor({0.82f, 0.87f, 0.92f, 1});
    text->SetEnableWordWrapping(false);
    text->SetEnableAutoSizing(false);
    text->SetRichText(false);
    text->SetOverflowMode(unity::TMP_Text::Overflow::Ellipsis);
    root_  = std::move(root);
    label_ = std::move(label);
    return text;
  }

  il2cpp::GcHandle     navigation_, donor_, parent_, ancestor_, font_, root_, label_;
  Clock::time_point    next_poll_{};
  std::optional<Scope> scope_;
  std::string          shown_text_;
  bool                 shown_ = false;
};
} // namespace system_ship_counter_hud
