#pragma once

// Production feature and checked game bindings against the existing controlled
// IL2CPP runtime. Only Config, disk-state admission and native widget bodies are
// substituted. This is not an in-game or platform-runtime qualification.
#include "../src/mod/features/officer_presets.cc"

namespace
{
std::optional<nlohmann::json>       preset_test_disk;
mod_state::SaveStatus               preset_test_receipt;
bool                                preset_test_admit = true;
std::vector<std::int64_t>           preset_test_submitted;
std::deque<std::unique_ptr<char[]>> preset_test_arrays;
int                                 preset_test_rebuilds     = 0;
bool                                preset_test_bad_renderer = false, preset_test_defer = false;

struct PresetNativeItem {
  Il2CppObject               header;
  OfficerPresetsViewContext* owner;
  std::int64_t               slot;
  std::int32_t               order, presentation;
  Il2CppString*              name;
  il2cpp::ArrayObject*       crew;
};
struct PresetNativeList {
  Il2CppObject         header;
  il2cpp::ArrayObject* items;
  std::int32_t         size;
};
struct PresetNativeContext {
  Il2CppObject              header;
  il2cpp::Object*           list;
  OfficerPresetItemContext* cached = nullptr;
};
struct PresetNativeWidget {
  Il2CppObject              header;
  OfficerPresetItemContext* context;
};
struct PresetNativeScroller {
  Il2CppObject                      header;
  il2cpp::Object*                   source       = nullptr;
  bool                              regenerating = false;
  il2cpp::Object*                   initializing = nullptr;
  float                             position     = 0.37f;
  std::array<PresetNativeWidget, 4> widgets;
};
struct PresetNativeController {
  Il2CppObject               header;
  OfficerPresetsViewContext* context;
  PresetSmartScroller*       scroller;
};
FakeClass *preset_item_class, *preset_widget_class, *preset_array_class;

Il2CppArray* PresetArray(Il2CppClass* element, il2cpp_array_size_t count)
{
  auto& type                     = Define("Fixture", "PresetVector", IL2CPP_TYPE_SZARRAY, &array_class);
  type.klass.byval_arg.data.type = &element->byval_arg;
  auto& storage =
      preset_test_arrays.emplace_back(std::make_unique<char[]>(sizeof(Il2CppArraySize) + count * sizeof(void*)));
  auto* array       = reinterpret_cast<Il2CppArraySize*>(storage.get());
  array->klass      = &type.klass;
  array->max_length = count;
  return reinterpret_cast<Il2CppArray*>(array);
}
bool PresetLookup(PresetSmartScroller* object, std::int32_t index, il2cpp::Object** receiver)
{
  auto& scroll = *reinterpret_cast<PresetNativeScroller*>(object);
  *receiver    = index >= 0 && index < 4 ? reinterpret_cast<il2cpp::Object*>(&scroll.widgets[index]) : nullptr;
  return *receiver;
}
bool PresetNativeLookup(PresetSmartScroller* object, std::int32_t index, il2cpp::Object** receiver, const MethodInfo*)
{ return PresetLookup(object, index, receiver); }
void PresetRebuild(PresetSmartScroller* object, il2cpp::Object*, il2cpp::Object* source, const MethodInfo*)
{
  auto& scroll = *reinterpret_cast<PresetNativeScroller*>(object);
  ++preset_test_rebuilds;
  if (preset_test_defer) {
    // Actual captured first-open shape: native bind holds data until Start,
    // with no adopted source or spawned receiver map yet.
    scroll.initializing = source;
    return;
  }
  scroll.source       = source;
  scroll.regenerating = false;
  scroll.initializing = nullptr;
  auto* items         = source;
  if (source->klass->byval_arg.type != IL2CPP_TYPE_SZARRAY)
    items = reinterpret_cast<il2cpp::Object*>(reinterpret_cast<PresetNativeList*>(source)->items);
  auto* array = reinterpret_cast<il2cpp::Array<OfficerPresetItemContext*>*>(items);
  for (std::size_t index = 0; index < 4; ++index) {
    scroll.widgets[index].header.klass = &preset_widget_class->klass;
    scroll.widgets[index].context      = (*array)[preset_test_bad_renderer ? 0 : index];
  }
}
float PresetScroll(PresetSmartScroller* object, const MethodInfo*)
{ return reinterpret_cast<PresetNativeScroller*>(object)->position; }
void PresetRestoreScroll(PresetSmartScroller* object, float value, const MethodInfo*)
{ reinterpret_cast<PresetNativeScroller*>(object)->position = value; }

void PresetNativeBind(OfficerPresetsViewController* owner)
{
  auto& controller = *reinterpret_cast<PresetNativeController*>(owner);
  auto& context    = *reinterpret_cast<PresetNativeContext*>(controller.context);
  PresetRebuild(controller.scroller, owner, context.list, nullptr);
}

void DefinePresetRuntime()
{
  auto& item    = Define("Digit.Prime.OfficerPresets", "OfficerPresetItemContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& context = Define("Digit.Prime.OfficerPresets", "OfficerPresetsViewContext", IL2CPP_TYPE_CLASS, &object_class);
  auto& controller =
      Define("Digit.Prime.OfficerPresets", "OfficerPresetsViewController", IL2CPP_TYPE_CLASS, &object_class);
  auto& widget = Define("Digit.Prime.OfficerPresets", "OfficerPresetItemWidget", IL2CPP_TYPE_CLASS, &object_class);
  auto& scroll = Define("Digit.Client.UI", "SmartScrollerBase", IL2CPP_TYPE_CLASS, &object_class);
  auto& list   = Define("Fixture", "PresetList", IL2CPP_TYPE_CLASS, &object_class);
  auto& presentation   = Define("Fixture", "PresetPresentation", IL2CPP_TYPE_VALUETYPE, &object_class, 4);
  presentation.is_enum = true;
  preset_item_class    = &item;
  preset_widget_class  = &widget;
  preset_array_class   = &list;
  AddField(item, "_officerPresetsViewContext", context, offsetof(PresetNativeItem, owner));
  AddField(item, "SlotId", long_class, offsetof(PresetNativeItem, slot));
  AddField(item, "OrderId", int_class, offsetof(PresetNativeItem, order));
  AddField(item, "Presentation", presentation, offsetof(PresetNativeItem, presentation));
  AddField(item, "PresetName", string_class, offsetof(PresetNativeItem, name));
  AddField(item, "Officers", array_class, offsetof(PresetNativeItem, crew));
  AddField(context, "PresetsItemsContext", object_class, offsetof(PresetNativeContext, list));
  AddField(context, "CurrentCachedItemContext", item, offsetof(PresetNativeContext, cached));
  AddField(controller, "m_context", context, offsetof(PresetNativeController, context));
  AddField(controller, "_smartScroller", scroll, offsetof(PresetNativeController, scroller));
  AddField(widget, "m_context", item, offsetof(PresetNativeWidget, context));
  AddField(scroll, "_data", object_class, offsetof(PresetNativeScroller, source));
  AddField(scroll, "_regeneratingContent", bool_class, offsetof(PresetNativeScroller, regenerating));
  AddField(scroll, "_dataHeldWhilstInitializing", object_class, offsetof(PresetNativeScroller, initializing));
  AddField(list, "_items", array_class, offsetof(PresetNativeList, items));
  AddField(list, "_size", int_class, offsetof(PresetNativeList, size));
  AddMethod(scroll, "TryGetInstanceForIndex", reinterpret_cast<void*>(&PresetNativeLookup), T(bool_class),
            {T(int_class), Ref(object_class)});
  AddMethod(scroll, "ClearAndGenerateContents", reinterpret_cast<void*>(&PresetRebuild), T(void_class),
            {T(object_class), T(object_class)});
  AddMethod(scroll, "get_ScrollPosition", reinterpret_cast<void*>(&PresetScroll), T(float_class), {});
  AddMethod(scroll, "RestoreScrollPosition", reinterpret_cast<void*>(&PresetRestoreScroll), T(void_class),
            {T(float_class)});
  Require(preset_fields.Resolve(), "all typed preset fields preflight");
}

void OfficerPresetNativeObservations()
{
  DefinePresetRuntime();
  const auto roots_before                          = warp_roots.size();
  Config::Get().ui.allow_officer_preset_reordering = true;
  PresetNativeContext context{{OfficerPresetsViewContext::Class::Raw(), nullptr}, nullptr};
  auto*               ctx  = reinterpret_cast<OfficerPresetsViewContext*>(&context);
  auto*               crew = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(&object_class.klass, 1));
  crew->Set(0, &base_object.header);
  auto* empty_crew = reinterpret_cast<il2cpp::ArrayObject*>(PresetArray(&object_class.klass, 1));
  std::array<PresetNativeItem, 4> items{};
  auto* array = reinterpret_cast<il2cpp::Array<OfficerPresetItemContext*>*>(PresetArray(&preset_item_class->klass, 4));
  const std::array<std::int64_t, 4> ids{0, 77, 9007199254740993LL, 9};
  for (int index = 0; index < 4; ++index) {
    items[index] = {{&preset_item_class->klass, nullptr},
                    ctx,
                    ids[index],
                    index,
                    10 + index,
                    ValuesString(index == 1 ? "" : "crew", index == 1 ? 0 : 4),
                    index == 1 ? empty_crew : reinterpret_cast<il2cpp::ArrayObject*>(crew)};
    array->Set(index, reinterpret_cast<OfficerPresetItemContext*>(&items[index]));
  }
  PresetNativeList list{{&preset_array_class->klass, nullptr}, reinterpret_cast<il2cpp::ArrayObject*>(array), 4};
  context.list = reinterpret_cast<il2cpp::Object*>(&list);
  PresetNativeScroller scroll;
  scroll.header.klass = PresetSmartScroller::Class::Raw();
  scroll.source       = nullptr;
  PresetNativeController controller{
      {OfficerPresetsViewController::Class::Raw(), nullptr}, ctx, reinterpret_cast<PresetSmartScroller*>(&scroll)};
  auto* owner      = reinterpret_cast<OfficerPresetsViewController*>(&controller);
  auto* scroller   = reinterpret_cast<PresetSmartScroller*>(&scroll);
  preset_test_disk = nlohmann::json{{"officer_preset_order", {"9007199254740993", "0", "9"}}};
  LoadPresetOrder();
  preset_test_defer = true;
  OfficerPresetsViewController_OnDidBindCanvasContext_Hook(PresetNativeBind, owner);
  auto       view              = active_preset_view;
  const auto native_bind_calls = preset_test_rebuilds;
  for (int frame = 0; frame < 3; ++frame)
    PresetFrame();
  Require(view && view->boundary.State() == preset_boundary::Phase::Native && !view->native_ready && !scroll.source
              && scroll.initializing == context.list && preset_test_rebuilds == native_bind_calls,
          "first bind/frames leave native null source and held list untouched before Start");
  for (int index = 0; index < 4; ++index)
    Require(items[index].presentation == 10 + index, "no presentation writes before native initialization");
  OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook(
      [&](auto*) {
        Require(!scroll.source && scroll.initializing == context.list,
                "close before native Start leaves the uninitialized native source untouched");
      },
      owner);
  view.reset();
  Require(preset_views.empty() && !active_preset_view && warp_roots.size() == roots_before,
          "closing an initial pending bind disposes feature custody without a repair");
  OfficerPresetsViewController_OnDidBindCanvasContext_Hook(PresetNativeBind, owner);
  view = active_preset_view;
  // Native Start/retrigger now adopts its own canonical list and creates the
  // receiver map. The feature may project only after observing that completion.
  preset_test_defer = false;
  PresetNativeBind(owner);
  PresetFrame();
  Require(view && view->boundary.State() == preset_boundary::Phase::Projected,
          "production bind roots and projects a canonical typed list");
  Require(scroll.widgets[0].context == reinterpret_cast<OfficerPresetItemContext*>(&items[2])
              && scroll.widgets[1].context == reinterpret_cast<OfficerPresetItemContext*>(&items[1]),
          "saved exact IDs apply; empty slots keep their native positions");

  auto replacement         = items[2];
  replacement.presentation = 12;
  int saves = 0, binds = 0;
  OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(
      [&](auto*, bool increase) {
        ++saves;
        Require(increase && scroll.source == context.list, "native original sees canonical source exactly once");
        array->Set(2, reinterpret_cast<OfficerPresetItemContext*>(&replacement));
        il2cpp::Object* receiver = nullptr;
        Require(PresetSmartScroller_TryGetInstanceForIndex_Hook(PresetLookup, scroller, 2, &receiver),
                "native canonical index resolves through qualified display mapping");
        Require(receiver == reinterpret_cast<il2cpp::Object*>(&scroll.widgets[0]), "correct visible receiver selected");
        reinterpret_cast<PresetNativeWidget*>(receiver)->context =
            reinterpret_cast<OfficerPresetItemContext*>(&replacement);
        ++binds;
      },
      owner, true);
  Require(saves == 1 && binds == 1 && scroll.source != context.list && replacement.presentation == 10,
          "native replacement reaches display array and destination presentation; projection restored");
  Require((*reinterpret_cast<il2cpp::Array<OfficerPresetItemContext*>*>(scroll.source))[0]
              == reinterpret_cast<OfficerPresetItemContext*>(&replacement),
          "replacement source relationship retained");

  int   renames  = 0;
  auto* selected = reinterpret_cast<OfficerPresetItemWidget*>(&scroll.widgets[0]);
  held_keys.fill(false);
  OfficerPresetItemWidget_OnEditNameButtonClicked_Hook([&](auto*) { ++renames; }, selected);
  Require(renames == 1, "ordinary edit remains native rename");
  held_keys[std::to_underlying(unity::KeyCode::LeftShift)] = true;
  OfficerPresetItemWidget_OnEditNameButtonClicked_Hook([&](auto*) { ++renames; }, selected);
  Require(renames == 1 && !preset_save_pending, "top-boundary Shift edit consumes native rename without a disk write");
  held_keys.fill(false);
#if __APPLE__
  held_keys[std::to_underlying(unity::KeyCode::LeftCommand)] = true;
#else
  held_keys[std::to_underlying(unity::KeyCode::LeftControl)] = true;
#endif
  OfficerPresetItemWidget_OnEditNameButtonClicked_Hook([&](auto*) { ++renames; }, selected);
  Require(renames == 1 && preset_save_pending && preset_test_submitted[0] == 0,
          "down edit skips empty row, accepts local move and requests state save separately");
  PresetFrame();
  Require(preset_save_pending, "queue admission is not a saved receipt");
  preset_test_receipt.outcome = mod_state::SaveOutcome::Failed;
  PresetFrame();
  Require(!preset_save_pending && preset_session_order[0] == 0, "failed save keeps accepted session order truthful");

  // A malformed receiver map must not be certified by successful invocation.
  preset_test_bad_renderer = true;
  Require(!RenderPresetOrder(view, false) && view->boundary.State() == preset_boundary::Phase::Restore,
          "failed renderer qualification schedules repair outside the rebuild stack");
  PresetFrame();
  Require(scroll.source == context.list && view->repair_pending,
          "wrong renderer after rebuild requires canonical repair, not a projected readiness claim");
  preset_test_bad_renderer = false;
  RepairPresetView(view);
  Require(view->boundary.State() == preset_boundary::Phase::Native, "canonical repair verifies source and receivers");
  Require(RenderPresetOrder(view, false), "reprojection after repair");
  // A later native rebuild may also be asynchronous. Keep its candidate rooted
  // and wait; don't replace/rebuild underneath pending initialization.
  preset_test_defer         = true;
  const auto before_pending = preset_test_rebuilds;
  Require(RenderPresetOrder(view, true) && view->boundary.State() == preset_boundary::Phase::Transition,
          "pending native rebuild is not a failed projection");
  auto* pending_source        = scroll.initializing;
  auto* source_during_pending = scroll.source;
  PresetFrame();
  PresetFrame();
  Require(preset_test_rebuilds == before_pending + 1 && scroll.source == source_during_pending
              && scroll.initializing == pending_source,
          "waiting frames do not clear/force a pending native source");
  preset_test_defer = false;
  PresetRebuild(scroller, owner, pending_source, nullptr);
  PresetFrame();
  Require(view->boundary.State() == preset_boundary::Phase::Projected && !view->pending_ticket,
          "completed native source/receiver adoption finalizes the same pending projection");
  // A nested save and an escaping exception must preserve original call counts
  // and the enclosing canonical source, not run a save again as a fallback.
  int nested = 0;
  OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(
      [&](auto*, bool) {
        ++nested;
        OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(
            [&](auto*, bool) {
              ++nested;
              Require(scroll.source == context.list, "nested original stays canonical");
            },
            owner, false);
        Require(scroll.source == context.list, "nested return preserves outer canonical scope");
        const auto before_nested_frame = preset_test_rebuilds;
        PresetFrame();
        Require(scroll.source == context.list && view->projection_requested
                    && preset_test_rebuilds == before_nested_frame,
                "reentrant frame cannot consume post-save work while the enclosing save owns canonical data");
      },
      owner, false);
  Require(nested == 2 && scroll.source != context.list, "nested originals each once, outer restores projection");
  struct SaveFailure {
  };
  bool escaped = false;
  try {
    OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(
        [&](auto*, bool) {
          ++nested;
          throw SaveFailure{};
        },
        owner, false);
  } catch (const SaveFailure&) {
    escaped = true;
    Require(nested == 3 && scroll.source != context.list && preset_saving == nullptr,
            "exception restores source and stack without retrying original");
  }
  Require(escaped, "original exception must escape unchanged");
  // Turning the setting off cannot bypass an already-owned save guard.
  Config::Get().ui.allow_officer_preset_reordering = false;
  OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(
      [&](auto*, bool) { Require(scroll.source == context.list, "disable never bypasses canonical save handoff"); },
      owner, false);
  PresetFrame();
  Require(scroll.source == context.list && view->boundary.State() == preset_boundary::Phase::Native,
          "live disable repairs the renderer and returns native data");
  Config::Get().ui.allow_officer_preset_reordering = true;
  Require(RenderPresetOrder(view, false), "re-enable can project again");

  // Same-source rebind inside a save creates a new generation. The old scope
  // must not restore its old display array into the newer bound view.
  auto* before_rebind = scroll.source;
  OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(
      [&](auto*, bool) { OfficerPresetsViewController_OnDidBindCanvasContext_Hook(PresetNativeBind, owner); }, owner,
      false);
  PresetFrame();
  auto new_view = active_preset_view;
  Require(new_view && new_view != view && scroll.source != before_rebind
              && new_view->boundary.State() == preset_boundary::Phase::Projected,
          "same-source rebind rejects stale save restoration");
  view.reset();
  // Closing from within calculation/save leaves the native list in place and
  // roots alive until the active scope has returned.
  OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(
      [&](auto*, bool) {
        OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook([](auto*) {}, owner);
        Require(scroll.source == context.list && !active_preset_view, "close during save cannot republish display");
      },
      owner, false);
  Require(scroll.source == context.list && preset_views.empty(), "closed source remains canonical after scope exit");
  new_view.reset();

  OfficerPresetsViewController_OnDidBindCanvasContext_Hook(PresetNativeBind, owner);
  PresetFrame();
  view = active_preset_view;
  OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook(
      [&](auto*) {
        Require(scroll.source == context.list && replacement.presentation == 12,
                "release restores canonical source and annotations");
      },
      owner);
  view.reset();
  Require(!active_preset_view && preset_views.empty() && warp_roots.size() == roots_before,
          "release disposes native roots; no abandoned projection ownership");
  held_keys.fill(false);
  std::cout
      << "Officer preset production adapter: native Start wait/early close, async adoption, bind, exact order, fixed "
         "empty rows, native replacement/receiver, chords, "
         "receipts, repair, nested/throwing saves, disable, same-source rebind, close during save and release passed\n";
}
// Saving a previously empty slot changes the sortable membership, not just its
// text. Exercise that independently from the earlier rename/replacement trial.
void OfficerPresetSaveObservations(bool create)
{
  DefinePresetRuntime();
  const auto roots_before                          = warp_roots.size();
  Config::Get().ui.allow_officer_preset_reordering = true;
  PresetNativeContext context{{OfficerPresetsViewContext::Class::Raw(), nullptr}, nullptr};
  auto*               ctx  = reinterpret_cast<OfficerPresetsViewContext*>(&context);
  auto*               crew = reinterpret_cast<il2cpp::Array<Il2CppObject*>*>(PresetArray(&object_class.klass, 1));
  crew->Set(0, &base_object.header);
  auto*                           empty = reinterpret_cast<il2cpp::ArrayObject*>(PresetArray(&object_class.klass, 1));
  std::array<PresetNativeItem, 4> items{};
  auto* array = reinterpret_cast<il2cpp::Array<OfficerPresetItemContext*>*>(PresetArray(&preset_item_class->klass, 4));
  const std::array<std::int64_t, 4> ids{10, 55, 30, 40};
  // Retained native PresentationType: FirstItem=0, MiddleItem=1, LastItem=2.
  const std::array<std::int32_t, 4> presentations{0, 1, 1, 2};
  for (int i = 0; i < 4; ++i) {
    items[i] = {{&preset_item_class->klass, nullptr},
                ctx,
                ids[i],
                i,
                presentations[i],
                ValuesString(i == 1 ? "" : "crew", i == 1 ? 0 : 4),
                i == 1 ? empty : reinterpret_cast<il2cpp::ArrayObject*>(crew)};
    array->Set(i, reinterpret_cast<OfficerPresetItemContext*>(&items[i]));
  }
  PresetNativeList list{{&preset_array_class->klass, nullptr}, reinterpret_cast<il2cpp::ArrayObject*>(array), 4};
  context.list = reinterpret_cast<il2cpp::Object*>(&list);
  PresetNativeScroller scroll;
  scroll.header.klass = PresetSmartScroller::Class::Raw();
  PresetNativeController controller{
      {OfficerPresetsViewController::Class::Raw(), nullptr}, ctx, reinterpret_cast<PresetSmartScroller*>(&scroll)};
  auto* owner          = reinterpret_cast<OfficerPresetsViewController*>(&controller);
  preset_session_order = {30, 10, 40};
  OfficerPresetsViewController_OnDidBindCanvasContext_Hook(PresetNativeBind, owner);
  PresetFrame();
  auto view = active_preset_view;
  Require(view && view->boundary.State() == preset_boundary::Phase::Projected, "save trial starts projected");
  const int index       = create ? 1 : 0;
  auto      replacement = items[index];
  replacement.slot      = create ? 50 : ids[index];
  replacement.name      = ValuesString("saved", 5);
  replacement.crew      = reinterpret_cast<il2cpp::ArrayObject*>(crew);
  // The retained native save body generates MiddleItem even for an edge row.
  replacement.presentation = 1;
  int        calls         = 0;
  const auto before_save   = preset_test_rebuilds;
  OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(
      [&](auto*, bool increase) {
        ++calls;
        Require(increase == create && scroll.source == context.list, "original save has canonical data once");
        array->Set(index, reinterpret_cast<OfficerPresetItemContext*>(&replacement));
        il2cpp::Object* receiver = nullptr;
        Require(PresetSmartScroller_TryGetInstanceForIndex_Hook(PresetLookup, controller.scroller, index, &receiver),
                "native replacement binds its correct existing receiver");
        reinterpret_cast<PresetNativeWidget*>(receiver)->context =
            reinterpret_cast<OfficerPresetItemContext*>(&replacement);
      },
      owner, create);
  Require(calls == 1 && preset_test_rebuilds == before_save, "no nested rebuild or repeated native save");
  preset_test_defer = create;
  PresetFrame();
  if (create) {
    Require(view->boundary.State() == preset_boundary::Phase::Transition,
            "post-save native rebuild may complete asynchronously");
    auto* pending = scroll.initializing;
    PresetFrame();
    PresetFrame();
    Require(scroll.initializing == pending && preset_test_rebuilds == before_save + 1,
            "post-save reconciliation requests one rebuild and waits for native completion");
    preset_test_defer = false;
    PresetRebuild(controller.scroller, owner, pending, nullptr);
    PresetFrame();
    const std::array<std::size_t, 4> expected{2, 0, 3, 1};
    for (std::size_t i = 0; i < expected.size(); ++i)
      Require(scroll.widgets[i].context == (*array)[expected[i]],
              "newly saved crew is reconciled into the local order without closing the screen");
    Require(view->slots[1] == 50 && !view->empty[1], "new saved identity replaces the qualified empty-slot header");
    held_keys.fill(false);
    held_keys[std::to_underlying(unity::KeyCode::LeftShift)] = true;
    int renames                                              = 0;
    OfficerPresetItemWidget_OnEditNameButtonClicked_Hook(
        [&](auto*) { ++renames; }, reinterpret_cast<OfficerPresetItemWidget*>(&scroll.widgets[3]));
    Require(renames == 0 && scroll.widgets[2].context == reinterpret_cast<OfficerPresetItemContext*>(&replacement),
            "new crew can move immediately; reorder consumes native edit");
    held_keys.fill(false);
  }
  OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook([](auto*) {}, owner);
  if (!create)
    Require(replacement.presentation == 0,
            "save preserves canonical FirstItem instead of adopting generated MiddleItem");
  for (int i = 0; i < 4; ++i) {
    auto* item = (*array)[i];
    Require(preset_fields.order.Read(item) == i && preset_fields.slot.Read(item) == (create && i == 1 ? 50 : ids[i]),
            "display reconciliation never rewrites canonical slots or order IDs");
    Require(preset_fields.presentation.Read(item) == presentations[i], "native positional presentations restored");
  }
  view.reset();
  Require(!active_preset_view && preset_views.empty() && warp_roots.size() == roots_before,
          "save trial releases custody");
  std::cout << "Preset save " << (create ? "new-crew reconciliation" : "native edge presentation") << " passed\n";
}
} // namespace

namespace
{
Config*                 bindings_config_override = nullptr;
bool                    bindings_replace_config  = false;
std::unique_ptr<Config> bindings_replacement_config;
} // namespace
Config& Config::Get()
{
  static Config config;
  if (bindings_replace_config) {
    bindings_replace_config     = false;
    auto        next            = std::unique_ptr<Config>(new Config);
    const auto& current         = bindings_config_override ? *bindings_config_override : config;
    next->graphics              = current.graphics;
    next->control               = current.control;
    bindings_replacement_config = std::move(next);
    bindings_config_override    = bindings_replacement_config.get();
  }
  return bindings_config_override ? *bindings_config_override : config;
}
namespace mod_state
{
std::optional<nlohmann::json> Read()
{ return preset_test_disk; }
bool PrepareAsync()
{ return true; }
std::uint64_t QueueOfficerOrder(const std::vector<std::int64_t>& order)
{
  if (!preset_test_admit)
    return 0;
  preset_test_submitted = order;
  preset_test_receipt   = {preset_test_receipt.revision + 1, SaveOutcome::Pending};
  return preset_test_receipt.revision;
}
SaveStatus Status(Group)
{ return preset_test_receipt; }
} // namespace mod_state

API(Il2CppArray*, il2cpp_array_new, (Il2CppClass * element, il2cpp_array_size_t count))
{ return PresetArray(element, count); }
END_API
API(void, il2cpp_gc_wbarrier_set_field, (Il2CppObject*, void** destination, void* value))
{ *destination = value; }
END_API
