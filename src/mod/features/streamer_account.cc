// Streamer Mode: protect the standalone native Player ID display.
#include "features/features.h"
#include "features/streamer_mode.h"
#include "features/streamer_surface_cover.h"
#include "frame.h"
#include "game/settings.h"

#include <algorithm>
#include <array>
#include <il2cpp/hook.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

namespace
{
struct StreamerIdView {
  il2cpp::GcHandle             owner;
  streamer_mode::SurfaceCovers covers;
  bool                         complete = true;
};

struct StreamerAccountViews {
  std::vector<std::shared_ptr<StreamerIdView>> rows;
  Throttle                                     review{std::chrono::milliseconds(250)};
  bool                                         hidden    = false;
  bool                                         reported  = false;
  bool                                         untracked = false;
  bool                                         pending   = false;
  bool                                         installed = false;
};

StreamerAccountViews& StreamerIdViews()
{
  static auto* views = new StreamerAccountViews;
  return *views;
}

il2cpp::CallResult<unity::GameObject*> ObserveStreamerIdLabel(PlayerIdDisplay* owner)
{
  auto* label = owner->Label();
  if (!label)
    return {true, nullptr};
  auto root = il2cpp::GcHandle::Strong(label);
  if (!root)
    return {};
  const auto label_alive = unity::TryAlive(root.Get());
  if (!label_alive || !label_alive.value)
    return {label_alive.succeeded, nullptr};
  const auto object = root.Get<TextLocalizer>()->TryCall<"get_gameObject", unity::GameObject*>();
  if (!object || !object.value)
    return {};
  const auto alive = unity::TryAlive(object.value);
  return {alive.succeeded, alive.value ? object.value : nullptr};
}

bool ApplyStreamerIdView(StreamerIdView& row, bool hidden)
{
  auto* observed_owner = row.owner.Get();
  if (!hidden || !observed_owner)
    return row.covers.Apply();
  auto       owner = il2cpp::GcHandle::Strong(observed_owner);
  const auto alive = owner ? unity::TryAlive(owner.Get()) : il2cpp::CallResult<bool>{};
  if (!alive)
    return false;
  if (!alive.value) {
    row.owner.Reset();
    return row.covers.Apply();
  }

  const auto target = ObserveStreamerIdLabel(owner.Get<PlayerIdDisplay>());
  return target && row.covers.Apply(std::array{target.value});
}

void ReportStreamerAccountCoverage(StreamerAccountViews& views)
{
  views.pending = std::ranges::any_of(views.rows, [](const auto& row) { return !row->complete; });
  if ((views.pending || (views.hidden && views.untracked)) && !std::exchange(views.reported, true))
    spdlog::warn("[StreamerAccountPrivacy] could not verify the Player ID display mask");
}

void UpdateStreamerIdView(PlayerIdDisplay* owner)
{
  auto& views = StreamerIdViews();
  const auto found = std::ranges::find_if(views.rows, [owner](const auto& row) { return row->owner.Get() == owner; });
  const auto row   = found == views.rows.end() ? std::make_shared<StreamerIdView>() : *found;
  if (found == views.rows.end()) {
    row->owner = il2cpp::GcHandle::Weak(owner);
    if (!row->owner) {
      views.untracked = true;
      ReportStreamerAccountCoverage(views);
      return;
    }
    views.rows.push_back(row);
  }
  row->complete = ApplyStreamerIdView(*row, streamer_mode::HideAccount());
  ReportStreamerAccountCoverage(views);
}

void ReleaseStreamerIdView(PlayerIdDisplay* owner)
{
  auto&      views = StreamerIdViews();
  const auto found = std::ranges::find_if(views.rows, [owner](const auto& row) { return row->owner.Get() == owner; });
  if (found == views.rows.end())
    return;
  const auto row = *found;
  row->owner.Reset();
  if (row->covers.Apply()) {
    std::erase(views.rows, row);
  } else {
    row->complete = false;
  }
  ReportStreamerAccountCoverage(views);
}

void PlayerIdDisplay_SetPlayerIdToText_Hook(auto original, PlayerIdDisplay* _this, bool enabled)
{
  UpdateStreamerIdView(_this);
  original(_this, enabled);
}

void PlayerIdDisplay_OnDisable_Hook(auto original, PlayerIdDisplay* _this)
{
  original(_this);
  ReleaseStreamerIdView(_this);
}

void StreamerAccountPrivacyFrame()
{
  auto&      views   = StreamerIdViews();
  const bool hidden  = streamer_mode::HideAccount();
  const bool changed = std::exchange(views.hidden, hidden) != hidden;
  if (!changed && (views.rows.empty() || !views.review.Due()))
    return;
  if (!hidden)
    views.untracked = false;
  const auto rows = views.rows;
  for (const auto& row : rows)
    if (changed || !row->complete || !row->owner.Get())
      row->complete = ApplyStreamerIdView(*row, hidden);
  std::erase_if(views.rows, [](const auto& row) { return !row->owner.Get() && row->complete; });
  ReportStreamerAccountCoverage(views);
}

bool InstallStreamerAccountPrivacy()
{
  using Class = PlayerIdDisplay::Class;
  if (!unity::CanvasGroup::AlphaAvailable() || !Class::HasField<"_textLocalizer", TextLocalizer*>())
    return false;
  const auto set_method     = Class::Method<void(bool)>("SetPlayerIdToText");
  const auto disable_method = Class::Method<void()>("OnDisable");
  if (!set_method || !disable_method || set_method.Info()->klass != Class::Raw()
      || disable_method.Info()->klass != Class::Raw())
    return false;
  auto* set     = il2cpp::CheckedEntry(set_method);
  auto* disable = il2cpp::CheckedEntry(disable_method);
  if (!set || !disable || set == disable)
    return false;
  il2cpp::HookSet hooks("StreamerAccountPrivacy");
  IL2CPP_STAGE_AT(hooks, set, PlayerIdDisplay_SetPlayerIdToText_Hook);
  IL2CPP_STAGE_AT(hooks, disable, PlayerIdDisplay_OnDisable_Hook);
  StreamerIdViews().installed = hooks.Commit();
  return StreamerIdViews().installed;
}
} // namespace

bool streamer_mode::StandaloneIdAvailable()
{ return StreamerIdViews().installed && FrameDeliveryAvailable(); }

bool streamer_mode::StandaloneIdDegraded()
{
  const auto& views = StreamerIdViews();
  return StandaloneIdAvailable() && HideAccount() && (views.pending || views.untracked);
}

const Feature kStreamerAccountPrivacy{"StreamerAccountPrivacy", &Config::Patches::streamermodehooks,
                                      InstallStreamerAccountPrivacy, StreamerAccountPrivacyFrame};
