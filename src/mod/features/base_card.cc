// [ui] keep_base_card_open retains the currently viewed enemy station's card
// across matching course responses and recalls; commands remain entirely native.
#include "features/base_card.h"
#include "config.h"
#include "features/features.h"

#include "game/base_card.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>
#include <unity/tracker.h>

#include <atomic>

namespace
{
std::atomic_bool base_card_ready = false;

bool BaseCardEnabled()
{ return base_card_ready.load() && Config::Get().ui.keep_base_card_open; }

void PreScanTargetWidget_OnSetCourseResponse_BaseCardHook(auto original, PreScanTargetWidget* _this,
                                                          SetCourseResponse response)
{
  try {
    if (BaseCardEnabled() && base_card_binding::RetainPreview(_this, response))
      return;
  } catch (...) {
    // Optional viewer retention must never prevent the game's fallback path.
  }
  original(_this, response);
}

void NavigationInteractionUIViewController_OnSetCourseResponse_BaseCardHook(
    auto original, NavigationInteractionUIViewController* _this, SetCourseResponse response)
{
  try {
    if (BaseCardEnabled() && base_card_binding::RetainNavigation(_this, response))
      return;
  } catch (...) {
  }
  original(_this, response);
}

bool InstallBaseCard()
{
#if (defined(_WIN32) && defined(_M_X64)) || (defined(__APPLE__) && (defined(__arm64__) || defined(__x86_64__)))
  const auto& native = base_card_binding::Native();
  if (!native.ready) {
    spdlog::warn("[BaseCard] station-card contract unavailable (preview-layout={} navigation-layout={} data={} poi={} "
                 "user={} preview-context={} navigation-context={} scanning={} cache={}); native dismissal retained",
                 base_card_binding::ResponseLayout(native.preview),
                 base_card_binding::ResponseLayout(native.navigation), native.preview_data != nullptr,
                 native.poi != nullptr, native.user != nullptr, native.preview_context != nullptr,
                 native.navigation_context != nullptr, native.scanning_instance != nullptr,
                 native.target_cache != nullptr);
    return false;
  }
  il2cpp::HookSet hooks("BaseCard");
  IL2CPP_STAGE(hooks, native.preview, PreScanTargetWidget_OnSetCourseResponse_BaseCardHook);
  IL2CPP_STAGE(hooks, native.navigation, NavigationInteractionUIViewController_OnSetCourseResponse_BaseCardHook);
  base_card_ready.store(hooks.Commit());
  return base_card_ready.load();
#else
  return false;
#endif
}
} // namespace

bool base_card::Available()
{ return base_card_ready.load(); }

bool base_card::KeepOpenForRecall(std::int64_t fleet)
{
  if (!BaseCardEnabled() || fleet == 0)
    return false;
  const SetCourseResponse response{fleet, true, true, nullptr};
  try {
    for (auto* widget : unity::Instances<PreScanTargetWidget>()) {
      auto       source     = il2cpp::GcHandle::Strong(widget);
      auto*      visible    = source ? source.Get<PreScanTargetWidget>()->visibilityController() : nullptr;
      auto       visibility = il2cpp::GcHandle::Strong(visible);
      const auto state =
          visible ? visible->TryCall<"get_VisibilityState", VisibilityState>() : il2cpp::CallResult<VisibilityState>{};
      if (visibility && state && (state.value == VisibilityState::Show || state.value == VisibilityState::Visible)
          && base_card_binding::RetainPreview(source.Get<PreScanTargetWidget>(), response))
        return true;
    }
  } catch (...) {
  }
  return false;
}

const Feature kBaseCard{"BaseCard", &Config::Patches::basecardhooks, InstallBaseCard};
