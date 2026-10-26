// Native Mod Settings page/category navigation. The complete native context tree is staged off-screen, then its
// exact root is published under query custody. Native navigation retains Back/Escape and pooled widget ownership.
#include "game/settings_pages.h"
#include "config.h"
#include "features/features.h"
#include "game/settings_search.h"
#include "settings/maintenance_pages.h"
#include "settings/mod_pages.h"
#include "settings/native_boolean_callback.h"
#include "settings/native_category_publication.h"
#include "settings/native_controls.h"
#include "settings/page_sections.h"
#include <algorithm>
#include <array>
#include <il2cpp/hook.h>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <spdlog/spdlog.h>
#include <thread>
#include <tuple>
#include <vector>

namespace
{
using namespace mod_settings;
using namespace mod_settings::native;
struct SettingsPageBinding {
  il2cpp::GcHandle            context, query;
  const PageCatalog::Page*    page    = nullptr;
  const PageCatalog::Heading* heading = nullptr;
  std::string                 id, label;
  bool                        category = false;
};
struct SettingsPageTree {
  il2cpp::GcHandle                      settings, director, parent, root;
  std::vector<SettingsPageBinding>      bindings;
  CategoryPublication<il2cpp::GcHandle> publication;
  bool                                  revoked = false;
};
struct SettingsPageReference {
  std::shared_ptr<SettingsPageTree> tree;
  std::size_t                       index;
};
struct SettingsPageVisit {
  il2cpp::GcHandle                    controller, context;
  il2cpp::GcHandle                    search_return;
  std::shared_ptr<SettingsPageTree>   tree;
  const PageCatalog::Page*            page = nullptr;
  PageSections                        sections;
  std::vector<SettingsOptionContext*> shown; // Comparison only; native contexts own these rows.
  bool                                refreshing = false, has_shown = false;
  std::string                         focus_item;
};
struct SettingsPageNavigation {
  std::weak_ptr<SettingsPageVisit> from;
  il2cpp::GcHandle                 target;
  std::string                      item;
  PageVisitToken                   token             = 0;
  bool                             return_to_current = false;
};
enum class SettingsPageLabelKind { Category, Text, Title };
struct SettingsPageLabel {
  il2cpp::GcHandle      widget, context, label;
  std::string           applied;
  SettingsPageLabelKind kind;
  bool                  restoring = false;
};
struct SettingsPageItemAdapter {
  PageItemAppender append     = nullptr;
  PageItemResolver resolve    = nullptr;
  PageInvalidator  invalidate = nullptr;
};
struct NativePagesState {
  std::vector<PageCatalog::Page>                                                     pages;
  std::array<SettingsPageItemAdapter, static_cast<std::size_t>(PageItemKind::Count)> adapters;
  std::vector<std::shared_ptr<SettingsPageTree>>                                     trees;
  std::map<il2cpp::Object*, std::shared_ptr<SettingsPageLabel>>                      labels;
  std::shared_ptr<SettingsPageVisit>                                                 visit;
  std::optional<SettingsPageNavigation>                                              pending;
  PageVisitObserver                                                                  observer = nullptr;
  NativeCallback<int>                                                                query;
  NativeCallback<Il2CppString*>                                                      text;
  std::thread::id                                                                    thread;
  uint64_t                                                                           navigation = 0;
  bool                                                                               active = false, refresh = false;
};
NativePagesState& NativePages()
{
  static auto* state = new NativePagesState;
  return *state;
}
bool PagesThread()
{ return NativePages().active && NativePages().thread == std::this_thread::get_id(); }
std::optional<std::size_t> PageChildOccurrences(SettingsContainerContext* parent, SettingsOptionContext* row)
{
  auto children = settings_pages_binding::Children(parent);
  if (!children)
    return std::nullopt;
  return std::ranges::count_if(*children, [&](const auto& child) { return child.Get() == row; });
}
struct SettingsCategoryHost {
  std::optional<bool> Owned(Il2CppObject* row, Il2CppObject* query)
  {
    auto* category = reinterpret_cast<SettingsOptionContext*>(row)->As<SettingsCategoryContext>();
    if (!category)
      return false;
    const auto observed = category->TryQuery();
    return observed ? std::optional<bool>(observed.value == query) : std::nullopt;
  }
  std::optional<std::size_t> Occurrences(Il2CppObject* parent, Il2CppObject* row)
  {
    return PageChildOccurrences(reinterpret_cast<SettingsContainerContext*>(parent),
                                reinterpret_cast<SettingsOptionContext*>(row));
  }
  std::optional<Il2CppObject*> Parent(Il2CppObject* row)
  {
    const auto parent = reinterpret_cast<SettingsCategoryContext*>(row)->TryParent();
    return parent ? std::optional<Il2CppObject*>(parent.value) : std::nullopt;
  }
  void Remove(Il2CppObject* parent, Il2CppObject* row)
  {
    (void)reinterpret_cast<SettingsContainerContext*>(parent)->TryCall<"RemoveChild", void(SettingsOptionContext*)>(
        reinterpret_cast<SettingsOptionContext*>(row));
  }
};
std::optional<SettingsPageReference> PageBinding(SettingsOptionContext* context, bool published = true)
{
  if (!context)
    return {};
  const auto trees = NativePages().trees;
  for (const auto& tree : trees) {
    if (tree->revoked || (published && tree->publication.state() != CategoryPublicationState::Published))
      continue;
    for (std::size_t i = 0, count = tree->bindings.size(); i < count; ++i) {
      if (tree->bindings[i].context.Get() != context)
        continue;
      auto query = il2cpp::GcHandle::Strong(tree->bindings[i].query.Get());
      if (!query)
        return {};
      const auto observed = settings_pages_binding::Query(context);
      if (!observed || observed.value != query.Get() || tree->revoked || tree->bindings[i].context.Get() != context
          || !NativePages().query.Owns(reinterpret_cast<Il2CppDelegate*>(observed.value)))
        return {};
      return SettingsPageReference{tree, i};
    }
  }
  return {};
}
BooleanSetting* ResolvePageBoolean(SettingsOptionContext* context)
{
  if (!PagesThread() || !context)
    return nullptr;
  const auto parent  = settings_pages_binding::Parent(context);
  auto       binding = parent && parent.value ? PageBinding(parent.value, false) : std::nullopt;
  if (!binding)
    return nullptr;
  const auto* page = binding->tree->bindings[binding->index].page;
  if (page)
    for (auto* setting : page->Controls<BooleanSetting>())
      if (context->HasLabel(setting->id()))
        return setting;
  return nullptr;
}
il2cpp::Object* BoundLabelContext(il2cpp::Object* widget, SettingsPageLabelKind kind)
{
  if (kind != SettingsPageLabelKind::Title) {
    const auto context = widget->TryCall<"get_Context", SettingsOptionContext*>();
    return context ? context.value : nullptr;
  }
  const auto settings = reinterpret_cast<SettingsPageController*>(widget)->TrySettings();
  const auto selected =
      settings && settings.value ? settings.value->TrySelected() : il2cpp::CallResult<SettingsContainerContext*>{};
  return selected ? selected.value : nullptr;
}
bool ReleasePageLabel(il2cpp::Object* widget, const std::shared_ptr<SettingsPageLabel>& expected = {})
{
  auto found = NativePages().labels.find(widget);
  if (found == NativePages().labels.end() || (expected && found->second != expected))
    return true;
  auto view = found->second;
  if (view->restoring)
    return false;
  view->restoring = true;
  struct RestoreGuard {
    bool& value;
    ~RestoreGuard()
    { value = false; }
  } restore_guard{view->restoring};
  auto       root     = il2cpp::GcHandle::Strong(view->label.Get());
  auto*      label    = root.Get<TextLocalizer>();
  const auto alive    = label ? unity::TryAlive(label) : il2cpp::CallResult<bool>{true, false};
  bool       restored = alive && !alive.value;
  if (alive && alive.value) {
    const auto current = label->TryCall<"CurrentText", Il2CppString*>();
    if (current && il2cpp::ToUtf8(current.value) != view->applied)
      restored = true;
    else if (current) {
      const auto cleared = label->TryCall<"ClearTextOverride", void>();
      restored           = cleared && !label->textOverride();
    }
  }
  found = NativePages().labels.find(widget);
  if (restored && found != NativePages().labels.end() && found->second == view)
    NativePages().labels.erase(found);
  return restored;
}
void ApplyPageLabel(il2cpp::Object* widget, SettingsOptionContext* context, TextLocalizer* label, std::string text,
                    SettingsPageLabelKind kind)
{
  if (!context || !label || !ReleasePageLabel(widget))
    return;
  auto view     = std::make_shared<SettingsPageLabel>();
  view->widget  = il2cpp::GcHandle::Weak(widget);
  view->context = il2cpp::GcHandle::Weak(context);
  view->label   = il2cpp::GcHandle::Weak(label);
  view->kind    = kind;
  view->applied = std::move(text);
  if (!view->widget || !view->context || !view->label || BoundLabelContext(widget, kind) != context)
    return;
  NativePages().labels.insert_or_assign(widget, view);
  (void)label->TryCall<"OverrideLocalizedText", void(Il2CppString*)>(il2cpp::NewString(view->applied));
  if (BoundLabelContext(widget, kind) != context)
    ReleasePageLabel(widget, view);
}
void ClosePageVisit()
{
  auto visit = std::exchange(NativePages().visit, {});
  if (!visit)
    return;
  auto controller = il2cpp::GcHandle::Strong(visit->controller.Get());
  auto context    = il2cpp::GcHandle::Strong(visit->context.Get());
  if (NativePages().observer)
    NativePages().observer(controller.Get<SettingsPageController>(), context.Get<SettingsContainerContext>(),
                           visit->page->id, NativePages().navigation, false);
  if (controller)
    ReleasePageLabel(controller.Get<il2cpp::Object>());
  // Revoke the visit before callbacks. A leave callback may open another page or destroy this controller.
  if (visit->page && visit->page->leave) {
    try {
      visit->page->leave();
    } catch (...) {
      spdlog::warn("[NativeSettingsPages] page cleanup unavailable");
    }
  }
}
bool CurrentVisit(const std::shared_ptr<SettingsPageVisit>& visit)
{
  if (!visit || NativePages().visit != visit || visit->tree->revoked
      || visit->tree->publication.state() != CategoryPublicationState::Published)
    return false;
  auto controller = il2cpp::GcHandle::Strong(visit->controller.Get());
  auto context    = il2cpp::GcHandle::Strong(visit->context.Get());
  if (!controller || !context)
    return false;
  const auto settings = controller.Get<SettingsPageController>()->TrySettings();
  const auto selected =
      settings && settings.value ? settings.value->TrySelected() : il2cpp::CallResult<SettingsContainerContext*>{};
  return selected && selected.value == context.Get() && NativePages().visit == visit;
}
std::optional<PageItemIdentity> PageRowIdentity(SettingsOptionContext* row)
{
  if (!row)
    return {};
  if (auto* setting = OwnedBooleanSetting(row))
    return PageItemIdentity{setting->id()};
  if (auto binding = PageBinding(row))
    return PageItemIdentity{binding->tree->bindings[binding->index].id};
  for (const auto& adapter : NativePages().adapters)
    if (adapter.resolve)
      if (auto identity = adapter.resolve(row))
        return identity;
  return {};
}
bool RefreshPageVisit(const std::shared_ptr<SettingsPageVisit>& visit)
{
  if (!CurrentVisit(visit) || visit->refreshing)
    return false;
  visit->refreshing = true;
  struct RefreshGuard {
    bool& value;
    ~RefreshGuard()
    { value = false; }
  } guard{visit->refreshing};
  auto        context    = il2cpp::GcHandle::Strong(visit->context.Get());
  auto        controller = il2cpp::GcHandle::Strong(visit->controller.Get());
  auto        settings   = il2cpp::GcHandle::Strong(visit->tree->settings.Get());
  auto        director   = il2cpp::GcHandle::Strong(visit->tree->director.Get());
  const auto& actions    = NativePages().adapters[static_cast<std::size_t>(PageItemKind::Action)];
  if (!context || !controller || !settings || !director)
    return false;
  if (actions.append)
    for (const auto& item : visit->page->items)
      if (std::holds_alternative<ActionSetting*>(item)
          && (!actions.append(director.Get<SettingsSectionDirector>(), settings.Get<SettingsContext>(),
                              context.Get<SettingsContainerContext>(), item)
              || !CurrentVisit(visit)))
        return false;
  auto children = settings_pages_binding::Children(context.Get<SettingsContainerContext>());
  if (!children)
    return false;
  struct Row {
    SettingsOptionContext* context;
    std::size_t            position, index;
  };
  std::vector<Row> ordered;
  for (const auto& child : *children) {
    auto* row = child.Get<SettingsOptionContext>();
    if (!row)
      continue;
    const auto identity = PageRowIdentity(row);
    if (identity && !identity->visible)
      continue;
    if (identity && !FilterMaintenanceSearch(*visit->page, identity->id))
      continue;
    if (!identity || visit->sections.Visible(*visit->page, identity->id))
      ordered.push_back({row, identity ? visit->page->PositionFor(identity->id) : visit->page->items.size(),
                         identity ? identity->index : 0});
  }
  if (!CurrentVisit(visit))
    return false;
  std::stable_sort(ordered.begin(), ordered.end(), [](const Row& a, const Row& b) {
    return std::tie(a.position, a.index) < std::tie(b.position, b.index);
  });
  std::vector<SettingsOptionContext*> shown;
  for (const auto& row : ordered)
    shown.push_back(row.context);
  if (visit->has_shown && shown == visit->shown)
    return true;
  if (BindPageRows(controller.Get<SettingsPageController>(), context.Get<SettingsContainerContext>(), shown)
          != PageBindResult::Bound
      || !CurrentVisit(visit))
    return false;
  visit->shown     = std::move(shown);
  visit->has_shown = true;
  return true;
}
void RepairCompletePageList(const std::shared_ptr<SettingsPageVisit>& visit)
{
  if (!CurrentVisit(visit))
    return;
  auto context    = il2cpp::GcHandle::Strong(visit->context.Get());
  auto controller = il2cpp::GcHandle::Strong(visit->controller.Get());
  auto children   = settings_pages_binding::Children(context.Get<SettingsContainerContext>());
  if (!children || !CurrentVisit(visit))
    return;
  std::vector<SettingsOptionContext*> rows;
  rows.reserve(children->size());
  for (const auto& child : *children)
    rows.push_back(child.Get<SettingsOptionContext>());
  visit->sections.ExpandAll();
  visit->refreshing = true;
  struct Guard {
    bool& value;
    ~Guard()
    { value = false; }
  } guard{visit->refreshing};
  // Repair only the panel's IList through the qualified untyped virtual slot. Repeating OnCategorySelected
  // would also repeat native selection/Back/event side effects after its original already ran.
  const auto repaired =
      BindPageRows(controller.Get<SettingsPageController>(), context.Get<SettingsContainerContext>(), rows);
  if (repaired == PageBindResult::Bound && CurrentVisit(visit)) {
    visit->shown     = std::move(rows);
    visit->has_shown = true;
  } else {
    visit->has_shown = false;
  }
}
void RenderPageWidget(il2cpp::Object* widget, SettingsPageLabelKind kind)
{
  auto widget_root = il2cpp::GcHandle::Strong(widget);
  if (!widget_root)
    return;
  const auto alive = unity::TryAlive(widget);
  if (!alive)
    return;
  if (!alive.value) {
    ReleasePageLabel(widget);
    return;
  }
  auto  context_root = il2cpp::GcHandle::Strong(BoundLabelContext(widget, kind));
  auto* context      = context_root.Get<SettingsOptionContext>();
  auto  binding      = PageBinding(context, false);
  if (!binding)
    return;
  const auto& record = binding->tree->bindings[binding->index];
  auto        text   = record.label;
  if (binding->tree->publication.state() != CategoryPublicationState::Published)
    text += " — unavailable";
  else if (record.heading && record.heading->collapsible) {
    auto       visit     = NativePages().visit;
    const bool collapsed = visit && visit->page == record.page && visit->sections.Collapsed(*record.heading);
    text                 = std::string(collapsed ? "+ " : "− ") + text;
    if (collapsed && record.heading->summary) {
      const auto summary = record.heading->summary();
      if (!summary.empty())
        text += " — " + summary;
    }
  } else if (record.category && record.page && record.page->summary) {
    const auto summary = record.page->summary();
    if (!summary.empty())
      text += " — " + summary;
  }
  TextLocalizer* label =
      kind == SettingsPageLabelKind::Category ? reinterpret_cast<SettingsCategoryWidget*>(widget)->Label()
      : kind == SettingsPageLabelKind::Text   ? reinterpret_cast<SettingsTextWidget*>(widget)->Label()
                                              : reinterpret_cast<SettingsPageController*>(widget)->Title();
  ApplyPageLabel(widget, context, label, std::move(text), kind);
}
void SettingsCategoryWidget_OnDidBindContext_Hook(auto original, SettingsCategoryWidget* widget)
{
  if (PagesThread())
    ReleasePageLabel(widget);
  original(widget);
  if (PagesThread())
    RenderPageWidget(widget, SettingsPageLabelKind::Category);
}
void SettingsCategoryWidget_OnAboutToReleaseContext_Hook(auto original, SettingsCategoryWidget* widget)
{
  if (PagesThread())
    ReleasePageLabel(widget);
  original(widget);
}
void SettingsTextWidget_SetWidgetData_Hook(auto original, SettingsTextWidget* widget)
{
  if (PagesThread())
    ReleasePageLabel(widget);
  original(widget);
  if (PagesThread())
    RenderPageWidget(widget, SettingsPageLabelKind::Text);
}
void SettingsTextWidget_ClearWidgetData_Hook(auto original, SettingsTextWidget* widget)
{
  if (PagesThread())
    ReleasePageLabel(widget);
  original(widget);
}
void SettingsPageController_OnCategorySelected_Hook(auto original, SettingsPageController* controller,
                                                    SettingsContainerContext* context)
{
  if (!PagesThread())
    return original(controller, context);
  auto controller_root = il2cpp::GcHandle::Strong(controller);
  auto context_root    = il2cpp::GcHandle::Strong(context);
  auto binding         = PageBinding(context, false);
  if (binding && binding->tree->publication.state() != CategoryPublicationState::Published)
    return; // An incompletely observed AddChild never authorizes navigation into a provisional tree.
  if (binding && binding->tree->bindings[binding->index].heading) {
    auto        visit   = NativePages().visit;
    const auto* heading = binding->tree->bindings[binding->index].heading;
    if (!heading->collapsible || !CurrentVisit(visit) || visit->refreshing || visit->controller.Get() != controller
        || visit->page != binding->tree->bindings[binding->index].page)
      return;
    const auto before = visit->sections.Snapshot();
    visit->sections.Toggle(*heading);
    if (!RefreshPageVisit(visit) && CurrentVisit(visit)) {
      visit->sections.Restore(before);
      RepairCompletePageList(visit);
    }
    RequestPageRefresh();
    return;
  }
  auto previous = NativePages().visit;
  if (CurrentVisit(previous) && previous->controller.Get() == controller && previous->search_return) {
    auto       current             = il2cpp::GcHandle::Strong(previous->context.Get());
    auto       destination         = il2cpp::GcHandle::Strong(previous->search_return.Get());
    const auto parent              = current ? settings_pages_binding::Parent(current.Get<SettingsOptionContext>())
                                             : il2cpp::CallResult<SettingsContainerContext*>{};
    auto       destination_binding = destination ? PageBinding(destination.Get<SettingsOptionContext>()) : std::nullopt;
    if (parent && parent.value == context && destination_binding && destination_binding->tree == previous->tree) {
      context_root = std::move(destination);
      context      = context_root.Get<SettingsContainerContext>();
      binding      = std::move(destination_binding);
    }
  }
  const auto request = ++NativePages().navigation;
  ClosePageVisit();
  if (NativePages().navigation != request)
    return;
  std::shared_ptr<SettingsPageVisit> visit;
  if (binding) {
    visit             = std::make_shared<SettingsPageVisit>();
    visit->controller = il2cpp::GcHandle::Weak(controller);
    visit->context    = il2cpp::GcHandle::Weak(context);
    visit->tree       = binding->tree;
    visit->page       = binding->tree->bindings[binding->index].page;
    visit->sections.Begin(*visit->page);
    visit->refreshing = true;
    if (visit->controller && visit->context)
      NativePages().visit = visit;
  }
  original(controller, context); // Native navigation owns SelectedOption, Back and panel release/bind.
  if (!visit || NativePages().navigation != request || NativePages().visit != visit)
    return;
  visit->refreshing = false;
  if (!CurrentVisit(visit)) {
    ClosePageVisit();
    return;
  }
  RenderPageWidget(controller, SettingsPageLabelKind::Title);
  if (!RefreshPageVisit(visit) && CurrentVisit(visit)) {
    RepairCompletePageList(visit);
  }
  if (CurrentVisit(visit) && NativePages().observer)
    NativePages().observer(controller, context, visit->page->id, request, true);
}
void SettingsPageController_OnDestroy_Hook(auto original, SettingsPageController* controller)
{
  if (PagesThread()) {
    if (NativePages().visit && NativePages().visit->controller.Get() == controller) {
      ++NativePages().navigation;
      ClosePageVisit();
    }
    ReleasePageLabel(controller);
  }
  original(controller);
}
int PageQuery(Il2CppObject*, const MethodInfo*)
{ return 0; } // OptionState.Visible. Only a completed off-screen tree is published.
Il2CppString* PageText(Il2CppObject*, const MethodInfo*)
{ return il2cpp::NewString(""); }
bool AddPageText(const std::shared_ptr<SettingsPageTree>& tree, SettingsSectionDirector* director,
                 SettingsPageContext* settings, SettingsContainerContext* parent, const PageCatalog::Page& page,
                 std::string id, std::string label, const PageCatalog::Heading* heading = nullptr,
                 std::string canonical = {})
{
  auto query  = il2cpp::GcHandle::Strong(SettingsPageContext::CategoryDelegate(director, NativePages().query.method()));
  auto getter = il2cpp::GcHandle::Strong(SettingsPageContext::TextDelegate(director, NativePages().text.method()));
  auto before = settings_pages_binding::Children(parent);
  if (!query || !getter || !before)
    return false;
  const auto added =
      settings->TryAddText(parent, il2cpp::NewString(id), getter.Get<settings_binding::ReferenceDelegate>(),
                           query.Get<settings_binding::ReferenceDelegate>());
  auto after = settings_pages_binding::Children(parent);
  if (!added || !after)
    return false;
  SettingsOptionContext* found = nullptr;
  for (const auto& child : *after) {
    auto* row = child.Get<SettingsOptionContext>();
    if (!row || std::ranges::any_of(*before, [&](const auto& old) { return old.Get() == row; }))
      continue;
    const auto observed = settings_pages_binding::Query(row);
    if (!observed)
      return false;
    if (observed.value != query.Get())
      continue;
    if (found || !row->Is<SettingsTextContext>() || !row->HasLabel(id))
      return false;
    found = row;
  }
  if (!found)
    return false;
  tree->bindings.push_back({il2cpp::GcHandle::Weak(found), il2cpp::GcHandle::Weak(query.Get()), &page, heading,
                            canonical.empty() ? std::move(id) : std::move(canonical), std::move(label), false});
  return bool(tree->bindings.back().context) && bool(tree->bindings.back().query);
}
SettingsCategoryContext* AddPageCategory(const std::shared_ptr<SettingsPageTree>& tree,
                                         SettingsSectionDirector* director, SettingsPageContext* settings,
                                         SettingsContainerContext* parent, const PageCatalog::Page& page,
                                         const PageCatalog::Heading* heading = nullptr)
{
  const auto& id = heading ? heading->id : page.id;
  auto query = il2cpp::GcHandle::Strong(SettingsPageContext::CategoryDelegate(director, NativePages().query.method()));
  if (!query)
    return nullptr;
  const auto added =
      settings->TryAddCategory(parent, il2cpp::NewString(id), query.Get<settings_binding::ReferenceDelegate>());
  auto  row      = il2cpp::GcHandle::Strong(added ? added.value : nullptr);
  auto* category = row.Get<SettingsCategoryContext>();
  if (!category || !category->HasLabel(id))
    return nullptr;
  const auto observed_query  = category->TryQuery();
  const auto observed_parent = category->TryParent();
  const auto count           = PageChildOccurrences(parent, category);
  if (!observed_query || observed_query.value != query.Get() || !observed_parent || observed_parent.value != parent
      || !count || *count != 1)
    return nullptr;
  tree->bindings.push_back({il2cpp::GcHandle::Weak(category), il2cpp::GcHandle::Weak(query.Get()), &page, heading, id,
                            heading ? heading->label : page.label, true});
  return tree->bindings.back().context && tree->bindings.back().query ? category : nullptr;
}
std::optional<PageItemKind> PageItemFamily(const PageCatalog::Item& item)
{
  if (std::holds_alternative<ChoiceSetting*>(item))
    return PageItemKind::Choice;
  if (std::holds_alternative<SliderSetting*>(item))
    return PageItemKind::Slider;
  if (std::holds_alternative<ActionSetting*>(item))
    return PageItemKind::Action;
  return {};
}
bool PopulateSettingsPage(const std::shared_ptr<SettingsPageTree>& tree, SettingsSectionDirector* director,
                          SettingsPageContext* settings, SettingsCategoryContext* category,
                          const PageCatalog::Page& page)
{
  for (const auto& item : page.items) {
    const auto failed = [&]() {
      const auto id = std::visit(
          [](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, PageCatalog::Heading>)
              return value.id;
            else if constexpr (requires { value->state(); })
              return value->state().id();
            else
              return value->id();
          },
          item);
      spdlog::error("[NativeSettingsPages] Could not construct page '{}' control '{}'; staged menu not published",
                    page.id, id);
      return false;
    };
    if (const auto* heading = std::get_if<PageCatalog::Heading>(&item)) {
      if (heading->collapsible) {
        if (!AddPageCategory(tree, director, settings, category, page, heading))
          return failed();
      } else if (!AddPageText(tree, director, settings, category, page, heading->id, heading->label, heading)) {
        return failed();
      }
    } else if (auto* const* boolean = std::get_if<BooleanSetting*>(&item)) {
      if ((*boolean)->Observe().state.availability == Availability::Unsupported)
        continue;
      AppendBooleanRow(director, settings, category, **boolean);
      const auto rows = settings_pages_binding::Children(category);
      if (!rows || !std::ranges::any_of(*rows, [&](const auto& row) {
            return row.Get() && OwnedBooleanSetting(row.Get<SettingsOptionContext>()) == *boolean;
          }))
        return failed();
    } else {
      if (auto* const* choice = std::get_if<ChoiceSetting*>(&item))
        if (!AddPageText(tree, director, settings, category, page, (*choice)->title_id(), (*choice)->state().label(),
                         nullptr, (*choice)->state().id()))
          return failed();
      const auto kind = PageItemFamily(item);
      if (!kind || !NativePages().adapters[static_cast<std::size_t>(*kind)].append(director, settings, category, item))
        return failed();
    }
  }
  return true;
}
void AppendSettingsPagesImpl(SettingsSectionDirector* director, SettingsContext* context)
{
  if (!PagesThread() || NativePages().pages.empty())
    return;
  for (const auto& existing : NativePages().trees)
    if (existing->settings.Get() == context && !existing->revoked)
      return; // Pending publication never authorizes a second tree.
  auto  context_root  = il2cpp::GcHandle::Strong(context);
  auto  director_root = il2cpp::GcHandle::Strong(director);
  auto  native_root   = il2cpp::GcHandle::Strong(context->RootOption());
  auto* parent = native_root ? native_root.Get<SettingsOptionContext>()->As<SettingsContainerContext>() : nullptr;
  if (!parent)
    return;
  const auto native_children = settings_pages_binding::Children(parent);
  if (!native_children)
    return;
  for (const auto& child : *native_children) {
    if (!child.Get())
      continue;
    const auto id = settings_pages_binding::LabelIdentifier(child.Get<SettingsOptionContext>());
    if (!id || il2cpp::Equals(id.value, NativePages().pages.front().id))
      return; // A foreign or unobserved matching root is never permission to publish a duplicate.
  }
  const auto staging = settings_pages_binding::NewStagingContainer();
  auto       stage   = il2cpp::GcHandle::Strong(staging ? staging.value : nullptr);
  if (!parent || !stage || !context_root || !director_root)
    return;
  auto tree      = std::make_shared<SettingsPageTree>();
  tree->settings = il2cpp::GcHandle::Weak(context);
  tree->director = il2cpp::GcHandle::Weak(director);
  tree->parent   = il2cpp::GcHandle::Weak(parent);
  if (!tree->settings || !tree->director || !tree->parent)
    return;
  NativePages().trees.push_back(tree); // Resolver custody covers off-screen Boolean model association too.
  struct StagingGuard {
    std::shared_ptr<SettingsPageTree> tree;
    ~StagingGuard()
    {
      if (tree->publication.state() == CategoryPublicationState::Idle)
        tree->revoked = true;
    }
  } staging_guard{tree};
  std::map<std::string, SettingsCategoryContext*> parents;
  il2cpp::GcHandle                                root;
  bool                                            staged = true;
  for (const auto& page : NativePages().pages) {
    auto* target   = page.parent.empty() ? stage.Get<SettingsContainerContext>() : parents.at(page.parent);
    auto* category = AddPageCategory(tree, director, reinterpret_cast<SettingsPageContext*>(context), target, page);
    if (!category) {
      staged = false;
      break;
    }
    if (!root)
      root = il2cpp::GcHandle::Strong(category);
    parents.emplace(page.id, category);
    if (!root
        || !PopulateSettingsPage(tree, director, reinterpret_cast<SettingsPageContext*>(context), category, page)) {
      staged = false;
      break;
    }
  }
  if (!staged || !root) {
    tree->revoked = true;
    return; // All partial appends belong only to the unreachable staging container.
  }
  auto*      category = root.Get<SettingsCategoryContext>();
  const auto detached =
      stage.Get<SettingsContainerContext>()->TryCall<"RemoveChild", void(SettingsOptionContext*)>(category);
  const auto detached_count  = PageChildOccurrences(stage.Get<SettingsContainerContext>(), category);
  const auto detached_parent = category->TryParent();
  const auto absent          = PageChildOccurrences(parent, category);
  const auto query           = category->TryQuery();
  if (!detached || !detached_count || *detached_count != 0 || !detached_parent || detached_parent.value || !absent
      || *absent != 0 || !query || !query.value || !tree->publication.Begin(parent, category, query.value)) {
    tree->revoked = true;
    return;
  }
  {
    struct PublicationReturn {
      CategoryPublication<il2cpp::GcHandle>& publication;
      bool                                   succeeded = false;
      ~PublicationReturn()
      { publication.Complete(succeeded); }
    } returned{tree->publication};
    tree->root         = il2cpp::GcHandle::Weak(category);
    returned.succeeded = parent->TryCall<"AddChild", void(SettingsOptionContext*)>(category).succeeded;
  }
  SettingsCategoryHost host;
  tree->publication.Retry(host);
  NativePages().refresh = true;
}
void AppendSettingsPages(SettingsSectionDirector* director, SettingsContext* context)
{
  try {
    AppendSettingsPagesImpl(director, context);
  } catch (...) {
    // AddGeneralSettings already returned. Staging failed without a visible partial tree; any publication
    // attempt retains its pre-call custody for frame cleanup even if C++ work failed afterwards.
    static bool warned = false;
    if (!std::exchange(warned, true))
      spdlog::warn("[NativeSettingsPages] page construction unavailable");
  }
}
void InvalidateSettingsPages()
{
  if (!PagesThread())
    return;
  ++NativePages().navigation;
  ClosePageVisit();
  std::set<const void*> seen;
  for (const auto& page : NativePages().pages)
    for (const auto& item : page.items)
      std::visit(
          [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, BooleanSetting*>) {
              if (seen.insert(value).second)
                value->InvalidateSession();
            } else if constexpr (std::is_same_v<T, ChoiceSetting*> || std::is_same_v<T, SliderSetting*>) {
              if (seen.insert(value).second)
                value->state().InvalidateSession();
            }
          },
          item);
  std::set<PageInvalidator> invalidators;
  for (const auto& adapter : NativePages().adapters)
    if (adapter.invalidate && invalidators.insert(adapter.invalidate).second)
      adapter.invalidate();
  RequestPageRefresh();
}
void ProcessSettingsPageNavigation()
{
  auto& state = NativePages();
  if (!state.pending)
    return;
  auto pending = std::move(*state.pending);
  state.pending.reset();
  auto source         = pending.from.lock();
  auto target         = il2cpp::GcHandle::Strong(pending.target.Get());
  auto controller     = il2cpp::GcHandle::Strong(source ? source->controller.Get() : nullptr);
  auto returning      = il2cpp::GcHandle::Strong(source ? source->context.Get() : nullptr);
  auto target_binding = target ? PageBinding(target.Get<SettingsOptionContext>()) : std::nullopt;
  if (!source || !IsPageVisitCurrent(pending.token) || !CurrentVisit(source) || !target_binding
      || target_binding->tree != source->tree || !controller || !returning)
    return;
  const auto selected =
      controller.Get<SettingsPageController>()->TryCall<"OnCategorySelected", void(SettingsContainerContext*)>(
          target.Get<SettingsContainerContext>());
  auto visit = state.visit;
  if (!selected || !CurrentVisit(visit) || visit->context.Get() != target.Get() || visit->tree != source->tree) {
    SetSettingsSearchError("The setting could not be opened in this Settings context.");
    return;
  }
  if (pending.return_to_current && returning.Get() != target.Get())
    visit->search_return = il2cpp::GcHandle::Weak(returning.Get());
  if (const auto* section = visit->page->SectionFor(pending.item); section && visit->sections.Collapsed(*section))
    visit->sections.Toggle(*section);
  visit->focus_item = std::move(pending.item);
  if (!RefreshPageVisit(visit) && CurrentVisit(visit))
    RepairCompletePageList(visit);
  state.refresh = true;
}
void FocusPageItem(const std::shared_ptr<SettingsPageVisit>& visit)
{
  if (!CurrentVisit(visit) || visit->focus_item.empty() || visit->refreshing || !visit->has_shown)
    return;
  auto controller = il2cpp::GcHandle::Strong(visit->controller.Get());
  auto context    = il2cpp::GcHandle::Strong(visit->context.Get());
  auto children   = context ? settings_pages_binding::Children(context.Get<SettingsContainerContext>()) : std::nullopt;
  if (!children || !controller)
    return;
  for (std::size_t i = 0; i < visit->shown.size(); ++i) {
    const auto found = std::ranges::find_if(*children, [&](const auto& row) { return row.Get() == visit->shown[i]; });
    if (found == children->end())
      continue;
    auto       row      = il2cpp::GcHandle::Strong(found->Get());
    const auto identity = row ? PageRowIdentity(row.Get<SettingsOptionContext>()) : std::nullopt;
    if (!identity || identity->id != visit->focus_item)
      continue;
    if (i > static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
      break;
    const auto focused = FocusSettingsSearchRow(controller.Get<SettingsPageController>(),
                                                row.Get<SettingsOptionContext>(), static_cast<int32_t>(i));
    if (!CurrentVisit(visit))
      return;
    if (focused == SettingsSearchFocus::Pending)
      return;
    if (focused == SettingsSearchFocus::Unavailable)
      SetSettingsSearchError("Page opened; native scrolling to this setting is unavailable.");
    visit->focus_item.clear();
    return;
  }
  visit->focus_item.clear();
  SetSettingsSearchError("Page opened; this setting is hidden by another setting or unavailable.");
}
void NativeSettingsPagesFrame()
{
  auto& state = NativePages();
  if (!PagesThread() || (state.trees.empty() && state.labels.empty() && !state.visit))
    return;
  ProcessSettingsPageNavigation();
  static Throttle lifecycle(std::chrono::milliseconds(250));
  if (!std::exchange(state.refresh, false) && !lifecycle.Due())
    return;
  SettingsCategoryHost host;
  const auto           trees = state.trees;
  for (const auto& tree : trees) {
    if (!tree->revoked)
      tree->publication.Retry(host);
    if (!tree->settings.Get() || tree->publication.state() == CategoryPublicationState::Retired)
      tree->revoked = true;
    if (tree->revoked && state.visit && state.visit->tree == tree)
      ClosePageVisit();
  }
  std::erase_if(state.trees, [](const auto& tree) { return tree->revoked; });
  auto visit = state.visit;
  if (visit && !CurrentVisit(visit))
    ClosePageVisit();
  else if (visit) {
    RefreshPageVisit(visit);
    FocusPageItem(visit);
  }
  std::vector<std::pair<il2cpp::Object*, std::shared_ptr<SettingsPageLabel>>> labels(state.labels.begin(),
                                                                                     state.labels.end());
  for (const auto& [key, view] : labels) {
    auto widget = il2cpp::GcHandle::Strong(view->widget.Get());
    if (!widget)
      ReleasePageLabel(key, view);
    else
      RenderPageWidget(widget.Get<il2cpp::Object>(), view->kind);
  }
}
bool InstallNativeSettingsPages()
{
  auto&      state = NativePages();
  const bool core  = std::ranges::any_of(FeatureInstallations(), [](const auto& feature) {
    return feature.name && std::string_view(feature.name) == "NativeSettings"
           && feature.state == InstallState::kInstalled;
  });
  if (!core) {
    spdlog::warn("[NativeSettingsPages] native Boolean controls unavailable");
    return false;
  }
  state.pages = BuildModPages().Build();
  AppendMaintenanceNotices(state.pages);
  for (const auto& page : state.pages)
    for (const auto& item : page.items) {
      const auto kind = PageItemFamily(item);
      if (kind && !state.adapters[static_cast<std::size_t>(*kind)].append) {
        spdlog::warn("[NativeSettingsPages] native control family unavailable");
        return false;
      }
    }
  const auto query = SettingsSectionDirector::Class::Method<int32_t()>("QueryShouldShowGenericPcSetting");
  const auto text  = SettingsSectionDirector::Class::Method<Il2CppString*()>("GetClientVersion");
  if (!SettingsPageContext::AddCategoryMethod() || !SettingsPageContext::AddTextMethod()
      || !SettingsUntypedWidget::BindMethod() || !SettingsSectionDirector::QualifiedQueryState()
      || !SettingsContainerContext::Class::Method<void()>(".ctor")
      || !SettingsContainerContext::Class::Method<void(SettingsOptionContext*)>("AddChild")
      || !SettingsContainerContext::Class::Method<void(SettingsOptionContext*)>("RemoveChild")
      || !SettingsCategoryWidget::Class::HasField<"_label", TextLocalizer*>()
      || !SettingsTextWidget::Class::HasField<"_label", TextLocalizer*>()
      || !SettingsPageController::Class::HasField<"_title", TextLocalizer*>()
      || !SettingsPageController::Class::HasField<"_optionTabPanel", SettingsOptionPanel*>()
      || !state.query.Initialize(query.Info(), PageQuery) || !state.text.Initialize(text.Info(), PageText))
    return false;
  const std::array entries{SettingsCategoryWidget::Class::Method<void()>("OnDidBindContext"),
                           SettingsCategoryWidget::Class::Method<void()>("OnAboutToReleaseContext"),
                           SettingsTextWidget::Class::Method<void()>("SetWidgetData"),
                           SettingsTextWidget::Class::Method<void()>("ClearWidgetData"),
                           SettingsPageController::Class::Method<void(SettingsContainerContext*)>("OnCategorySelected"),
                           SettingsPageController::Class::Method<void()>("OnDestroy")};
  const std::array owners{SettingsCategoryWidget::Class::Raw(), SettingsCategoryWidget::Class::Raw(),
                          SettingsTextWidget::Class::Raw(),     SettingsTextWidget::Class::Raw(),
                          SettingsPageController::Class::Raw(), SettingsPageController::Class::Raw()};
  const std::array core_entries{SettingsSectionDirector::Class::Method<void(SettingsContext*)>("AddGeneralSettings"),
                                ToggleOptionWidget::Class::Method<void()>("SetWidgetData"),
                                ToggleOptionWidget::Class::Method<void(bool)>("OnToggleValueChanged"),
                                ToggleOptionWidget::Class::Method<void()>("OnAboutToReleaseContext"),
                                PersistentPrefsManager::Class::Method<void()>("RegisterEvents"),
                                PersistentPrefsManager::Class::Method<void()>("GameSessionStartedEventHandler"),
                                PersistentPrefsManager::Class::Method<void()>("LoadPersistentPrefsFromCloud")};
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i] || entries[i].Info()->klass != owners[i]) {
      spdlog::warn("[NativeSettingsPages] required native navigation override unavailable");
      return false;
    }
    if (std::ranges::any_of(core_entries, [&](const auto& entry) { return entry.Pointer() == entries[i].Pointer(); })) {
      spdlog::warn("[NativeSettingsPages] native navigation entry overlaps the Boolean control owner");
      return false;
    }
    for (std::size_t j = 0; j < i; ++j)
      if (entries[j].Pointer() == entries[i].Pointer()) {
        spdlog::warn("[NativeSettingsPages] native navigation entries share a target");
        return false;
      }
  }
  il2cpp::HookSet hooks("NativeSettingsPages");
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsCategoryWidget::Class::Method<void()>("OnDidBindContext")),
                  SettingsCategoryWidget_OnDidBindContext_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsCategoryWidget::Class::Method<void()>("OnAboutToReleaseContext")),
                  SettingsCategoryWidget_OnAboutToReleaseContext_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsTextWidget::Class::Method<void()>("SetWidgetData")),
                  SettingsTextWidget_SetWidgetData_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsTextWidget::Class::Method<void()>("ClearWidgetData")),
                  SettingsTextWidget_ClearWidgetData_Hook);
  IL2CPP_STAGE_AT(hooks,
                  il2cpp::CheckedEntry(
                      SettingsPageController::Class::Method<void(SettingsContainerContext*)>("OnCategorySelected")),
                  SettingsPageController_OnCategorySelected_Hook);
  IL2CPP_STAGE_AT(hooks, il2cpp::CheckedEntry(SettingsPageController::Class::Method<void()>("OnDestroy")),
                  SettingsPageController_OnDestroy_Hook);
  if (!RegisterPageAppender(AppendSettingsPages) || !RegisterBooleanResolver(ResolvePageBoolean)
      || !RegisterPageInvalidator(InvalidateSettingsPages) || !hooks.Commit())
    return false;
  for (const auto& page : state.pages)
    for (auto* setting : page.Controls<BooleanSetting>())
      setting->SetChangeObserver(RequestPageRefresh);
  state.thread = std::this_thread::get_id();
  state.active = true;
  return true;
}
} // namespace

bool mod_settings::native::RegisterPageItemAdapter(PageItemKind kind, PageItemAppender append, PageItemResolver resolve,
                                                   PageInvalidator invalidate)
{
  const auto index = static_cast<std::size_t>(kind);
  if (index >= NativePages().adapters.size() || !append || !resolve || NativePages().active
      || NativePages().adapters[index].append)
    return false;
  NativePages().adapters[index] = {append, resolve, invalidate};
  return true;
}
void mod_settings::native::RequestPageRefresh()
{ NativePages().refresh = true; }
mod_settings::native::PageVisitToken mod_settings::native::CurrentPageVisit()
{
  if (!PagesThread())
    return 0;
  const auto visit = NativePages().visit;
  const auto token = NativePages().navigation;
  return CurrentVisit(visit) && NativePages().navigation == token ? token : 0;
}
bool mod_settings::native::IsPageVisitCurrent(PageVisitToken token)
{ return token != 0 && token == CurrentPageVisit(); }
bool mod_settings::native::PageRefreshInProgress()
{ return PagesThread() && NativePages().visit && NativePages().visit->refreshing; }
bool mod_settings::native::RegisterPageVisitObserver(PageVisitObserver observer)
{
  if (!observer || NativePages().observer)
    return false;
  NativePages().observer = observer;
  return true;
}
std::span<const PageCatalog::Page> mod_settings::native::NativeSettingsPages()
{
  return PagesThread() ? std::span<const PageCatalog::Page>(NativePages().pages) : std::span<const PageCatalog::Page>{};
}
bool mod_settings::native::CurrentSettingsPage(SettingsPageController*& controller, SettingsContainerContext*& context,
                                               std::string_view& page)
{
  controller = nullptr;
  context    = nullptr;
  page       = {};
  auto visit = NativePages().visit;
  if (!PagesThread() || !CurrentVisit(visit))
    return false;
  controller = visit->controller.Get<SettingsPageController>();
  context    = visit->context.Get<SettingsContainerContext>();
  page       = visit->page->id;
  return true;
}
bool mod_settings::native::QueueSettingsPage(std::string_view page, std::string_view item, bool return_to_current)
{
  auto       visit = NativePages().visit;
  const auto token = CurrentPageVisit();
  if (!token || !visit || visit->refreshing || (return_to_current && visit->page->id != NativePages().pages.front().id))
    return false;
  for (const auto& binding : visit->tree->bindings) {
    if (!binding.category || binding.heading || !binding.page || binding.page->id != page)
      continue;
    auto target   = il2cpp::GcHandle::Strong(binding.context.Get());
    auto observed = target ? PageBinding(target.Get<SettingsOptionContext>()) : std::nullopt;
    if (!observed || observed->tree != visit->tree || !IsPageVisitCurrent(token))
      return false;
    SettingsPageNavigation pending;
    pending.from              = visit;
    pending.target            = il2cpp::GcHandle::Weak(target.Get());
    pending.item              = item;
    pending.token             = token;
    pending.return_to_current = return_to_current;
    if (!pending.target)
      return false;
    NativePages().pending = std::move(pending);
    return true;
  }
  return false;
}
mod_settings::native::PageBindResult mod_settings::native::BindPageRows(SettingsPageController*   controller,
                                                                        SettingsContainerContext* expected,
                                                                        std::span<SettingsOptionContext* const> rows)
{
  if (!controller || !expected)
    return PageBindResult::Unavailable;
  auto controller_root = il2cpp::GcHandle::Strong(controller);
  auto context_root    = il2cpp::GcHandle::Strong(expected);
  if (!controller_root || !context_root)
    return PageBindResult::Unavailable;
  const auto settings      = controller->TrySettings();
  auto       settings_root = il2cpp::GcHandle::Strong(settings ? settings.value : nullptr);
  const auto selected      = settings_root ? settings_root.Get<SettingsPageContext>()->TrySelected()
                                           : il2cpp::CallResult<SettingsContainerContext*>{};
  auto       panel         = il2cpp::GcHandle::Strong(controller->Panel());
  if (!selected || !panel)
    return PageBindResult::Unavailable;
  if (selected.value != expected)
    return PageBindResult::Stale;
  std::vector<il2cpp::GcHandle> row_roots;
  row_roots.reserve(rows.size());
  for (auto* row : rows) {
    auto root = il2cpp::GcHandle::Strong(row);
    if (row && !root)
      return PageBindResult::Unavailable;
    row_roots.push_back(std::move(root));
  }
  auto array = il2cpp::GcHandle::Strong(
      il2cpp::Array<SettingsOptionContext*>::New(SettingsOptionContext::Class::Raw(), rows.size()));
  if (!array)
    return PageBindResult::Unavailable;
  for (std::size_t i = 0; i < rows.size(); ++i)
    array.Get<il2cpp::Array<SettingsOptionContext*>>()->Set(i, rows[i]);
  const auto current = controller->TrySettings();
  const auto reselected =
      current && current.value ? current.value->TrySelected() : il2cpp::CallResult<SettingsContainerContext*>{};
  if (!current || !reselected)
    return PageBindResult::Unavailable;
  if (current.value != settings_root.Get() || reselected.value != expected || controller->Panel() != panel.Get())
    return PageBindResult::Stale;
  const auto bound = panel.Get<SettingsOptionPanel>()->TryBind(array.Get<il2cpp::Array<SettingsOptionContext*>>());
  const auto after = controller->TrySettings();
  const auto after_selected =
      after && after.value ? after.value->TrySelected() : il2cpp::CallResult<SettingsContainerContext*>{};
  if (!after || !after_selected)
    return PageBindResult::Unavailable;
  if (after.value != settings_root.Get() || after_selected.value != expected || controller->Panel() != panel.Get())
    return PageBindResult::Stale;
  return bound ? PageBindResult::Bound : PageBindResult::Unavailable;
}

const Feature kNativeSettingsPages{"NativeSettingsPages", &Config::Patches::nativesettingshooks,
                                   InstallNativeSettingsPages, NativeSettingsPagesFrame};
