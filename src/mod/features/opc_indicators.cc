// [ui] highlight_opc_fleets and fleet_hud_opc_eta restore PLAY's native fleet
// mining indicators. Both are off by default and use the same qualified sample.
#include "config.h"
#include "features/features.h"
#include "features/fleet_opc_sample.h"

#include "game/fleet_opc.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
constexpr std::string_view kOpcHighlight = "CommunityOPCHighlight";
constexpr std::string_view kOpcLabel     = "CommunityOpcEtaLabel";
constexpr std::string_view kOpcBacking   = "CommunityOpcEtaBackground";
constexpr std::string_view kOpcCardLabel = "CommunityOpcCardEtaLabel";

enum class OpcOwner : uint8_t { Local, State, Flag };

struct OpcView {
  il2cpp::GcHandle source;
  OpcOwner         owner      = OpcOwner::Local;
  uint64_t         generation = 0;
  int64_t          retry      = 0;
  uint8_t          failures   = 0;
  int64_t          fleet      = 0;
  uintptr_t        body       = 0;
  uintptr_t        timer      = 0;
};
struct OpcUi {
  bool                                         installed      = false;
  bool                                         updating       = false;
  uint8_t                                      options_active = 0;
  bool                                         touched        = false;
  uint64_t                                     generation     = 0;
  int64_t                                      next           = 0;
  std::unordered_map<il2cpp::Object*, OpcView> views;
};
OpcUi& Opc()
{
  // Weak handles have process lifetime; no IL2CPP API runs at C++ teardown.
  static auto* state = new OpcUi;
  return *state;
}
int64_t OpcNow()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void OpcShow(unity::Transform* parent, std::string_view name, bool show)
{
  if (auto* object = fleet_opc_binding::ChildObject(parent, name))
    object->SetActive(show);
}
void OpcHideBody(unity::Transform* body)
{
  OpcShow(body, kOpcHighlight, false);
  OpcShow(body, kOpcLabel, false);
  OpcShow(body, kOpcBacking, false);
}

unity::GameObject* OpcCreateHighlight(unity::Transform* body)
{
  auto* source        = fleet_opc_binding::ChildObject(body, "Background");
  auto* source_mining = fleet_opc_binding::ChildObject(body, "MiningContainer");
  auto* mining        = source_mining ? source_mining->GetComponent<unity::Image>() : nullptr;
  auto* sprite        = mining ? mining->sprite() : nullptr;
  if (!source || !sprite)
    return nullptr;
  auto sprite_root = il2cpp::GcHandle::Strong(sprite);
  if (!sprite_root)
    return nullptr;
  auto* copy = unity::Instantiate(source, body);
  auto  root = il2cpp::GcHandle::Strong(copy);
  if (!copy)
    return nullptr;
  if (!root) {
    unity::Destroy(copy);
    return nullptr;
  }
  copy->SetActive(false);
  auto* transform    = copy->transform();
  auto* outer        = copy->GetComponent<unity::Image>();
  auto* inner_object = fleet_opc_binding::ChildObject(transform, "WaveDefenseBackground");
  auto* inner        = inner_object ? inner_object->GetComponent<unity::Image>() : nullptr;
  if (!transform || !outer || !inner) {
    unity::Destroy(copy);
    return nullptr;
  }
  for (auto* child : transform->Children())
    if (auto* object = child ? child->gameObject() : nullptr)
      object->SetActive(false);
  auto*      source_transform = source->transform();
  const auto source_index =
      source_transform ? source_transform->TryCall<"GetSiblingIndex", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!outer->TryCall<"set_color", void(unity::Color)>(unity::Color{1.0f, 0.66f, 0.12f, 1.0f})
      || !outer->TryCall<"set_raycastTarget", void(bool)>(false)
      || !inner->TryCall<"set_sprite", void(unity::Sprite*)>(sprite_root.Get<unity::Sprite>())
      || !inner->TryCall<"set_type", void(unity::Image::Type)>(unity::Image::Type::Filled)
      || !inner->TryCall<"set_fillAmount", void(float)>(1.0f)
      || !inner->TryCall<"set_color", void(unity::Color)>(unity::Color{0.78f, 0.40f, 0.06f, 0.62f})
      || !inner->TryCall<"set_raycastTarget", void(bool)>(false) || !source_index
      || !transform->TryCall<"SetSiblingIndex", void(int32_t)>(source_index.value)
      || !copy->TryCall<"set_name", void(Il2CppString*)>(il2cpp::NewString(kOpcHighlight))) {
    unity::Destroy(copy);
    return nullptr;
  }
  inner_object->SetActive(true);
  return copy;
}

bool OpcConfigureLabel(unity::TextMeshProUGUI* label)
{
  return label->TryCall<"set_fontSize", void(float)>(18.0f)
         && label->TryCall<"set_alignment", void(unity::TMP_Text::Alignment)>(unity::TMP_Text::Alignment::Center)
         && label->TryCall<"set_raycastTarget", void(bool)>(false);
}
bool OpcConfigureBacking(unity::Image* image)
{
  return image->TryCall<"set_color", void(unity::Color)>(unity::Color{0.035f, 0.075f, 0.10f, 0.86f})
         && image->TryCall<"set_raycastTarget", void(bool)>(false);
}
unity::TextMeshProUGUI* OpcEnsureLabel(unity::Transform* parent, std::string_view name)
{
  if (!parent)
    return nullptr;
  if (auto* existing = fleet_opc_binding::ChildObject(parent, name)) {
    if (auto* label = existing->GetComponent<unity::TextMeshProUGUI>(); label && label->transform())
      return label;
    // Unity destruction is deferred. Do not create a duplicate discoverable
    // name beside invalid artwork in this frame.
    unity::Destroy(existing);
    return nullptr;
  }
  // Preserve the game's current font when a native label is available locally.
  auto*            parent_object = parent->gameObject();
  auto*            donor = parent_object ? parent_object->GetComponentInChildren<unity::TextMeshProUGUI>() : nullptr;
  auto             font  = il2cpp::GcHandle::Strong(donor ? donor->font() : nullptr);
  il2cpp::GcHandle root;
  auto*            rect   = unity::CreateUiObject(name, parent, root);
  auto*            object = rect ? rect->gameObject() : nullptr;
  auto*            label  = object ? object->AddComponent<unity::TextMeshProUGUI>() : nullptr;
  if (!label || !OpcConfigureLabel(label)) {
    if (object)
      unity::Destroy(object);
    return nullptr;
  }
  if (font)
    label->SetFont(font.Get<il2cpp::Object>());
  label->SetEnableWordWrapping(false);
  label->SetEnableAutoSizing(false);
  label->SetRichText(true);
  if (auto* layout = object->AddComponent<unity::LayoutElement>())
    (void)layout->ExcludeFromLayout();
  return label;
}
unity::GameObject* OpcEnsureBacking(unity::Transform* parent)
{
  if (auto* existing = fleet_opc_binding::ChildObject(parent, kOpcBacking)) {
    if (existing->GetComponent<unity::Image>())
      return existing;
    unity::Destroy(existing);
    return nullptr;
  }
  il2cpp::GcHandle root;
  auto*            rect   = unity::CreateUiObject(kOpcBacking, parent, root);
  auto*            object = rect ? rect->gameObject() : nullptr;
  auto*            image  = object ? object->AddComponent<unity::Image>() : nullptr;
  if (!image || !OpcConfigureBacking(image)) {
    if (object)
      unity::Destroy(object);
    return nullptr;
  }
  if (auto* layout = object->AddComponent<unity::LayoutElement>())
    (void)layout->ExcludeFromLayout();
  return object;
}
bool OpcText(unity::TextMeshProUGUI* label, const std::string& text)
{
  const auto old = label->TryCall<"get_text", Il2CppString*>();
  return (old && il2cpp::Equals(old.value, text))
         || static_cast<bool>(label->TryCall<"set_text", void(Il2CppString*)>(il2cpp::NewString(text)));
}
bool OpcLabelMatches(unity::Transform* parent, std::string_view name, const std::string& text)
{
  auto* object = fleet_opc_binding::ChildObject(parent, name);
  auto  root   = il2cpp::GcHandle::Strong(object ? object->GetComponent<unity::TextMeshProUGUI>() : nullptr);
  if (!root)
    return false;
  const auto observed = root.Get<unity::TextMeshProUGUI>()->TryCall<"get_text", Il2CppString*>();
  return observed && il2cpp::Equals(observed.value, text);
}
bool OpcPlace(unity::RectTransform* rect, unity::Vector2 anchor, unity::Vector2 pivot, unity::Vector2 size,
              unity::Vector2 position)
{
  return rect->TryCall<"set_anchorMin", void(unity::Vector2)>(anchor)
         && rect->TryCall<"set_anchorMax", void(unity::Vector2)>(anchor)
         && rect->TryCall<"set_pivot", void(unity::Vector2)>(pivot)
         && rect->TryCall<"set_sizeDelta", void(unity::Vector2)>(size)
         && rect->TryCall<"set_anchoredPosition", void(unity::Vector2)>(position)
         && rect->TryCall<"SetAsLastSibling", void>();
}

bool OpcRenderEta(unity::Transform* body, bool selected, const fleet_opc::Status& status)
{
  const auto display = fleet_opc::Display(status);
  if (!body || display.empty()) {
    OpcShow(body, kOpcLabel, false);
    OpcShow(body, kOpcBacking, false);
    return true;
  }
  auto* backing           = OpcEnsureBacking(body);
  auto* label             = OpcEnsureLabel(body, kOpcLabel);
  auto* label_native      = label ? label->transform() : nullptr;
  auto* backing_native    = backing ? backing->transform() : nullptr;
  auto* label_transform   = label_native ? label_native->As<unity::RectTransform>() : nullptr;
  auto* backing_transform = backing_native ? backing_native->As<unity::RectTransform>() : nullptr;
  if (!label_transform || !backing_transform) {
    OpcShow(body, kOpcLabel, false);
    OpcShow(body, kOpcBacking, false);
    return false;
  }
  const auto color = status.safe_on_node ? unity::Color{0.45f, 0.86f, 1, 1} : unity::Color{1, 0.69f, 0.13f, 1};
  if (!OpcPlace(backing_transform, {0.5f, 1}, {0.5f, 0}, {108, 30}, {0, selected ? 25.0f : 15.0f})
      || !OpcPlace(label_transform, {0.5f, 1}, {0.5f, 0}, {148, 36}, {0, selected ? 24.0f : 14.0f})
      || !label->TryCall<"set_color", void(unity::Color)>(color) || !OpcText(label, "<b>" + display + "</b>")) {
    OpcShow(body, kOpcLabel, false);
    OpcShow(body, kOpcBacking, false);
    return false;
  }
  backing->SetActive(true);
  label_transform->gameObject()->SetActive(true);
  return true;
}
bool OpcRenderCard(FleetLocalViewController* panel, const std::string& display)
{
  auto* timer = fleet_opc_binding::TimerAnchor(panel);
  if (!timer || display.empty()) {
    OpcShow(timer, kOpcCardLabel, false);
    return true;
  }
  auto* label  = OpcEnsureLabel(timer, kOpcCardLabel);
  auto* native = label ? label->transform() : nullptr;
  auto* rect   = native ? native->As<unity::RectTransform>() : nullptr;
  if (!rect) {
    OpcShow(timer, kOpcCardLabel, false);
    return false;
  }
  if (!OpcPlace(rect, {0.5f, 0.5f}, {0.5f, 0.5f}, {190, 26}, {7.6f, -39})
      || !label->TryCall<"set_color", void(unity::Color)>(unity::Color{0.82f, 0.91f, 0.96f, 1})
      || !OpcText(label, display)) {
    OpcShow(timer, kOpcCardLabel, false);
    return false;
  }
  rect->gameObject()->SetActive(true);
  return true;
}

void OpcRelease(il2cpp::Object* source)
{
  auto& state = Opc();
  auto  found = state.views.find(source);
  if (found == state.views.end())
    return;
  const auto owner = found->second.owner;
  auto       root  = il2cpp::GcHandle::Strong(found->second.source.Get());
  state.views.erase(found);
  if (!state.touched || !root || !unity::Alive(root.Get()))
    return;
  auto* body      = fleet_opc_binding::Body(root.Get<il2cpp::Object>());
  auto  body_root = il2cpp::GcHandle::Strong(body);
  // State and flag widgets share the tile body, but own different artwork.
  // Clearing one pooled widget must not remove the other enabled indicator.
  if (owner != OpcOwner::State)
    OpcShow(body, kOpcHighlight, false);
  if (owner != OpcOwner::Flag) {
    OpcShow(body, kOpcLabel, false);
    OpcShow(body, kOpcBacking, false);
  }
  if (owner == OpcOwner::Local)
    (void)OpcRenderCard(root.Get<il2cpp::Object>()->As<FleetLocalViewController>(), {});
}

void OpcRefresh(il2cpp::Object* key)
{
  auto&      state = Opc();
  const auto found = state.views.find(key);
  if (found == state.views.end())
    return;
  auto* observed_source = found->second.source.Get();
  if (!observed_source) {
    state.views.erase(found);
    return;
  }
  auto  source_root = il2cpp::GcHandle::Strong(observed_source);
  auto* source      = source_root.Get<il2cpp::Object>();
  if (!source)
    return;
  if (!unity::Alive(source)) {
    state.views.erase(found);
    return;
  }
  const auto generation        = found->second.generation;
  const bool local             = found->second.owner == OpcOwner::Local;
  const bool eta               = Config::Get().ui.fleet_hud_opc_eta;
  const bool highlight_enabled = Config::Get().ui.highlight_opc_fleets;
  if (!eta && !highlight_enabled && !state.touched)
    return;
  state.touched   = state.touched || eta || highlight_enabled;
  auto* body      = fleet_opc_binding::Body(source);
  auto  body_root = il2cpp::GcHandle::Strong(body);
  if (!eta && !highlight_enabled) {
    OpcHideBody(body);
    if (local)
      (void)OpcRenderCard(source->As<FleetLocalViewController>(), {});
    return;
  }
  const auto observed    = fleet_opc_binding::Fleet(source, local);
  auto       fleet_root  = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  auto*      fleet       = fleet_root.Get<FleetPlayerData>();
  const auto slot        = fleet ? fleet->TryCall<"get_Index", int32_t>() : il2cpp::CallResult<int32_t>{};
  const auto id          = fleet ? fleet->TryId() : il2cpp::CallResult<int64_t>{};
  const auto fleet_state = fleet ? fleet->TryCall<"get_CurrentState", FleetState>() : il2cpp::CallResult<FleetState>{};
  const bool active      = unity::ActiveAndEnabled(source);
  const bool usable      = active && slot && slot.value >= 0 && slot.value < 10 && id && id.value != 0 && fleet_state;
  auto*      bar         = fleet_opc_binding::Bar(source);
  auto       bar_root    = il2cpp::GcHandle::Strong(bar);
  auto*      panel       = bar ? bar->fleetPanelController() : nullptr;
  auto       panel_root  = il2cpp::GcHandle::Strong(panel);
  const auto selected =
      bar && slot ? bar->TryCall<"IsIndexSelected", bool(int32_t)>(slot.value) : il2cpp::CallResult<bool>{};
  const bool panel_component = panel == source;
  const bool selected_view   = panel_component || (selected && selected.value);
  if (!usable) {
    OpcHideBody(body);
    if (panel_component)
      (void)OpcRenderCard(panel, {});
    return;
  }
  if (!highlight_enabled)
    OpcShow(body, kOpcHighlight, false);
  if (!eta) {
    OpcShow(body, kOpcLabel, false);
    OpcShow(body, kOpcBacking, false);
    if (panel_component)
      (void)OpcRenderCard(panel, {});
  }
  if (!highlight_enabled && !eta)
    return;
  // Setup backoff must not keep artwork visible after its data ceases to apply.
  // These observations retain the shared cargo cache and frame cadence; only
  // allocation/configuration work below waits for the retry deadline.
  const auto has_ship = highlight_enabled ? fleet->TryCall<"get_HasShip", bool>() : il2cpp::CallResult<bool>{};
  const bool deployed = (static_cast<int32_t>(fleet_state.value) & static_cast<int32_t>(FleetState::Deployed)) != 0;
  const bool show     = highlight_enabled && body && has_ship && has_ship.value && deployed
                        && fleet_opc::Read(fleet, slot.value, id.value, static_cast<int32_t>(fleet_state.value)).opc;
  const auto status   = eta ? fleet_opc::Observe(fleet, slot.value, id.value, static_cast<int32_t>(fleet_state.value))
                            : fleet_opc::Status{};
  const auto display  = fleet_opc::Display(status);
  if (!show)
    OpcShow(body, kOpcHighlight, false);
  if (display.empty()) {
    OpcShow(body, kOpcLabel, false);
    OpcShow(body, kOpcBacking, false);
    if (selected_view)
      (void)OpcRenderCard(panel, {});
  }
  auto*      timer      = eta && selected_view ? fleet_opc_binding::TimerAnchor(panel) : nullptr;
  auto       timer_root = il2cpp::GcHandle::Strong(timer);
  const auto current    = state.views.find(key);
  if (current == state.views.end() || current->second.generation != generation)
    return;
  // Backoff belongs to this fleet and native anchor, not to a reused widget.
  const auto body_id  = reinterpret_cast<uintptr_t>(body);
  const auto timer_id = reinterpret_cast<uintptr_t>(timer);
  if (current->second.fleet != id.value || current->second.body != body_id || current->second.timer != timer_id) {
    current->second.fleet    = id.value;
    current->second.body     = body_id;
    current->second.timer    = timer_id;
    current->second.retry    = 0;
    current->second.failures = 0;
  }
  if (!show && display.empty()) {
    current->second.retry    = 0;
    current->second.failures = 0;
    return;
  }
  if (current->second.retry > OpcNow()) {
    // A failure in one surface must not keep another surface's previous
    // nonempty value visible when the current status has changed.
    if (!display.empty() && !OpcLabelMatches(body, kOpcLabel, "<b>" + display + "</b>")) {
      OpcShow(body, kOpcLabel, false);
      OpcShow(body, kOpcBacking, false);
    }
    if (selected_view && !display.empty() && !OpcLabelMatches(timer, kOpcCardLabel, fleet_opc::CardDisplay(status)))
      OpcShow(timer, kOpcCardLabel, false);
    return;
  }
  bool success = true;
  if (show) {
    auto* highlight = fleet_opc_binding::ChildObject(body, kOpcHighlight);
    if (show && !highlight)
      highlight = OpcCreateHighlight(body);
    if (highlight)
      highlight->SetActive(show);
    success = !show || highlight;
  }
  if (eta && !display.empty()) {
    success = OpcRenderEta(body, selected_view, status) && success;
    if (selected_view)
      success = OpcRenderCard(panel, fleet_opc::CardDisplay(status)) && success;
  }
  // Native calls may rebind/release a row. Do not publish bookkeeping through a
  // stale unordered_map iterator or apply the previous generation's backoff.
  const auto latest = state.views.find(key);
  if (latest == state.views.end() || latest->second.generation != generation)
    return;
  if (success) {
    latest->second.retry    = 0;
    latest->second.failures = 0;
  } else {
    latest->second.failures = std::min<uint8_t>(6, latest->second.failures + 1);
    latest->second.retry    = OpcNow() + std::min<int64_t>(30000, int64_t{1000} << (latest->second.failures - 1));
  }
}
void OpcBind(il2cpp::Object* source, OpcOwner owner)
{
  auto& state = Opc();
  if (!state.installed)
    return;
  std::erase_if(state.views, [](const auto& entry) { return !entry.second.source.Get(); });
  OpcRelease(source);
  auto weak = il2cpp::GcHandle::Weak(source);
  if (!weak)
    return;
  state.views.insert_or_assign(source, OpcView{std::move(weak), owner, ++state.generation});
  if (!state.updating && (Config::Get().ui.highlight_opc_fleets || Config::Get().ui.fleet_hud_opc_eta)) {
    state.updating = true;
    OpcRefresh(source);
    state.updating = false;
  }
}
void OpcFrame()
{
  auto& state = Opc();
  if (state.views.empty() || state.updating)
    return;
  const auto& ui = Config::Get().ui;
  const auto  enabled =
      uint8_t{static_cast<uint8_t>((ui.highlight_opc_fleets ? 1 : 0) | (ui.fleet_hud_opc_eta ? 2 : 0))};
  const bool changed = enabled != state.options_active;
  if (!enabled && !changed)
    return;
  const auto now = OpcNow();
  if (now < state.next && !changed)
    return;
  if (changed) {
    for (int32_t slot = 0; slot < 10; ++slot)
      fleet_opc::Invalidate(slot);
    for (auto& [_, view] : state.views) {
      view.retry    = 0;
      view.failures = 0;
    }
  }
  state.options_active = enabled;
  state.next           = now + 1000;
  std::vector<il2cpp::Object*> keys;
  keys.reserve(state.views.size());
  for (const auto& [key, view] : state.views)
    keys.push_back(key);
  state.updating = true;
  for (auto* key : keys)
    OpcRefresh(key);
  state.updating = false;
}

void FleetLocalViewController_BindDataContext_OpcHook(auto original, FleetLocalViewController* _this,
                                                      il2cpp::Object* provider, il2cpp::Object* context)
{
  original(_this, provider, context);
  OpcBind(_this, OpcOwner::Local);
}
void FleetLocalViewController_OnCurrentCargoReactiveEvent_OpcHook(auto original, FleetLocalViewController* _this,
                                                                  int32_t dirty_flags)
{
  original(_this, dirty_flags);
  if (!Config::Get().ui.highlight_opc_fleets && !Config::Get().ui.fleet_hud_opc_eta)
    return;
  const auto observed = fleet_opc_binding::Fleet(_this, true);
  auto       root     = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  const auto slot = root ? root.Get<FleetPlayerData>()->TryCall<"get_Index", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (slot)
    fleet_opc::Invalidate(slot.value);
  if (!Opc().updating) {
    Opc().updating = true;
    OpcRefresh(_this);
    Opc().updating = false;
  }
}
void FleetStateWidget_SetWidgetData_OpcHook(auto original, fleet_opc_binding::StateWidget* _this)
{
  original(_this);
  OpcBind(_this, OpcOwner::State);
}
void FleetStateWidget_ClearWidgetData_OpcHook(auto original, fleet_opc_binding::StateWidget* _this)
{
  OpcRelease(_this);
  original(_this);
}
void FleetbarFlagWidget_SetWidgetData_OpcHook(auto original, fleet_opc_binding::FlagWidget* _this)
{
  original(_this);
  OpcBind(_this, OpcOwner::Flag);
}
void FleetbarFlagWidget_ClearWidgetData_OpcHook(auto original, fleet_opc_binding::FlagWidget* _this)
{
  OpcRelease(_this);
  original(_this);
}
bool InstallOpcIndicators()
{
  const std::array entries{
      il2cpp::CheckedEntry(
          FleetLocalViewController::Class::Method<void(il2cpp::Object*, il2cpp::Object*)>("BindDataContext")),
      il2cpp::CheckedEntry(FleetLocalViewController::Class::Method<void(int32_t)>("OnCurrentCargoReactiveEvent")),
      il2cpp::CheckedEntry(fleet_opc_binding::StateWidget::Class::Method<void()>("SetWidgetData")),
      il2cpp::CheckedEntry(fleet_opc_binding::StateWidget::Class::Method<void()>("ClearWidgetData")),
      il2cpp::CheckedEntry(fleet_opc_binding::FlagWidget::Class::Method<void()>("SetWidgetData")),
      il2cpp::CheckedEntry(fleet_opc_binding::FlagWidget::Class::Method<void()>("ClearWidgetData"))};
  for (size_t index = 0; index < entries.size(); ++index)
    for (size_t previous = 0; previous < index; ++previous)
      if (entries[index] && entries[index] == entries[previous]) {
        spdlog::warn("[OpcIndicators] incompatible callbacks share a native entry; indicators unavailable");
        return false;
      }
  il2cpp::HookSet hooks("OpcIndicators");
  IL2CPP_STAGE_AT(hooks, entries[0], FleetLocalViewController_BindDataContext_OpcHook);
  IL2CPP_STAGE_AT(hooks, entries[1], FleetLocalViewController_OnCurrentCargoReactiveEvent_OpcHook);
  IL2CPP_STAGE_AT(hooks, entries[2], FleetStateWidget_SetWidgetData_OpcHook);
  IL2CPP_STAGE_AT(hooks, entries[3], FleetStateWidget_ClearWidgetData_OpcHook);
  IL2CPP_STAGE_AT(hooks, entries[4], FleetbarFlagWidget_SetWidgetData_OpcHook);
  IL2CPP_STAGE_AT(hooks, entries[5], FleetbarFlagWidget_ClearWidgetData_OpcHook);
  Opc().installed = hooks.Commit();
  return Opc().installed;
}
} // namespace

const Feature kOpcIndicators{"OpcIndicators", &Config::Patches::opcindicatorhooks, InstallOpcIndicators, OpcFrame};
