// ui.show_alliance_games_research controls native material research tooltips.
// Currency and post-purchase refresh remain active independently of that setting.
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
namespace store_game                   = alliance_store_game;
using Clock                            = std::chrono::steady_clock;
constexpr int kResearchRefreshAttempts = 4;

struct AllianceStoreWork {
  il2cpp::GcHandle  source;
  uint64_t          epoch   = 0;
  bool              pending = false, currency = false, refreshing_currency = false, layout = false;
  Clock::time_point due{};
  bool              research_enabled = true, research_pending = false;
  int               research_attempts = 0;
  Clock::time_point research_due{};
};
// Retained managed handles must not be freed by process static destructors after IL2CPP shuts down.
auto& alliance_store_work = *new AllianceStoreWork;

enum class ResearchUpdate { Unchanged, Upgraded, Annotated, Restored, Unavailable, Failed };

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
struct StoreResearchNotice {
  il2cpp::GcHandle            item, previous_text, published_text;
  store_game::OverridableBool show_description{};
  bool                        override_locale = false;
};
struct OwnedStoreTooltip {
  il2cpp::GcHandle                     widget, plain, action, store;
  il2cpp::GcHandle                     search, locale, settings;
  bool                                 bound = false, plain_enabled = true, retiring = false;
  uint64_t                             generation = 0;
  std::shared_ptr<StoreResearchNotice> notice;
};
auto& alliance_store_tooltips = *new std::vector<std::shared_ptr<OwnedStoreTooltip>>;

std::shared_ptr<OwnedStoreTooltip> OwnedTooltip(StoreInventoryTooltip* tooltip)
{
  for (auto& entry : alliance_store_tooltips)
    if (entry->widget.Get() == tooltip)
      return entry;
  return nullptr;
}

bool RestoreStoreNotice(OwnedStoreTooltip& entry)
{
  const auto notice = entry.notice;
  if (!notice)
    return true;
  store_game::Roots roots;
  auto*             widget = roots.Keep(entry.widget.Get<StoreInventoryTooltip>());
  auto*             plain  = roots.Keep(entry.plain.Get<StoreTooltipTrigger>());
  if (!roots.healthy)
    return false;
  if (!widget || !plain) {
    entry.notice.reset();
    return true;
  }
  const auto alive = unity::TryAlive(widget);
  if (!alive)
    return false;
  auto& f = store_game::Members();
  if (!alive.value
      || f.description_override.Read(plain) != reinterpret_cast<Il2CppString*>(notice->published_text.Get())) {
    // Native refresh/rebind replaced the text. It now owns the description.
    entry.notice.reset();
    return true;
  }
  const auto item = widget->TryCall<"get_Context", InventoryItem*>();
  if (!item)
    return false;
  const bool same_item = item.value && item.value == notice->item.Get();
  const auto shown     = f.show_description.Read(plain);
  bool       ok        = true;
  if (shown.serialized == notice->show_description.serialized && shown.current && shown.modified)
    ok &= f.show_description.Write(plain, notice->show_description);
  if (f.override_locale.Read(plain))
    ok &= f.override_locale.Write(plain, notice->override_locale);
  if (!ok)
    return false;
  // Never put the previous material's body onto a reused card. Ask the native
  // widget for its new item's description after removing our owned string.
  if (!f.description_override.Write(plain,
                                    same_item ? reinterpret_cast<Il2CppString*>(notice->previous_text.Get()) : nullptr))
    return false;
  entry.notice.reset();
  return same_item || widget->TryCall<"UpdateTooltipDescription", void()>().succeeded;
}

bool RestoreStoreTooltip(OwnedStoreTooltip& entry)
{
  if (!entry.bound && !entry.notice)
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
  if (!RestoreStoreNotice(entry))
    return false;
  if (!entry.bound)
    return true;
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

std::shared_ptr<OwnedStoreTooltip> OwnStoreTooltip(StoreInventoryTooltip* widget, StoreTooltipTrigger* plain,
                                                   AllianceStoreScroller* store)
{
  auto entry = OwnedTooltip(widget);
  if (entry && (entry->bound || entry->notice || entry->retiring))
    return entry;
  auto widget_handle = il2cpp::GcHandle::Weak(widget);
  auto plain_handle  = il2cpp::GcHandle::Weak(plain);
  auto store_handle  = il2cpp::GcHandle::Weak(store);
  if (!widget_handle || !plain_handle || !store_handle)
    return nullptr;
  if (!entry) {
    entry = std::make_shared<OwnedStoreTooltip>();
    alliance_store_tooltips.push_back(entry);
  }
  entry->widget = std::move(widget_handle);
  entry->plain  = std::move(plain_handle);
  entry->store  = std::move(store_handle);
  return entry;
}

ResearchUpdate AnnotateStoreTooltip(StoreInventoryTooltip* widget, StoreTooltipTrigger* plain, InventoryItem* item,
                                    AllianceStoreScroller* store, const auto& same_item)
{
  auto owned = OwnStoreTooltip(widget, plain, store);
  if (!owned || owned->retiring)
    return ResearchUpdate::Unavailable;
  auto&      f          = store_game::Members();
  const auto generation = ++owned->generation;
  auto       current    = [&] {
    return same_item() && !owned->retiring && owned->generation == generation && f.trigger.Read(widget) == plain
           && !f.action.Read(widget);
  };
  // Native refresh preserves the material's ordinary localized description,
  // including maximum/timer details, before we add the passive status.
  if (!widget->TryCall<"UpdateTooltipDescription", void()>() || !current())
    return ResearchUpdate::Unavailable;
  store_game::Roots roots;
  const auto        body = store_game::Description(plain, roots);
  if (!body || !current())
    return ResearchUpdate::Unavailable;
  auto  notice   = std::make_shared<StoreResearchNotice>();
  auto* previous = roots.Keep(f.description_override.Read(plain));
  auto  text     = il2cpp::ToUtf8(body.value);
  if (!text.empty())
    text += "\n\n";
  text += "No Available Research";
  auto* published = roots.Keep(il2cpp::NewString(text));
  if (!roots.healthy || !published || !current())
    return ResearchUpdate::Unavailable;
  notice->item           = il2cpp::GcHandle::Weak(item);
  notice->previous_text  = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(previous));
  notice->published_text = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(published));
  if (!notice->item || (previous && !notice->previous_text) || !notice->published_text)
    return ResearchUpdate::Unavailable;
  notice->show_description = f.show_description.Read(plain);
  notice->override_locale  = f.override_locale.Read(plain);
  owned->notice            = notice;
  const store_game::OverridableBool shown{notice->show_description.serialized, true, true};
  if (!f.description_override.Write(plain, published) || !f.show_description.Write(plain, shown)
      || !f.override_locale.Write(plain, true) || !current() || owned->notice != notice) {
    (void)RestoreStoreTooltip(*owned);
    return ResearchUpdate::Failed;
  }
  return ResearchUpdate::Annotated;
}

ResearchUpdate ReconcileStoreTooltip(StoreInventoryTooltip* tooltip, StoreInventoryTooltip*& donor,
                                     store_game::Roots& pass_roots)
{
  auto owned = OwnedTooltip(tooltip);
  // Remove only our text before any eligibility decision or appearance copy.
  // Unknown data must leave the ordinary tooltip, never a stale negative.
  const bool had_notice = owned && owned->notice;
  if (had_notice && !RestoreStoreTooltip(*owned))
    return ResearchUpdate::Failed;
  if (!Config::Get().ui.show_alliance_games_research)
    return owned && owned->bound ? (RestoreStoreTooltip(*owned) ? ResearchUpdate::Restored : ResearchUpdate::Failed)
           : had_notice          ? ResearchUpdate::Restored
                                 : ResearchUpdate::Unchanged;
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
    if (!Config::Get().ui.show_alliance_games_research || !store || alliance_store_work.epoch != epoch
        || alliance_store_work.source.Get() != store)
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
  // Existing native action components and unrelated arrangements remain native.
  if (!roots.healthy || (old_action && (!owned || owned->action.Get() != old_action))
      || (old_action ? old_trigger != old_action : old_trigger != plain))
    return ResearchUpdate::Unchanged;
  const auto eligible = store_game::Eligible(item);
  if (!eligible || !same_item())
    return ResearchUpdate::Unavailable;
  if (!eligible.value) {
    if (owned && owned->bound && !RestoreStoreTooltip(*owned))
      return ResearchUpdate::Failed;
    return same_item() ? AnnotateStoreTooltip(tooltip, plain, item, store, same_item) : ResearchUpdate::Unavailable;
  }
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
  owned = OwnStoreTooltip(tooltip, plain, store);
  if (!owned || owned->retiring)
    return ResearchUpdate::Unavailable;
  if (!action)
    action = store_game::ActionComponent(tooltip, true, roots);
  if (!action)
    return ResearchUpdate::Unavailable;
  if (owned->action.Get() != action) {
    owned->action = il2cpp::GcHandle::Weak(action);
    if (!owned->action) {
      store_game::Enabled(action, false);
      return ResearchUpdate::Unavailable;
    }
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
  // Debounce through separate-frame native card spawning.
  work.due     = Clock::now() + std::chrono::milliseconds(250);
  work.pending = true;
  work.currency |= currency && !work.refreshing_currency;
  work.layout |= currency && !work.refreshing_currency;
}

void AllianceStoreScroller_OnDidBindCanvasContext_Hook(auto original, AllianceStoreScroller* self)
{
  original(self);
  std::erase_if(alliance_store_tooltips, [](auto& entry) { return !entry->widget.Get(); });
  QueueStore(self, true);
}

void AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook(auto original, AllianceStoreScroller* self)
{
  auto& work = alliance_store_work;
  if (work.source.Get() == self) {
    work.source.Reset();
    work.pending = work.currency = work.layout = false;
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

void ShopListScrollerViewController_RegenerateSortedBundles_Hook(auto original, ShopListScrollerViewController* self,
                                                                 bool instant)
{
  original(self, instant);
  if (store_game::IsStore(self) && !alliance_store_work.refreshing_currency)
    QueueStore(reinterpret_cast<AllianceStoreScroller*>(self), true);
}

void BundleDataWidget_OnAboutToReleaseContext_Hook(auto original, BundleDataWidget* self)
{
  auto* tooltip = store_game::Members().bundle_tooltip.Read(self);
  if (auto entry = OwnedTooltip(tooltip))
    (void)RestoreStoreTooltip(*entry);
  original(self);
}

void QueueStoreCard(BundleDataWidget* self)
{
  // Native OnDidBindContext sets this AFTER SetupTooltip. Never use the
  // RefreshBundle argument or a pre-bind owner as current-card identity.
  auto* owner = store_game::Members().bundle_owner.Read(self);
  if (owner && alliance_store_work.source.Get() == owner)
    QueueStore(reinterpret_cast<AllianceStoreScroller*>(owner), false);
}
void BundleDataWidget_RefreshBundle_Hook(auto original, BundleDataWidget* self, Bundle* bundle)
{
  original(self, bundle);
  QueueStoreCard(self);
}
void BundleDataWidget_OnDidBindContext_Hook(auto original, BundleDataWidget* self)
{
  original(self);
  QueueStoreCard(self);
}

void AllianceStoreFrame()
{
  auto&      work             = alliance_store_work;
  const auto now              = Clock::now();
  const bool research_enabled = Config::Get().ui.show_alliance_games_research;
  if (research_enabled != work.research_enabled) {
    work.research_enabled  = research_enabled;
    work.research_pending  = true;
    work.research_attempts = 0;
    work.research_due      = {};
  }
  const bool research_pending = work.research_pending && now >= work.research_due;
  if (research_pending) {
    work.research_due = now + std::chrono::milliseconds(250);
    if (!research_enabled) {
      work.research_pending = false;
      // Restoration may reenter native bind/pruning, and must work even while
      // Settings obscures the store or its eligibility data is unavailable.
      const auto entries = alliance_store_tooltips;
      for (const auto& entry : entries)
        if ((entry->bound || entry->notice) && !RestoreStoreTooltip(*entry))
          work.research_pending = true;
    }
  }
  const bool store_pending = work.pending && now >= work.due;
  if (!store_pending && !(research_pending && research_enabled))
    return;
  const bool currency = store_pending && std::exchange(work.currency, false);
  const bool layout   = store_pending && std::exchange(work.layout, false);
  if (store_pending)
    work.pending = false;
  const auto epoch           = work.epoch;
  auto*      observed_source = work.source.Get<AllianceStoreScroller>();
  if (!observed_source) {
    // The next native store bind already schedules its own pass. Do not keep
    // polling while the user is elsewhere with no retained live store.
    if (research_enabled)
      work.research_pending = false;
    return;
  }
  store_game::Roots roots;
  auto*             source = roots.Keep(observed_source);
  const auto        alive  = unity::TryAlive(source);
  if (alive && !alive.value && research_enabled)
    work.research_pending = false;
  const auto observed = source && alive && alive.value
                            ? source->TryCall<"get_AllianceGamesContext", AllianceStoreContext*>()
                            : il2cpp::CallResult<AllianceStoreContext*>{};
  auto*      context  = observed ? roots.Keep(observed.value) : nullptr;
  const auto active   = context ? context->TryCall<"get_IsShopViewActive", bool>() : il2cpp::CallResult<bool>{};
  if (!context || !active || !active.value)
    return;
  if (work.epoch != epoch || work.source.Get() != source)
    return;
  auto& fields = store_game::Members();
  auto* smart  = roots.Keep(fields.scroller.Read(source));
  if (smart && (fields.regenerating.Read(smart) || fields.spawning.Read(smart) || fields.initializing.Read(smart))) {
    work.pending |= store_pending;
    work.currency |= currency;
    work.layout |= layout;
    work.due = Clock::now() + std::chrono::milliseconds(250);
    return;
  }
  bool position_notified = false;
  if (layout && smart) {
    const auto position   = store_game::NormalizedPosition(smart);
    const auto live_smart = unity::TryAlive(smart);
    if (position && live_smart && live_smart.value && work.epoch == epoch && work.source.Get() == source
        && fields.scroller.Read(source) == smart) {
      position_notified = store_game::NotifyPosition(smart, position.value);
    }
  }
  if (work.epoch != epoch || work.source.Get() != source || fields.scroller.Read(source) != smart)
    return;
  auto*      pool  = smart ? roots.Keep(fields.spawned.Read(smart)) : nullptr;
  const auto count = pool ? pool->TryCall<"get_Count", int32_t>() : il2cpp::CallResult<int32_t>{};
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
  if (work.research_pending && research_enabled && Config::Get().ui.show_alliance_games_research && work.epoch == epoch
      && work.source.Get() == source) {
    // A preference change may beat donor/eligibility readiness without another
    // native bind event. Retry settled failures briefly, never poll permanently
    // for a missing/drifted capability. Native store events can still retry later.
    work.research_pending = (unavailable || failed) && ++work.research_attempts < kResearchRefreshAttempts;
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
  if (layout)
    spdlog::info("[AllianceGamesStore] settled layout notification={} (position unchanged)", position_notified);
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
  IL2CPP_STAGE(hooks, AllianceStoreScroller::Class::Method<void()>("OnDidBindCanvasContext"),
               AllianceStoreScroller_OnDidBindCanvasContext_Hook);
  IL2CPP_STAGE(hooks, AllianceStoreScroller::Class::Method<void()>("OnAboutToReleaseCanvasContext"),
               AllianceStoreScroller_OnAboutToReleaseCanvasContext_Hook);
  IL2CPP_STAGE(hooks, ShopListScrollerViewController::Class::Method<void(bool)>("RegenerateSortedBundles"),
               ShopListScrollerViewController_RegenerateSortedBundles_Hook);
  IL2CPP_STAGE(hooks, BundleDataWidget::Class::Method<void(Bundle*)>("RefreshBundle"),
               BundleDataWidget_RefreshBundle_Hook);
  IL2CPP_STAGE(hooks, BundleDataWidget::Class::Method<void()>("OnAboutToReleaseContext"),
               BundleDataWidget_OnAboutToReleaseContext_Hook);
  IL2CPP_STAGE(hooks, BundleDataWidget::Class::Method<void()>("OnDidBindContext"),
               BundleDataWidget_OnDidBindContext_Hook);
  return hooks.Commit();
}
} // namespace

const Feature kAllianceGamesStore{"AllianceGamesStore", &Config::Patches::alliancegamesstorehooks,
                                  InstallAllianceGamesStore, AllianceStoreFrame};
