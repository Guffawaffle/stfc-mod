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

struct OpcView {
  il2cpp::GcHandle source;
  bool             local      = false;
  uint64_t         generation = 0;
  int64_t          retry      = 0;
  uint8_t          failures   = 0;
};
struct OpcUi {
  bool                                         installed      = false;
  bool                                         updating       = false;
  bool                                         options_active = false;
  bool                                         touched        = false;
  uint64_t                                     generation     = 0;
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
  auto  sprite_root = il2cpp::GcHandle::Strong(sprite);
  auto* copy        = unity::Instantiate(source, body);
  auto  root        = il2cpp::GcHandle::Strong(copy);
  if (!copy || !root)
    return nullptr;
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
  outer->SetColor({1.0f, 0.66f, 0.12f, 1.0f});
  outer->SetRaycastTarget(false);
  inner->SetSprite(sprite_root.Get<unity::Sprite>());
  inner->SetType(unity::Image::Type::Filled);
  (void)inner->TryCall<"set_fillAmount", void(float)>(1.0f);
  inner->SetColor({0.78f, 0.40f, 0.06f, 0.62f});
  inner->SetRaycastTarget(false);
  inner_object->SetActive(true);
  const auto source_index = source->transform()->TryCall<"GetSiblingIndex", int32_t>();
  if (source_index)
    (void)transform->TryCall<"SetSiblingIndex", void(int32_t)>(source_index.value);
  fleet_opc_binding::Name(copy, kOpcHighlight);
  return copy;
}

unity::TextMeshProUGUI* OpcLabel(unity::Transform* parent, std::string_view name)
{
  auto* object = fleet_opc_binding::ChildObject(parent, name);
  return object ? object->GetComponent<unity::TextMeshProUGUI>() : nullptr;
}
unity::TextMeshProUGUI* OpcEnsureLabel(unity::Transform* parent, std::string_view name)
{
  if (!parent)
    return nullptr;
  if (auto* label = OpcLabel(parent, name))
    return label;
  // Preserve the game's current font when a native label is available locally.
  auto*            parent_object = parent->gameObject();
  auto*            donor = parent_object ? parent_object->GetComponentInChildren<unity::TextMeshProUGUI>() : nullptr;
  auto             font  = il2cpp::GcHandle::Strong(donor ? donor->font() : nullptr);
  il2cpp::GcHandle root;
  auto*            rect   = unity::CreateUiObject(name, parent, root);
  auto*            object = rect ? rect->gameObject() : nullptr;
  auto*            label  = object ? object->AddComponent<unity::TextMeshProUGUI>() : nullptr;
  if (!label) {
    if (object)
      unity::Destroy(object);
    return nullptr;
  }
  if (font)
    label->SetFont(font.Get<il2cpp::Object>());
  label->SetFontSize(18.0f);
  label->SetAlignment(unity::TMP_Text::Alignment::Center);
  label->SetRaycastTarget(false);
  label->SetEnableWordWrapping(false);
  label->SetEnableAutoSizing(false);
  label->SetRichText(true);
  if (auto* layout = object->AddComponent<unity::LayoutElement>())
    (void)layout->ExcludeFromLayout();
  return label;
}
unity::GameObject* OpcEnsureBacking(unity::Transform* parent)
{
  if (auto* existing = fleet_opc_binding::ChildObject(parent, kOpcBacking))
    return existing;
  il2cpp::GcHandle root;
  auto*            rect   = unity::CreateUiObject(kOpcBacking, parent, root);
  auto*            object = rect ? rect->gameObject() : nullptr;
  auto*            image  = object ? object->AddComponent<unity::Image>() : nullptr;
  if (!image) {
    if (object)
      unity::Destroy(object);
    return nullptr;
  }
  image->SetColor({0.035f, 0.075f, 0.10f, 0.86f});
  image->SetRaycastTarget(false);
  if (auto* layout = object->AddComponent<unity::LayoutElement>())
    (void)layout->ExcludeFromLayout();
  return object;
}
void OpcText(unity::TextMeshProUGUI* label, const std::string& text)
{
  const auto old = label->TryCall<"get_text", Il2CppString*>();
  if (!old || !il2cpp::Equals(old.value, text))
    label->SetText(text);
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
  auto* label_transform   = label ? label->transform()->As<unity::RectTransform>() : nullptr;
  auto* backing_transform = backing ? backing->transform()->As<unity::RectTransform>() : nullptr;
  if (!label_transform || !backing_transform)
    return false;
  backing_transform->Place({0.5f, 1}, {0.5f, 1}, {0.5f, 0}, {108, 30}, {0, selected ? 25.0f : 15.0f});
  label_transform->Place({0.5f, 1}, {0.5f, 1}, {0.5f, 0}, {148, 36}, {0, selected ? 24.0f : 14.0f});
  (void)backing_transform->TryCall<"SetAsLastSibling", void>();
  (void)label_transform->TryCall<"SetAsLastSibling", void>();
  label->SetColor(status.safe_on_node ? unity::Color{0.45f, 0.86f, 1, 1} : unity::Color{1, 0.69f, 0.13f, 1});
  OpcText(label, "<b>" + display + "</b>");
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
  auto* label = OpcEnsureLabel(timer, kOpcCardLabel);
  auto* rect  = label ? label->transform()->As<unity::RectTransform>() : nullptr;
  if (!rect)
    return false;
  rect->Place({0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {190, 26}, {7.6f, -39});
  (void)rect->TryCall<"SetAsLastSibling", void>();
  label->SetColor({0.82f, 0.91f, 0.96f, 1});
  OpcText(label, display);
  rect->gameObject()->SetActive(true);
  return true;
}

void OpcRelease(il2cpp::Object* source)
{
  auto& state = Opc();
  auto  found = state.views.find(source);
  if (found == state.views.end())
    return;
  auto root = il2cpp::GcHandle::Strong(found->second.source.Get());
  state.views.erase(found);
  if (!state.touched || !root || !unity::Alive(root.Get()))
    return;
  OpcHideBody(fleet_opc_binding::Body(root.Get<il2cpp::Object>()));
}

void OpcRefresh(il2cpp::Object* key)
{
  auto&      state = Opc();
  const auto found = state.views.find(key);
  if (found == state.views.end())
    return;
  auto  source_root = il2cpp::GcHandle::Strong(found->second.source.Get());
  auto* source      = source_root.Get<il2cpp::Object>();
  if (!source || !unity::Alive(source)) {
    state.views.erase(found);
    return;
  }
  const auto generation        = found->second.generation;
  const bool local             = found->second.local;
  const auto retry             = found->second.retry;
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
  if (retry > OpcNow())
    return;
  bool success = true;
  if (highlight_enabled) {
    const auto has_ship  = fleet->TryCall<"get_HasShip", bool>();
    const bool deployed  = (static_cast<int32_t>(fleet_state.value) & static_cast<int32_t>(FleetState::Deployed)) != 0;
    const bool show      = body && has_ship && has_ship.value && deployed
                           && fleet_opc::Read(fleet, slot.value, id.value, static_cast<int32_t>(fleet_state.value)).opc;
    auto*      highlight = fleet_opc_binding::ChildObject(body, kOpcHighlight);
    if (show && !highlight)
      highlight = OpcCreateHighlight(body);
    if (highlight)
      highlight->SetActive(show);
    success = !show || highlight;
  }
  if (eta) {
    const auto status = fleet_opc::Observe(fleet, slot.value, id.value, static_cast<int32_t>(fleet_state.value));
    success           = OpcRenderEta(body, panel_component || (selected && selected.value), status) && success;
    if (panel_component || (selected && selected.value))
      success =
          OpcRenderCard(panel, fleet_opc::Display(status).empty() ? std::string{} : fleet_opc::CardDisplay(status))
          && success;
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
void OpcBind(il2cpp::Object* source, bool local)
{
  auto& state = Opc();
  if (!state.installed)
    return;
  std::erase_if(state.views, [](const auto& entry) { return !entry.second.source.Get(); });
  OpcRelease(source);
  auto weak = il2cpp::GcHandle::Weak(source);
  if (!weak)
    return;
  state.views.insert_or_assign(source, OpcView{std::move(weak), local, ++state.generation});
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
  const bool enabled = Config::Get().ui.highlight_opc_fleets || Config::Get().ui.fleet_hud_opc_eta;
  const bool changed = enabled != state.options_active;
  if (!enabled && !changed)
    return;
  static int64_t next = 0;
  const auto     now  = OpcNow();
  if (now < next && !changed)
    return;
  state.options_active = enabled;
  next                 = now + 1000;
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
  OpcBind(_this, true);
}
void FleetLocalViewController_OnCurrentCargoReactiveEvent_OpcHook(auto original, FleetLocalViewController* _this,
                                                                  int32_t dirty_flags)
{
  original(_this, dirty_flags);
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
  OpcBind(_this, false);
}
void FleetStateWidget_ClearWidgetData_OpcHook(auto original, fleet_opc_binding::StateWidget* _this)
{
  OpcRelease(_this);
  original(_this);
}
void FleetbarFlagWidget_SetWidgetData_OpcHook(auto original, fleet_opc_binding::FlagWidget* _this)
{
  original(_this);
  OpcBind(_this, false);
}
void FleetbarFlagWidget_ClearWidgetData_OpcHook(auto original, fleet_opc_binding::FlagWidget* _this)
{
  OpcRelease(_this);
  original(_this);
}
bool InstallOpcIndicators()
{
  il2cpp::HookSet hooks("OpcIndicators");
  IL2CPP_STAGE(hooks, FleetLocalViewController::Class::Method("BindDataContext", 2),
               FleetLocalViewController_BindDataContext_OpcHook);
  IL2CPP_STAGE(hooks, FleetLocalViewController::Class::Method<void(int32_t)>("OnCurrentCargoReactiveEvent"),
               FleetLocalViewController_OnCurrentCargoReactiveEvent_OpcHook);
  IL2CPP_STAGE(hooks, fleet_opc_binding::StateWidget::Class::Method<void()>("SetWidgetData"),
               FleetStateWidget_SetWidgetData_OpcHook);
  IL2CPP_STAGE(hooks, fleet_opc_binding::StateWidget::Class::Method<void()>("ClearWidgetData"),
               FleetStateWidget_ClearWidgetData_OpcHook);
  IL2CPP_STAGE(hooks, fleet_opc_binding::FlagWidget::Class::Method<void()>("SetWidgetData"),
               FleetbarFlagWidget_SetWidgetData_OpcHook);
  IL2CPP_STAGE(hooks, fleet_opc_binding::FlagWidget::Class::Method<void()>("ClearWidgetData"),
               FleetbarFlagWidget_ClearWidgetData_OpcHook);
  Opc().installed = hooks.Commit();
  return Opc().installed;
}
} // namespace

const Feature kOpcIndicators{"OpcIndicators", &Config::Patches::opcindicatorhooks, InstallOpcIndicators, OpcFrame};
