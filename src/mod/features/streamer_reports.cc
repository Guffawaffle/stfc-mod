// Streamer Mode covers private report and banner display fields without
// changing the game's result, report exports or notifications data.
#include "game/streamer_reports.h"
#include "features/features.h"
#include "features/notifications.h"
#include "features/streamer_mode.h"
#include "features/streamer_surface_cover.h"
#include "frame.h"

#include <il2cpp/hook.h>

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <utility>

namespace
{
struct ReportMaskPolicy {
  bool identity = false, location = false, notification = false;
  bool operator==(const ReportMaskPolicy&) const = default;
};

ReportMaskPolicy CurrentReportMask()
{
  const bool reports = streamer_mode::HideReports();
  return {reports || streamer_mode::HideIdentity(), reports || streamer_mode::HideLocations(), reports};
}

struct ReportMaskTargets {
  std::map<unity::GameObject*, il2cpp::GcHandle> objects;
  bool                                           verified = true;

  void Add(unity::Component* component)
  {
    if (!component)
      return; // Some report variants omit serialized fields.
    auto       root  = il2cpp::GcHandle::Strong(component);
    const auto alive = root ? unity::TryAlive(root.Get()) : il2cpp::CallResult<bool>{};
    if (!alive) {
      verified = false;
      return;
    }
    if (!alive.value)
      return;
    const auto game_object = root.Get<unity::Component>()->TryCall<"get_gameObject", unity::GameObject*>();
    if (!game_object || !game_object.value) {
      verified = false;
      return;
    }
    if (!objects.contains(game_object.value)) {
      auto game_root = il2cpp::GcHandle::Strong(game_object.value);
      if (!game_root) {
        verified = false;
        return;
      }
      objects.emplace(game_object.value, std::move(game_root));
    }
  }
};

using VisitReportTargets = void (*)(il2cpp::Object*, const ReportMaskPolicy&, ReportMaskTargets&);

struct ReportMaskView {
  il2cpp::GcHandle                                          owner;
  VisitReportTargets                                        visit{};
  std::map<unity::GameObject*, streamer_mode::SurfaceCover> covers;
  bool                                                      applying = false;
  bool                                                      verified = true;
};

struct StreamerReportState {
  std::map<il2cpp::Object*, ReportMaskView> views;
  ReportMaskPolicy                          last{};
  bool                                      installed        = false;
  bool                                      admission_failed = false;
};

StreamerReportState& ReportState()
{
  static auto* state = new StreamerReportState;
  return *state;
}

void ApplyReportMask(ReportMaskView& view, const ReportMaskPolicy& policy)
{
  if (view.applying)
    return;
  if (policy == ReportMaskPolicy{} && view.covers.empty()) {
    view.verified = true;
    return;
  }
  view.applying = true;
  struct ReportMaskCompletion {
    bool& applying;
    ~ReportMaskCompletion()
    { applying = false; }
  } completion{view.applying};
  auto*             observed = view.owner.Get();
  auto              owner    = il2cpp::GcHandle::Strong(observed);
  ReportMaskTargets selected;
  if (policy != ReportMaskPolicy{} && observed) {
    const auto alive = owner ? unity::TryAlive(owner.Get()) : il2cpp::CallResult<bool>{};
    if (!alive) {
      view.verified = false;
      return;
    }
    if (alive.value)
      view.visit(owner.Get<il2cpp::Object>(), policy, selected);
  }
  bool verified = selected.verified;
  for (const auto& [game_object, root] : selected.objects)
    verified &= view.covers[game_object].Set(root.Get<unity::GameObject>(), true);
  std::erase_if(view.covers, [&](auto& entry) {
    if (!selected.verified || selected.objects.contains(entry.first))
      return false;
    const auto restored = entry.second.Restore();
    verified &= restored;
    return restored;
  });
  view.verified = verified;
}

void PruneReportMasks()
{
  auto& views = ReportState().views;
  for (auto entry = views.begin(); entry != views.end();) {
    if (entry->second.applying) {
      ++entry;
      continue;
    }
    if (auto* observed = entry->second.owner.Get()) {
      auto       root  = il2cpp::GcHandle::Strong(observed);
      const auto alive = root ? unity::TryAlive(root.Get()) : il2cpp::CallResult<bool>{};
      if (!alive || alive.value) {
        ++entry;
        continue;
      }
    }
    ApplyReportMask(entry->second, {});
    if (entry->second.covers.empty())
      entry = views.erase(entry);
    else
      ++entry;
  }
}

void ObserveReportView(il2cpp::Object* owner, VisitReportTargets visitor)
{
  auto& views = ReportState().views;
  auto  found = views.find(owner);
  if (found == views.end() || found->second.owner.Get() != owner) {
    PruneReportMasks();
    auto weak = il2cpp::GcHandle::Weak(owner);
    if (!weak) {
      ReportState().admission_failed = true;
      return;
    }
    found               = views.try_emplace(owner).first;
    found->second.owner = std::move(weak);
    found->second.visit = visitor;
  }
  ApplyReportMask(found->second, CurrentReportMask());
}

void RestoreReportView(il2cpp::Object* owner)
{
  auto&      views = ReportState().views;
  const auto found = views.find(owner);
  if (found == views.end() || found->second.owner.Get() != owner || found->second.applying)
    return;
  ApplyReportMask(found->second, {});
  if (found->second.covers.empty())
    views.erase(found);
  else
    found->second.owner.Reset();
}

template <typename Owner, typename Fields, bool ReportMaskPolicy::* Facet>
void VisitSelectedReportFields(il2cpp::Object* object, const ReportMaskPolicy& policy, ReportMaskTargets& targets)
{
  if (policy.*Facet)
    Fields::Visit(static_cast<Owner*>(object), [&](auto* field) { targets.Add(field); });
}

void VisitDestructionReportTargets(il2cpp::Object* object, const ReportMaskPolicy& policy, ReportMaskTargets& targets)
{
  VisitSelectedReportFields<StreamerDestructionReportRow, StreamerDestructionReportRow::Identity,
                            &ReportMaskPolicy::identity>(object, policy, targets);
  VisitSelectedReportFields<StreamerDestructionReportRow, StreamerDestructionReportRow::Location,
                            &ReportMaskPolicy::location>(object, policy, targets);
}

void VisitBattleReportTargets(il2cpp::Object* object, const ReportMaskPolicy& policy, ReportMaskTargets& targets)
{
  auto*      report = static_cast<StreamerBattleReport*>(object);
  const auto add    = [&](auto* field) { targets.Add(field); };
  if (policy.location) {
    auto* system = report->SystemInformation();
    auto  root   = il2cpp::GcHandle::Strong(system);
    targets.verified &= !system || static_cast<bool>(root);
    StreamerReportSystem::Location::Visit(root.Get<StreamerReportSystem>(), add);
  }
  if (policy.identity) {
    const auto single = [&](StreamerReportTarget* target) {
      auto root = il2cpp::GcHandle::Strong(target);
      targets.verified &= !target || static_cast<bool>(root);
      StreamerReportTarget::Identity::Visit(root.Get<StreamerReportTarget>(), add);
    };
    report->VisitTargets(single, [&](StreamerReportMultiTarget* target) {
      auto root = il2cpp::GcHandle::Strong(target);
      targets.verified &= !target || static_cast<bool>(root);
      if (root) {
        StreamerReportMultiTarget::Identity::Visit(root.Get<StreamerReportMultiTarget>(), add);
        single(root.Get<StreamerReportMultiTarget>()->SingleTarget());
      }
    });
  }
}

void VisitReportTargetRow(il2cpp::Object* object, const ReportMaskPolicy& policy, ReportMaskTargets& targets)
{
  auto* row = static_cast<StreamerReportTargetRow*>(object);
  if (policy.identity && row->IsReportRow())
    StreamerReportTargetRow::Identity::Visit(row, [&](auto* field) { targets.Add(field); });
}

void BattleReportRowWidget_OnDidBindContext_Hook(auto original, StreamerBattleReportRow* _this)
{
  original(_this);
  ObserveReportView(_this, VisitSelectedReportFields<StreamerBattleReportRow, StreamerBattleReportRow::Identity,
                                                     &ReportMaskPolicy::identity>);
}

void DestructionReportRowWidget_OnDidBindContext_Hook(auto original, StreamerDestructionReportRow* _this)
{
  original(_this);
  ObserveReportView(_this, VisitDestructionReportTargets);
}

void BattleReportViewController_AboutToShow_Hook(auto original, StreamerBattleReport* _this)
{
  original(_this);
  ObserveReportView(_this, VisitBattleReportTargets);
}

void BattleReportViewController_AboutToHide_Hook(auto original, StreamerBattleReport* _this)
{
  original(_this);
  RestoreReportView(_this);
}

void BattleReportViewController_UpdateComponentsContext_Hook(auto original, StreamerBattleReport* _this)
{
  original(_this);
  ObserveReportView(_this, VisitBattleReportTargets);
}

void BattleLogGenericEntryDetailWidget_OnDidBindContext_Hook(auto original, StreamerBattleLogEntry* _this)
{
  original(_this);
  ObserveReportView(
      _this,
      VisitSelectedReportFields<StreamerBattleLogEntry, StreamerBattleLogEntry::Identity, &ReportMaskPolicy::identity>);
}

void TargetRowWidget_OnDidBindContext_Hook(auto original, StreamerReportTargetRow* _this)
{
  original(_this);
  if (_this->IsReportRow())
    ObserveReportView(_this, VisitReportTargetRow);
  else
    RestoreReportView(_this);
}

void BattleResultToastWidget_OnDidBindContext_Hook(auto original, StreamerBattleToast* _this)
{
  original(_this);
  ObserveReportView(
      _this,
      VisitSelectedReportFields<StreamerBattleToast, StreamerBattleToast::Details, &ReportMaskPolicy::notification>);
}

void IncomingAttackToastWidget_OnDidBindContext_Hook(auto original, StreamerIncomingToast* _this)
{
  original(_this);
  ObserveReportView(_this, VisitSelectedReportFields<StreamerIncomingToast, StreamerIncomingToast::Details,
                                                     &ReportMaskPolicy::notification>);
}

void ArmadaToastWidget_OnDidBindContext_Hook(auto original, StreamerArmadaToast* _this)
{
  original(_this);
  ObserveReportView(
      _this,
      VisitSelectedReportFields<StreamerArmadaToast, StreamerArmadaToast::Details, &ReportMaskPolicy::notification>);
}

void StandardToastWidget_OnDidBindContext_Hook(auto original, StreamerStandardToast* _this)
{
  original(_this);
  ObserveReportView(_this, VisitSelectedReportFields<StreamerStandardToast, StreamerStandardToast::Details,
                                                     &ReportMaskPolicy::notification>);
}

void StreamerReportsFrame()
{
  PublishDesktopNotificationPrivacy();
  auto&           state   = ReportState();
  const auto      policy  = CurrentReportMask();
  const bool      changed = std::exchange(state.last, policy) != policy;
  static Throttle retry(std::chrono::milliseconds(250));
  if (!changed && (!retry.Due() || !streamer_mode::ReportsDegraded()))
    return;
  PruneReportMasks();
  for (auto& [unused, view] : state.views)
    if (changed || !view.verified)
      ApplyReportMask(view, policy);
}

template <typename Binding> void* ReportEntry(const char* method_name)
{
  const auto method = Binding::Class::template Method<void()>(method_name);
  return method && method.Info()->klass == Binding::Class::Raw() ? il2cpp::CheckedEntry(method) : nullptr;
}

bool InstallStreamerReports()
{
  if (!unity::CanvasGroup::Available() || !StreamerBattleReportRow::Identity::Available()
      || !StreamerDestructionReportRow::Identity::Available() || !StreamerDestructionReportRow::Location::Available()
      || !StreamerReportSystem::Location::Available() || !StreamerBattleReport::TargetsAvailable()
      || !StreamerReportTarget::Identity::Available() || !StreamerReportMultiTarget::Identity::Available()
      || !StreamerBattleLogEntry::Identity::Available() || !StreamerReportTargetRow::Identity::Available()
      || !StreamerReportTargetRow::OwnerAvailable() || !StreamerBattleToast::Details::Available()
      || !StreamerIncomingToast::Details::Available() || !StreamerArmadaToast::Details::Available()
      || !StreamerStandardToast::Details::Available())
    return false;

  const auto row            = ReportEntry<StreamerBattleReportRow>("OnDidBindContext");
  const auto destruction    = ReportEntry<StreamerDestructionReportRow>("OnDidBindContext");
  const auto show           = ReportEntry<StreamerBattleReport>("AboutToShow");
  const auto hide           = ReportEntry<StreamerBattleReport>("AboutToHide");
  const auto update         = ReportEntry<StreamerBattleReport>("UpdateComponentsContext");
  const auto entry          = ReportEntry<StreamerBattleLogEntry>("OnDidBindContext");
  const auto participant    = ReportEntry<StreamerReportTargetRow>("OnDidBindContext");
  const auto battle_toast   = ReportEntry<StreamerBattleToast>("OnDidBindContext");
  const auto incoming_toast = ReportEntry<StreamerIncomingToast>("OnDidBindContext");
  const auto armada_toast   = ReportEntry<StreamerArmadaToast>("OnDidBindContext");
  const auto standard_toast = ReportEntry<StreamerStandardToast>("OnDidBindContext");
  std::array entries{row,         destruction,  show,           hide,         update,        entry,
                     participant, battle_toast, incoming_toast, armada_toast, standard_toast};
  std::ranges::sort(entries, std::less<void*>{});
  if (std::ranges::contains(entries, nullptr) || std::ranges::adjacent_find(entries) != entries.end())
    return false;

  il2cpp::HookSet hooks("StreamerReports");
  IL2CPP_STAGE_AT(hooks, row, BattleReportRowWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, destruction, DestructionReportRowWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, show, BattleReportViewController_AboutToShow_Hook);
  IL2CPP_STAGE_AT(hooks, hide, BattleReportViewController_AboutToHide_Hook);
  IL2CPP_STAGE_AT(hooks, update, BattleReportViewController_UpdateComponentsContext_Hook);
  IL2CPP_STAGE_AT(hooks, entry, BattleLogGenericEntryDetailWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, participant, TargetRowWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, battle_toast, BattleResultToastWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, incoming_toast, IncomingAttackToastWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, armada_toast, ArmadaToastWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, standard_toast, StandardToastWidget_OnDidBindContext_Hook);
  return ReportState().installed = hooks.Commit();
}
} // namespace

bool streamer_mode::ReportsAvailable()
{ return ReportState().installed && FrameDeliveryAvailable(); }

bool streamer_mode::ReportsDegraded()
{
  const auto& state = ReportState();
  return state.admission_failed
         || std::ranges::any_of(state.views, [](const auto& entry) { return !entry.second.verified; });
}

const Feature kStreamerReports{"StreamerReports", &Config::Patches::streamermodehooks, InstallStreamerReports,
                               StreamerReportsFrame};
