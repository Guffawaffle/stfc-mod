// [graphics] galactic_anomaly_timer and mirror_space_timer share a countdown under the HUD's Help All / Engage /
// Claim drawer. The earliest active Mirror ship countdown in the viewed system takes priority. All Unity work
// runs on the frame tick; there are no detours of its own.

#include "config.h"
#include "features/features.h"
#include "features/navigation_hud.h"

#include "game/app.h"
#include "game/fleet.h"

#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/ui.h>

#include <chrono>
#include <cstdint>
#include <utility>

namespace
{
using AnomalyClock = std::chrono::steady_clock;

template <typename T> il2cpp::GcHandle AnomalyTimerRoot(const il2cpp::CallResult<T*>& observed)
{ return observed ? il2cpp::GcHandle::Strong(observed.value) : il2cpp::GcHandle{}; }

int64_t MirrorRemainingTicks(HudNavigationViewController* nav)
{
  auto context = AnomalyTimerRoot(nav->TryCall<"get_CanvasContext", NavigationViewData*>());
  auto address =
      context ? AnomalyTimerRoot(context.Get<NavigationViewData>()->TryCall<"get_ViewingAddress", NodeAddress*>())
              : il2cpp::GcHandle{};
  const auto system = address ? address.Get<NodeAddress>()->TrySystem() : il2cpp::CallResult<int64_t>{};
  if (!system || system.value <= 0)
    return 0;

  auto bar = il2cpp::GcHandle::Strong(unity::Newest<FleetBarViewController>());
  auto fleets =
      bar ? AnomalyTimerRoot(bar.Get<FleetBarViewController>()->TryCall<"get_CanvasContext", FleetBarContext*>())
          : il2cpp::GcHandle{};
  auto data = fleets ? AnomalyTimerRoot(fleets.Get<FleetBarContext>()->TryCall<"get_FleetData", il2cpp::ArrayObject*>())
                     : il2cpp::GcHandle{};
  if (!data)
    return 0;
  const auto checked = il2cpp::TryReferenceArray<FleetPlayerData*>(data.Get());
  if (!checked || !checked.value)
    return 0;

  int64_t remaining = 0;
  for (auto* entry : il2cpp::Items(checked.value)) {
    auto fleet = il2cpp::GcHandle::Strong(entry);
    if (!fleet)
      continue;
    auto*      player = fleet.Get<FleetPlayerData>();
    const auto mirror = player->TryIsInMirrorUniverse();
    if (!mirror)
      return 0;
    if (!mirror.value)
      continue;
    const auto observed_location = player->TryAddress();
    if (!observed_location)
      return 0;
    auto location = AnomalyTimerRoot(observed_location);
    if (!location)
      continue;
    const auto fleet_system = location.Get<NodeAddress>()->TrySystem();
    if (!fleet_system)
      return 0;
    if (fleet_system.value != system.value)
      continue;

    StatusEffect* effect = nullptr;
    const auto    found  = player->TryGetStatusEffectData(StatusEffectType::MirrorUniverseTimer, effect);
    if (!found)
      return 0;
    auto status = found.value ? il2cpp::GcHandle::Strong(effect) : il2cpp::GcHandle{};
    if (!status)
      continue;
    const auto active = status.Get<StatusEffect>()->TryIsStatusActive();
    if (!active)
      return 0;
    if (!active.value)
      continue;
    const auto time = status.Get<StatusEffect>()->TryRemainingTimespan();
    if (!time)
      return 0;
    if (time.value.ticks > 0 && (remaining == 0 || time.value.ticks < remaining))
      remaining = time.value.ticks;
  }
  return remaining;
}

// The rounded key-hint background next to the native anomaly button, reused for the panel.
unity::Sprite* NativeBackgroundSprite(HudNavigationViewController* nav)
{
  auto* button     = nav->galacticAnomaliesButtonWidget();
  auto* transform  = button ? button->transform() : nullptr;
  auto* parent     = transform ? transform->parent() : nullptr;
  auto* background = parent ? parent->Find("ShortcutKeybindHint/Background") : nullptr;
  auto* object     = background ? background->gameObject() : nullptr;
  auto* image      = object ? object->GetComponent<unity::Image>() : nullptr;
  if (!image)
    return nullptr;
  // ImageSelector applies localized artwork through overrideSprite.
  auto* sprite = image->overrideSprite();
  return sprite ? sprite : image->sprite();
}

unity::Image* AddPanelImage(unity::GameObject* object, unity::Color tint, unity::Sprite* sprite)
{
  auto* image = object->AddComponent<unity::Image>();
  if (!image)
    return nullptr;
  image->SetColor(tint);
  image->SetRaycastTarget(false);
  if (sprite && sprite->pixelsPerUnit() > 0) {
    image->SetSprite(sprite);
    image->SetType(unity::Image::Type::Sliced);
    // Sliced borders are drawn at the sprite's pixel density; normalizing it to 100 keeps the corners the same size
    // whatever the localized artwork's density is.
    image->SetPixelsPerUnitMultiplier(100 / sprite->pixelsPerUnit());
  }
  return image;
}

// The countdown panel under the upper-right Help All / Engage / Claim drawer. Parenting to the drawer follows its
// scale and visibility without moving any native button.
class AnomalyTimer
{
public:
  void Update()
  {
    const auto now = AnomalyClock::now();
    if (now < next_update_)
      return;
    next_update_ = now + std::chrono::milliseconds(250);
    const bool anomaly_enabled = Config::Get().graphics.galactic_anomaly_timer;
    const bool mirror_enabled  = Config::Get().graphics.mirror_space_timer;
    if (!anomaly_enabled && !mirror_enabled) {
      DestroyPanel();
      navigation_.Reset();
      return;
    }

    auto* nav = Navigation(now);
    int64_t ticks = nav && mirror_enabled ? MirrorRemainingTicks(nav) : 0;
    if (ticks <= 0 && nav && anomaly_enabled && nav->CanShowGalacticAnomaly())
      ticks = RemainingTicks(nav);
    auto* label = Label();
    if (ticks <= 0) {
      if (auto* panel = label ? label->ParentObject() : nullptr)
        panel->SetActive(false);
      return;
    }
    if (!label)
      label = Create(nav);
    if (!label) {
      static bool warned = false;
      if (!std::exchange(warned, true))
        spdlog::warn("[AnomalyTimer] could not build the countdown panel; leaving the HUD unchanged");
      DestroyPanel();
      next_update_ = now + std::chrono::seconds(5);
      return;
    }

    // TimeSpan ticks are 100 ns. Round up, so the last second shows as 1s rather than 0s.
    const auto seconds = ticks / 10'000'000 + (ticks % 10'000'000 != 0);
    if (seconds != shown_seconds_) {
      shown_seconds_  = seconds;
      const auto text = seconds >= 3600
                            ? fmt::format("{}h {:02}m {:02}s", seconds / 3600, seconds / 60 % 60, seconds % 60)
                            : fmt::format("{:02}m {:02}s", seconds / 60, seconds % 60);
      label->SetText(text);
    }
    if (auto* panel = label->ParentObject())
      panel->SetActive(true);
  }

private:
  // Navigation and the alliance drawer have separate registered owner canvases.
  HudNavigationViewController* Navigation(AnomalyClock::time_point now)
  {
    if (Hub::CurrentSection() != SectionID::Navigation_System) {
      DestroyPanel();
      navigation_.Reset();
      next_discovery_ = {};
      return nullptr;
    }
    auto* nav = navigation_.Get<HudNavigationViewController>();
    if (now < next_discovery_)
      return navigation_hud::Active(nav) ? nav : nullptr;
    next_discovery_ = now + std::chrono::seconds(1);
    auto current    = navigation_hud::FindNavigation();
    if (current.Get() != navigation_.Get()) {
      DestroyPanel();
      navigation_ = std::move(current);
    } else if (label_ && !PanelOwnerCurrent()) {
      DestroyPanel();
    }
    nav = navigation_.Get<HudNavigationViewController>();
    return navigation_hud::Active(nav) ? nav : nullptr;
  }

  bool PanelOwnerCurrent()
  {
    auto current = navigation_hud::FindAllianceDrawer();
    if (!unity::Alive(current.Get()) || current.Get() != drawer_.Get())
      return false;
    auto anchor = navigation_hud::FindDrawerAnchor(current.Get<HudAllianceAndNewsViewController>());
    return unity::Alive(anchor.parent.Get()) && anchor.parent.Get() == drawer_parent_.Get();
  }

  unity::TMP_Text* Label()
  {
    auto* label = label_.Get<unity::TMP_Text>();
    return unity::Alive(label) ? label : nullptr;
  }

  void DestroyPanel()
  {
    if (auto* panel = Label() ? Label()->ParentObject() : nullptr) {
      panel->SetActive(false);
      unity::Destroy(panel);
    }
    label_.Reset();
    drawer_parent_.Reset();
    drawer_.Reset();
    shown_seconds_ = -1;
  }

  static int64_t RemainingTicks(HudNavigationViewController* nav)
  {
    auto*   context = nav->CanvasContext();
    auto*   address = context ? context->ViewingAddress() : nullptr;
    int64_t system  = address ? address->System() : -1;
    auto*   manager = system > 0 ? GalacticAnomaliesManager::Current() : nullptr;
    auto*   anomaly = manager ? manager->GetSystemGalacticAnomalies(system) : nullptr;
    if (!anomaly || !anomaly->IsActive())
      return 0;
    auto* timer = anomaly->EndTimeTimerDataContext();
    return timer ? timer->RemainingTime().ticks : 0;
  }

  // Builds the panel: the native rounded sprite twice, with a narrow inset making the border, and a countdown in
  // the system-name font. A missing sprite still gives a visible frame. Returns the countdown label, or null.
  unity::TMP_Text* Create(HudNavigationViewController* nav)
  {
    auto  names       = navigation_hud::FindSystemNames(nav);
    auto  name_object = navigation_hud::SystemNameObject(il2cpp::At(names.Get<il2cpp::List<unity::Component*>>(), 0));
    auto* object      = name_object.Get<unity::GameObject>();
    auto  font_source = il2cpp::GcHandle::Strong(object ? object->GetComponent<unity::TextMeshProUGUI>() : nullptr);
    auto* source      = font_source.Get<unity::TextMeshProUGUI>();
    auto  font_root   = il2cpp::GcHandle::Strong(source ? source->font() : nullptr);
    auto* font        = font_root.Get<il2cpp::Object>();
    if (!unity::Alive(font))
      return nullptr;

    auto  drawer = navigation_hud::FindAllianceDrawer();
    auto  anchor = navigation_hud::FindDrawerAnchor(drawer.Get<HudAllianceAndNewsViewController>());
    auto* parent = anchor.parent.Get<unity::Transform>();
    if (!unity::Alive(parent))
      return nullptr;

    il2cpp::GcHandle panel, fill, text;
    auto*            panel_rect = unity::CreateUiObject("CommunityMod_AnomalyTimer", parent, panel);
    if (!panel_rect)
      return nullptr;
    panel_rect->Place({0.5f, 0}, {0.5f, 0}, {0.5f, 1}, {198, 38}, {0, -40});
    auto* rounded = NativeBackgroundSprite(nav);
    if (!AddPanelImage(panel.Get<unity::GameObject>(), {0.64f, 0.34f, 0.46f, 0.85f}, rounded)) {
      Abandon(panel);
      return nullptr;
    }

    auto* fill_rect = unity::CreateUiObject("AnomalyFill", panel_rect, fill);
    if (!fill_rect || !AddPanelImage(fill.Get<unity::GameObject>(), {0.06f, 0.10f, 0.14f, 0.96f}, rounded)) {
      Abandon(panel);
      return nullptr;
    }
    fill_rect->Place({0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {194, 34}, {0, 0});
    fill.Get<unity::GameObject>()->SetActive(true);

    auto* text_rect = unity::CreateUiObject("Countdown", panel_rect, text);
    auto* label     = text_rect ? text.Get<unity::GameObject>()->AddComponent<unity::TextMeshProUGUI>() : nullptr;
    if (!label) {
      Abandon(panel);
      return nullptr;
    }
    text_rect->Place({0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {190, 28}, {0, 0});
    label->SetFont(font);
    label->SetFontSize(22);
    label->SetAlignment(unity::TMP_Text::Alignment::Center);
    label->SetColor({0.95f, 0.83f, 0.89f, 1});
    label->SetRaycastTarget(false);
    label->SetEnableWordWrapping(false);
    text.Get<unity::GameObject>()->SetActive(true);
    // The hierarchy owns the children; keep the label and drawer mount to detect owner replacement.
    label_ = il2cpp::GcHandle::Strong(label);
    drawer_        = std::move(drawer);
    drawer_parent_ = std::move(anchor.parent);
    return label;
  }

  // Destroys a half-built panel.
  static void Abandon(il2cpp::GcHandle& panel)
  {
    if (auto* object = panel.Get<unity::GameObject>(); unity::Alive(object))
      unity::Destroy(object);
  }

  il2cpp::GcHandle         navigation_, drawer_, drawer_parent_, label_;
  AnomalyClock::time_point next_update_{}, next_discovery_{};
  int64_t                  shown_seconds_ = -1;
};

// Leaked on purpose: its GC handles must not be released by static destructors after the runtime is gone.
AnomalyTimer* anomaly_timer = nullptr;

void AnomalyTimerFrame()
{ anomaly_timer->Update(); }

bool InstallAnomalyTimer()
{
  anomaly_timer = new AnomalyTimer;
  return true;
}
} // namespace

const Feature kAnomalyTimer{"AnomalyTimer", &Config::Patches::galacticanomalytimerhooks, InstallAnomalyTimer,
                            AnomalyTimerFrame};
