// Alliance Games store: native material research tooltips and currency refresh
// after bundle readiness. One feature owner, no claim-trace or file probe.
#include "game/alliance_store.h"
#include "config.h"
#include "features/features.h"

#include <il2cpp/hook.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <utility>

namespace
{
namespace store_game = alliance_store_game;
using Clock          = std::chrono::steady_clock;

struct AllianceStoreWork {
  il2cpp::GcHandle  source;
  uint64_t          epoch   = 0;
  bool              pending = false, currency = false, refreshing_currency = false;
  Clock::time_point due{};
} alliance_store_work;

enum class ResearchUpdate { Unchanged, Upgraded, Restored, Unavailable, Failed };

bool SameStoreItem(StoreInventoryTooltip* tooltip, InventoryItem* item)
{
  const auto alive = unity::TryAlive(tooltip);
  if (!alive || !alive.value)
    return false;
  const auto current = tooltip->TryCall<"get_Context", InventoryItem*>();
  return current && current.value == item;
}

// Weak component identity survives pooled reuse without retaining a scene. Only
// the original serialized assets stay rooted while our replacement is bound.
struct OwnedStoreTooltip {
  il2cpp::GcHandle widget, plain, action, store;
  il2cpp::GcHandle search, locale, settings;
  bool             bound = false, plain_enabled = true, retiring = false;
  uint64_t         generation = 0;
};
std::vector<std::shared_ptr<OwnedStoreTooltip>> alliance_store_tooltips;

std::shared_ptr<OwnedStoreTooltip> OwnedTooltip(StoreInventoryTooltip* tooltip)
{
  for (auto& entry : alliance_store_tooltips)
    if (entry->widget.Get() == tooltip)
      return entry;
  return nullptr;
}

bool RestoreStoreTooltip(OwnedStoreTooltip& entry)
{
  if (!entry.bound)
    return true;
  if (entry.retiring)
    return false;
  ++entry.generation; // Any in-flight setup loses publication authority.
  entry.retiring = true;
  struct ClearRetiring {
    bool& value;
    ~ClearRetiring()
    { value = false; }
  } clear{entry.retiring};
  store_game::Roots roots;
  auto*             widget = roots.Keep(entry.widget.Get<StoreInventoryTooltip>());
  auto*             plain  = roots.Keep(entry.plain.Get<StoreTooltipTrigger>());
  auto*             action = roots.Keep(entry.action.Get<StoreTooltipAction>());
  const auto        alive  = unity::TryAlive(widget);
  if (!roots.healthy || !widget || !plain || !action || !alive)
    return false;
  if (!alive.value) {
    entry.bound = false;
    entry.search.Reset();
    entry.locale.Reset();
    entry.settings.Reset();
    return true;
  }
  auto& f = store_game::Members();
  if ((f.trigger.Read(widget) != plain && f.trigger.Read(widget) != action)
      || (f.action.Read(widget) && f.action.Read(widget) != action)) {
    // A new native owner replaced our arrangement. Disable only our component;
    // never overwrite the replacement owner's fields or enabled state.
    (void)store_game::Enabled(action, false);
    entry.bound = false;
    entry.search.Reset();
    entry.locale.Reset();
    entry.settings.Reset();
    return true;
  }
  bool ok = store_game::Enabled(action, false);
  ok &= f.trigger.Write(widget, plain);
  ok &= f.action.Write(widget, nullptr);
  ok &= f.search.Write(widget, entry.search.Get<StoreActionLocale>());
  ok &= f.locale.Write(widget, entry.locale.Get<StoreActionLocale>());
  ok &= f.settings.Write(widget, entry.settings.Get<StoreActionLocales>());
  ok &= widget->TryCall<"UpdateTooltipDescription", void()>().succeeded;
  ok &= store_game::Enabled(plain, entry.plain_enabled);
  if (ok) {
    entry.bound = false;
    entry.search.Reset();
    entry.locale.Reset();
    entry.settings.Reset();
  }
  return ok;
}

ResearchUpdate ReconcileStoreTooltip(StoreInventoryTooltip* tooltip, StoreInventoryTooltip*& donor,
                                     store_game::Roots& pass_roots)
{
  store_game::Roots roots;
  if (!roots.Keep(tooltip))
    return ResearchUpdate::Unavailable;
  const auto epoch = alliance_store_work.epoch;
  auto*      store = roots.Keep(alliance_store_work.source.Get<AllianceStoreScroller>());
  const auto alive = unity::TryAlive(tooltip);
  const auto observed =
      alive && alive.value ? tooltip->TryCall<"get_Context", InventoryItem*>() : il2cpp::CallResult<InventoryItem*>{};
  auto* item      = observed ? roots.Keep(observed.value) : nullptr;
  auto  same_item = [&] {
    if (!store || alliance_store_work.epoch != epoch || alliance_store_work.source.Get() != store)
      return false;
    const auto live_store = unity::TryAlive(store);
    return live_store && live_store.value && SameStoreItem(tooltip, item) && alliance_store_work.epoch == epoch
           && alliance_store_work.source.Get() == store;
  };
  auto* plain = item ? store_game::PlainComponent(tooltip, roots) : nullptr;
  if (!plain)
    return ResearchUpdate::Unchanged;
  auto& f           = store_game::Members();
  auto* old_action  = roots.Keep(f.action.Read(tooltip));
  auto* old_trigger = roots.Keep(f.trigger.Read(tooltip));
  auto  owned       = OwnedTooltip(tooltip);
  // Existing native action components and unrelated arrangements remain native.
  if (!roots.healthy || (old_action && (!owned || owned->action.Get() != old_action))
      || (old_action ? old_trigger != old_action : old_trigger != plain))
    return ResearchUpdate::Unchanged;
  const auto eligible = store_game::Eligible(item);
  if (!eligible || !same_item())
    return ResearchUpdate::Unavailable;
  if (!eligible.value)
    return owned && owned->bound ? (RestoreStoreTooltip(*owned) ? ResearchUpdate::Restored : ResearchUpdate::Failed)
                                 : ResearchUpdate::Unchanged;
  if (!donor)
    donor = store_game::Template(pass_roots);
  if (!donor)
    return ResearchUpdate::Unavailable;
  auto*      search        = roots.Keep(f.search.Read(donor));
  auto*      locale        = roots.Keep(f.locale.Read(donor));
  auto*      settings      = roots.Keep(f.settings.Read(donor));
  auto*      loader        = roots.Keep(f.loader.Read(plain));
  const auto plain_enabled = plain->TryCall<"get_enabled", bool>();
  if (!roots.healthy || !search || !loader || !plain_enabled || !same_item())
    return ResearchUpdate::Unavailable;
  auto* action = store_game::ActionComponent(tooltip, false, roots);
  // Reuse only the disabled component this feature previously added.
  if (action && (!owned || owned->action.Get() != action))
    return ResearchUpdate::Unchanged;
  if (!action)
    action = store_game::ActionComponent(tooltip, true, roots);
  if (!action)
    return ResearchUpdate::Unavailable;
  if (!owned) {
    auto entry    = std::make_shared<OwnedStoreTooltip>();
    entry->widget = il2cpp::GcHandle::Weak(tooltip);
    entry->plain  = il2cpp::GcHandle::Weak(plain);
    entry->action = il2cpp::GcHandle::Weak(action);
    if (!entry->widget || !entry->plain || !entry->action) {
      store_game::Enabled(action, false);
      return ResearchUpdate::Unavailable;
    }
    alliance_store_tooltips.push_back(std::move(entry));
    owned = alliance_store_tooltips.back();
  }
  const bool previously_bound = owned->bound;
  if (!owned->bound) {
    auto retain = []<typename T>(T* value, il2cpp::GcHandle& handle) {
      handle = il2cpp::GcHandle::Strong(value);
      return !value || (handle && handle.Get() == value);
    };
    if (!retain(f.search.Read(tooltip), owned->search) || !retain(f.locale.Read(tooltip), owned->locale)
        || !retain(f.settings.Read(tooltip), owned->settings)) {
      owned->search.Reset();
      owned->locale.Reset();
      owned->settings.Reset();
      store_game::Enabled(action, false);
      return ResearchUpdate::Unavailable;
    }
    owned->store = il2cpp::GcHandle::Weak(store);
    if (!owned->store || owned->store.Get() != store) {
      owned->search.Reset();
      owned->locale.Reset();
      owned->settings.Reset();
      store_game::Enabled(action, false);
      return ResearchUpdate::Unavailable;
    }
    owned->plain_enabled = plain_enabled.value;
    owned->bound         = true;
  }
  const auto generation = ++owned->generation;
  auto current = [&] { return same_item() && owned->bound && !owned->retiring && owned->generation == generation; };
  struct Rollback {
    std::shared_ptr<OwnedStoreTooltip> entry;
    bool                               committed = false;
    ~Rollback()
    {
      if (!committed)
        (void)RestoreStoreTooltip(*entry);
    }
  } rollback{owned};

  if (!store_game::Enabled(action, false) || !store_game::CopyAppearance(plain, action, roots) || !current()
      || !f.search.Write(tooltip, search) || !f.locale.Write(tooltip, locale) || !f.settings.Write(tooltip, settings)
      || !f.trigger.Write(tooltip, action) || !f.action.Write(tooltip, action)
      || !tooltip->TryCall<"UpdateTooltipDescription", void()>() || !current()
      || !tooltip->TryCall<"SetupActionTooltipTrigger", void()>() || !current() || !store_game::Enabled(plain, false)
      || !store_game::Enabled(action, true) || !current())
    return ResearchUpdate::Failed;
  const auto enabled = action->TryCall<"get_enabled", bool>();
  if (!current() || !enabled || !enabled.value || f.trigger.Read(tooltip) != action || f.action.Read(tooltip) != action)
    return ResearchUpdate::Failed;
  rollback.committed = true;
  return previously_bound ? ResearchUpdate::Unchanged : ResearchUpdate::Upgraded;
}

void QueueStore(AllianceStoreScroller* source, bool currency)
{
  if (!store_game::IsStore(source))
    return;
  auto& work = alliance_store_work;
  if (work.source.Get() != source) {
    work.source = il2cpp::GcHandle::Weak(source);
    ++work.epoch;
  }
  if (!work.source)
    return;
  if (!work.pending)
    work.due = Clock::now() + std::chrono::milliseconds(250);
  work.pending = true;
  work.currency |= currency && !work.refreshing_currency;
}

void AllianceStoreBound_Hook(auto original, AllianceStoreScroller* self)
{
  original(self);
  std::erase_if(alliance_store_tooltips, [](auto& entry) { return !entry->widget.Get(); });
  QueueStore(self, true);
}

void AllianceStoreReleased_Hook(auto original, AllianceStoreScroller* self)
{
  auto& work = alliance_store_work;
  if (work.source.Get() == self) {
    work.source.Reset();
    work.pending = work.currency = false;
    ++work.epoch;
  }
  // Native restoration can reenter bind/pruning. Keep stable entry custody and
  // invalidate the source before the first such callback.
  const auto entries = alliance_store_tooltips;
  for (auto& entry : entries)
    if (entry->store.Get() == self)
      (void)RestoreStoreTooltip(*entry);
  original(self);
}

void StoreRegenerated_Hook(auto original, ShopListScrollerViewController* self, bool instant)
{
  original(self, instant);
  if (store_game::IsStore(self) && !alliance_store_work.refreshing_currency)
    QueueStore(reinterpret_cast<AllianceStoreScroller*>(self), true);
}

void StoreBundleReleased_Hook(auto original, BundleDataWidget* self)
{
  auto* tooltip = store_game::Members().bundle_tooltip.Read(self);
  if (auto entry = OwnedTooltip(tooltip))
    (void)RestoreStoreTooltip(*entry);
  original(self);
}

void StoreBundleRefreshed_Hook(auto original, BundleDataWidget* self, Bundle* bundle)
{
  original(self, bundle);
  // No scene enumeration when ordinary shop cards refresh. A weak store owner
  // is established only by the Alliance Games lifecycle/regeneration hooks.
  store_game::Roots roots;
  auto*             source = roots.Keep(alliance_store_work.source.Get<AllianceStoreScroller>());
  const auto        live   = unity::TryAlive(source);
  if (!source || !live || !live.value)
    return;
  const auto parent    = source->TryCall<"get_transform", unity::Transform*>();
  const auto child     = self->TryCall<"get_transform", unity::Transform*>();
  auto*      ancestor  = parent ? roots.Keep(parent.value) : nullptr;
  auto*      transform = child ? roots.Keep(child.value) : nullptr;
  const auto belongs   = transform && ancestor ? transform->TryCall<"IsChildOf", bool(unity::Transform*)>(ancestor)
                                               : il2cpp::CallResult<bool>{};
  if (belongs && belongs.value)
    QueueStore(source, false);
}

void AllianceStoreFrame()
{
  auto& work = alliance_store_work;
  if (!work.pending || Clock::now() < work.due)
    return;
  const bool currency     = std::exchange(work.currency, false);
  work.pending            = false;
  const auto        epoch = work.epoch;
  store_game::Roots roots;
  auto*             source   = roots.Keep(work.source.Get<AllianceStoreScroller>());
  const auto        alive    = unity::TryAlive(source);
  const auto        observed = source && alive && alive.value
                                   ? source->TryCall<"get_AllianceGamesContext", AllianceStoreContext*>()
                                   : il2cpp::CallResult<AllianceStoreContext*>{};
  auto*             context  = observed ? roots.Keep(observed.value) : nullptr;
  const auto        active   = context ? context->TryCall<"get_IsShopViewActive", bool>() : il2cpp::CallResult<bool>{};
  if (!context || !active || !active.value)
    return;
  auto&      fields = store_game::Members();
  auto*      smart  = roots.Keep(fields.scroller.Read(source));
  auto*      pool   = smart ? roots.Keep(fields.spawned.Read(smart)) : nullptr;
  const auto count  = pool ? pool->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
  if (!count || count.value < 0)
    return;
  int                    upgraded = 0, restored = 0, unavailable = 0, failed = 0;
  StoreInventoryTooltip* donor        = nullptr;
  auto*                  tooltip_type = StoreInventoryTooltip::Class::TypeObject();
  if (!tooltip_type)
    return;
  for (int32_t index = 0; index < count.value; ++index) {
    if (work.epoch != epoch || work.source.Get() != source)
      return;
    store_game::Roots card_roots;
    const auto        entry = pool->TryCall<"get_Item", unity::Transform*(int32_t)>(index);
    auto*             rect  = entry ? card_roots.Keep(entry.value) : nullptr;
    const auto        live  = unity::TryAlive(rect);
    if (!rect || !live || !live.value)
      continue;
    const auto tooltip = rect->TryCall<"GetComponentInChildren", StoreInventoryTooltip*>(tooltip_type, true);
    switch (tooltip && tooltip.value ? ReconcileStoreTooltip(tooltip.value, donor, roots) : ResearchUpdate::Unchanged) {
      case ResearchUpdate::Upgraded:
        ++upgraded;
        break;
      case ResearchUpdate::Restored:
        ++restored;
        break;
      case ResearchUpdate::Unavailable:
        ++unavailable;
        break;
      case ResearchUpdate::Failed:
        ++failed;
        break;
      default:
        break;
    }
  }
  bool                        refreshed = false;
  il2cpp::CallResult<int32_t> currency_before, currency_after;
  // Native bundle-derived discovery, not a fixed list of currency IDs. An
  // empty/loading store waits for its real data/regeneration event.
  const auto fetching = context->TryCall<"get_IsFetchingBundlesInProgress", bool>();
  const auto bundles  = context->TryCall<"get_BundlesCount", int32_t>();
  if (currency && work.epoch == epoch && work.source.Get() == source && fetching && !fetching.value && bundles
      && bundles.value > 0) {
    work.refreshing_currency = true;
    struct Reset {
      bool& value;
      ~Reset()
      { value = false; }
    } reset{work.refreshing_currency};
    currency_before        = store_game::CurrencyCount(context, roots);
    const auto current     = source->TryCall<"get_AllianceGamesContext", AllianceStoreContext*>();
    const auto live_source = unity::TryAlive(source);
    if (current && current.value == context && live_source && live_source.value && work.epoch == epoch
        && work.source.Get() == source) {
      refreshed      = context->TryCall<"UpdateStoreCurrency", void()>().succeeded;
      currency_after = store_game::CurrencyCount(context, roots);
    }
  }
  const auto visible  = smart->TryCall<"get_VisibleItemsCount", int32_t>();
  const auto position = smart->TryCall<"get_ScrollPosition", float>();
  spdlog::info("[AllianceGamesStore] pool={} upgraded={} restored={} unavailable={} failed={} currency_refresh={} "
               "currencies={}->{} visible={} position={}",
               count.value, upgraded, restored, unavailable, failed, refreshed,
               currency_before ? std::to_string(currency_before.value) : "unavailable",
               currency_after ? std::to_string(currency_after.value) : "unavailable",
               visible ? std::to_string(visible.value) : "unavailable",
               position ? std::to_string(position.value) : "unavailable");
}

bool InstallAllianceGamesStore()
{
  if (!store_game::Members().Resolve())
    return false;
  il2cpp::HookSet hooks("AllianceGamesStore");
  IL2CPP_STAGE(hooks, AllianceStoreScroller::Class::Method<void()>("OnDidBindCanvasContext"), AllianceStoreBound_Hook);
  IL2CPP_STAGE(hooks, AllianceStoreScroller::Class::Method<void()>("OnAboutToReleaseCanvasContext"),
               AllianceStoreReleased_Hook);
  IL2CPP_STAGE(hooks, ShopListScrollerViewController::Class::Method<void(bool)>("RegenerateSortedBundles"),
               StoreRegenerated_Hook);
  IL2CPP_STAGE(hooks, BundleDataWidget::Class::Method<void(Bundle*)>("RefreshBundle"), StoreBundleRefreshed_Hook);
  IL2CPP_STAGE(hooks, BundleDataWidget::Class::Method<void()>("OnAboutToReleaseContext"), StoreBundleReleased_Hook);
  return hooks.Commit();
}
} // namespace

const Feature kAllianceGamesStore{"AllianceGamesStore", &Config::Patches::alliancegamesstorehooks,
                                  InstallAllianceGamesStore, AllianceStoreFrame};
