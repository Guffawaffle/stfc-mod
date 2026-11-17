#pragma once

// Actual post-bind Settings value callbacks in the existing controlled runtime.
// Native page publication/rendering and the game process are not exercised.
#include "../src/mod/features/settings_values.cc"
#include "settings/native_controls.h"

namespace
{
uint64_t settings_consumer_visit        = 73;
int      settings_consumer_refreshes    = 0;
bool     settings_consumer_page_refresh = false;
} // namespace
namespace mod_settings::native
{
PageVisitToken CurrentPageVisit()
{ return settings_consumer_visit; }
bool IsPageVisitCurrent(PageVisitToken token)
{ return token == settings_consumer_visit; }
bool PageRefreshInProgress()
{ return settings_consumer_page_refresh; }
void RequestPageRefresh()
{ ++settings_consumer_refreshes; }
bool RegisterPageItemAdapter(PageItemKind, PageItemAppender, PageItemResolver, PageInvalidator)
{ return true; }
} // namespace mod_settings::native

namespace
{
struct SettingsConsumerWidget {
  Il2CppObject           header;
  SettingsOptionContext* context = nullptr;
};
struct SettingsConsumerContext {
  Il2CppObject  header;
  Il2CppObject *get = nullptr, *set = nullptr;
  Il2CppObject* query = nullptr;
};
std::function<void()>  settings_consumer_on_context;
SettingsOptionContext* SettingsConsumerGetContext(SettingsConsumerWidget* widget, const MethodInfo*)
{
  auto* before = widget->context;
  if (settings_consumer_on_context)
    std::exchange(settings_consumer_on_context, {})();
  return before;
}
settings_binding::ReferenceDelegate* SettingsConsumerQuery(SettingsConsumerContext* context, const MethodInfo*)
{ return reinterpret_cast<settings_binding::ReferenceDelegate*>(context->query); }

void SettingsChoiceConsumerCases()
{
  using namespace mod_settings;
  SettingsValuesCallbackObservations();
  auto& option       = ValuesClass("Digit.Prime.GameSettings", "OptionContext");
  auto& selection    = ValuesClass("Digit.Prime.GameSettings", "SelectionItemOptionContext", &option);
  auto& widget_class = ValuesClass("Digit.Prime.GameSettings", "SelectionItemOptionWidget");
  auto& get_int      = ValuesClass("Fixture.SettingsValues", "FuncInt");
  auto& set_int      = ValuesClass("Fixture.SettingsValues", "ActionInt");
  auto& safe_get     = ValuesClass("Fixture.SettingsValues", "SafeFuncInt");
  auto& safe_set     = ValuesClass("Fixture.SettingsValues", "SafeActionInt");
  auto& list_int     = ValuesClass("Fixture.SettingsValues", "ListIntActions");
  AddMethod(widget_class, "get_Context", reinterpret_cast<void*>(&SettingsConsumerGetContext), T(selection), {});
  AddMethod(option, "get_QueryOptionState", reinterpret_cast<void*>(&SettingsConsumerQuery), T(delegate_base), {});
  Il2CppObject     getter{&get_int.klass}, setter{&set_int.klass}, foreign{&set_int.klass}, query{&delegate_base.klass};
  ValuesListObject callbacks;
  callbacks.header.klass = &list_int.klass;
  callbacks.entries      = {&setter, nullptr};
  ValuesRefObject         get{{&safe_get.klass}, &getter}, set{{&safe_set.klass}, &callbacks.header};
  SettingsConsumerContext context{{&selection.klass}, &get.header, &set.header, &query};
  SettingsConsumerContext replacement_context{{&selection.klass}, &get.header, &set.header, &query};
  auto*                   row         = reinterpret_cast<SettingsOptionContext*>(&context);
  auto*                   replacement = reinterpret_cast<SettingsOptionContext*>(&replacement_context);
  SettingsConsumerWidget  widget{{&widget_class.klass}, row};
  auto*                   native_widget = reinterpret_cast<SettingsSelectionWidget*>(&widget);
  int                     current = 0, persisted = -1, writes = 0, saves = 0, originals = 0;
  ValueDefinition<int>    definition{"choice.consumer", "Consumer choice",
                                     [&] { return ValueReadResult<int>::Known(current, 1); },
                                     [&](int desired, uint64_t generation) {
                                    Require(generation == 1, "choice callback retains observed generation");
                                    ++writes;
                                    current = desired;
                                    return ApplyResult::Applied;
                                     }};
  definition.persist = [&](const int& value) {
    ++saves;
    persisted = value;
  };
  ChoiceSetting single(definition, {"Native", "Always", "Threshold"});
  ChoiceSetting multiple(definition, {"Federation", "Klingon", "Romulan"}, ChoiceMode::Multiple);
  auto&         state        = ValuesState();
  const auto    roots_before = warp_roots.size();
  state.thread               = std::this_thread::get_id();
  state.active               = true;
  SettingsValueRow record;
  record.context = il2cpp::GcHandle::Weak(row);
  record.get     = il2cpp::GcHandle::Weak(&getter);
  record.set     = il2cpp::GcHandle::Weak(&setter);
  record.query   = il2cpp::GcHandle::Weak(&query);
  record.choice  = &single;
  record.index   = 1;
  state.rows.insert_or_assign(row, std::move(record));
  auto view     = std::make_shared<SettingsValueView>();
  view->widget  = il2cpp::GcHandle::Weak(native_widget);
  view->context = il2cpp::GcHandle::Weak(row);
  view->choice  = &single;
  view->state.emplace(single, 1);
  view->state->Bind();
  view->visit = settings_consumer_visit;
  state.views.insert_or_assign(native_widget, view);
  const auto original = [&](auto* receiver, bool) {
    Require(receiver == native_widget, "choice original retains receiver");
    ++originals;
  };
  SelectionValueChanged(original, native_widget, true);
  Require(current == 1 && persisted == 1 && writes == 1 && saves == 1 && originals == 0,
          "actual choice callback applies and persists one owned selection");
  SelectionValueChanged(original, native_widget, false);
  Require(current == 1 && writes == 1 && saves == 1, "radio uncheck cannot clear the selected preference");
  view->rendering = true;
  SelectionValueChanged(original, native_widget, true);
  view->rendering = false;
  Require(writes == 1 && saves == 1, "render callback cannot authorize choice persistence");
  ++settings_consumer_visit;
  SelectionValueChanged(original, native_widget, true);
  Require(writes == 1 && saves == 1, "departed page cannot save through retained choice callback");
  view->visit = settings_consumer_visit;
  current     = 0;
  view->state->Bind();
  settings_values_on_count = [&] { settings_consumer_on_context = [&] { widget.context = replacement; }; };
  SelectionValueChanged(original, native_widget, true);
  Require(current == 0 && writes == 1 && saves == 1,
          "native context replacement during ownership observation must revoke the old choice intent");
  widget.context           = row;
  settings_values_on_count = [&] { settings_values_on_count = [&] { ++settings_consumer_visit; }; };
  SelectionValueChanged(original, native_widget, true);
  Require(current == 0 && writes == 1 && saves == 1,
          "page departure during the final ownership read cannot authorize a choice save");
  view->visit              = settings_consumer_visit;
  settings_values_on_count = [&] { settings_values_on_count = [&] { state.views.erase(native_widget); }; };
  SelectionValueChanged(original, native_widget, true);
  Require(current == 0 && writes == 1 && saves == 1,
          "retiring the view during ownership reads cannot save through a retained shared record");
  state.views.insert_or_assign(native_widget, view);
  current                   = 7;
  state.rows.at(row).choice = &multiple;
  view->choice              = &multiple;
  view->state.emplace(multiple, 1);
  view->state->Bind();
  settings_consumer_page_refresh = true;
  SelectionValueChanged(original, native_widget, false);
  settings_consumer_page_refresh = false;
  Require(current == 7 && writes == 1 && saves == 1, "page refresh cannot authorize a multi-choice uncheck");
  SelectionValueChanged(original, native_widget, false);
  Require(current == 5 && persisted == 5 && writes == 2 && saves == 2,
          "actual multi-choice callback removes only its own bit and saves the complete mask");
  callbacks.entries[0] = &foreign;
  SelectionValueChanged(original, native_widget, true);
  Require(originals == 1 && current == 5 && writes == 2 && saves == 2 && state.views.empty(),
          "foreign pooled row retains native behavior without a mod write");
  state.rows.clear();
  view.reset();
  state.active = false;
  Require(warp_roots.size() == roots_before, "choice consumer fixture releases its managed custody");
  std::cout
      << "Settings choice consumer: single/multiple, save admission, render/page guards and pooled ownership passed\n";
}

void SettingsSliderConsumerCases()
{
  using namespace mod_settings;
  // Reuse the existing closed SafeFunc/Action/list metadata and its production
  // ownership checks; the callback observations themselves remain unchanged.
  SettingsValuesCallbackObservations();
  auto& option         = ValuesClass("Digit.Prime.GameSettings", "OptionContext");
  auto& slider_context = ValuesClass("Digit.Prime.GameSettings", "SliderOptionContext", &option);
  auto& widget_class   = ValuesClass("Digit.Prime.GameSettings", "SliderOptionWidget");
  auto& get_float      = ValuesClass("Fixture.SettingsValues", "FuncFloat");
  auto& set_float      = ValuesClass("Fixture.SettingsValues", "ActionFloat");
  auto& safe_get       = ValuesClass("Fixture.SettingsValues", "SafeFuncFloat");
  auto& safe_set       = ValuesClass("Fixture.SettingsValues", "SafeActionFloat");
  auto& list_float     = ValuesClass("Fixture.SettingsValues", "ListFloatActions");
  AddMethod(widget_class, "get_Context", reinterpret_cast<void*>(&SettingsConsumerGetContext), T(slider_context), {});
  AddMethod(option, "get_QueryOptionState", reinterpret_cast<void*>(&SettingsConsumerQuery), T(delegate_base), {});
  Il2CppObject getter{&get_float.klass}, setter{&set_float.klass}, foreign{&set_float.klass},
      query{&delegate_base.klass};
  ValuesListObject callbacks;
  callbacks.header.klass = &list_float.klass;
  callbacks.entries      = {&setter, nullptr};
  ValuesRefObject         get{{&safe_get.klass}, &getter}, set{{&safe_set.klass}, &callbacks.header};
  SettingsConsumerContext context{{&slider_context.klass}, &get.header, &set.header, &query};
  SettingsConsumerContext replacement_context{{&slider_context.klass}, &get.header, &set.header, &query};
  auto*                   row         = reinterpret_cast<SettingsOptionContext*>(&context);
  auto*                   replacement = reinterpret_cast<SettingsOptionContext*>(&replacement_context);
  SettingsConsumerWidget  widget{{&widget_class.klass}, row};
  auto*                   native_widget = reinterpret_cast<SettingsSliderWidget*>(&widget);
  float                   current = .52f, persisted = 0;
  int                     writes = 0, saves = 0;
  ValueDefinition<float>  definition{
      "slider.consumer", "Consumer slider", [&] { return ValueReadResult<float>::Known(current, 1); },
      [&](float desired, uint64_t generation) {
        Require(generation == 1, "actual slider consumer forwards its observed owner generation");
        ++writes;
        current = desired;
        return ApplyResult::Applied;
      }};
  definition.persist = [&](const float& value) {
    ++saves;
    persisted = value;
  };
  SliderSetting slider(std::move(definition), 0.f, 1.f, .01f, [] { return true; }, SliderLabel::Value, 2);
  auto&         state        = ValuesState();
  const auto    roots_before = warp_roots.size();
  state.thread               = std::this_thread::get_id();
  state.active               = true;
  SettingsValueRow record;
  record.context = il2cpp::GcHandle::Weak(row);
  record.get     = il2cpp::GcHandle::Weak(&getter);
  record.set     = il2cpp::GcHandle::Weak(&setter);
  record.query   = il2cpp::GcHandle::Weak(&query);
  record.slider  = &slider;
  state.rows.insert_or_assign(row, std::move(record));
  auto view     = std::make_shared<SettingsValueView>();
  view->widget  = il2cpp::GcHandle::Weak(native_widget);
  view->context = il2cpp::GcHandle::Weak(row);
  view->slider  = &slider;
  view->state.emplace(slider);
  view->state->Bind();
  view->visit = settings_consumer_visit;
  state.views.insert_or_assign(native_widget, view);
  constexpr float raw            = .3592861f;
  int             native_changes = 0, labels = 0;
  float           label_value   = -1;
  const auto      native_change = [&](auto*, float) { ++native_changes; };
  const auto      label         = [&](auto* receiver, float value) {
    Require(receiver == native_widget, "label callback preserves the native receiver");
    ++labels;
    label_value = value;
  };
  SliderValueChanged(native_change, native_widget, raw);
  Require(writes == 1 && saves == 1 && persisted == current && std::abs(current - .36f) < .000001f
              && view->state->number() == current && settings_consumer_refreshes == 1 && native_changes == 0,
          "actual changed callback snaps once, retains owner readback and admits exactly one unrounded save");
  const auto saved_bits_value      = current;
  const auto original_slider_value = settings_values_slider_value;
  const auto expect_label          = [&](float expected, const char* reason) {
    const auto before = labels;
    SliderValueLabel(label, native_widget, raw);
    Require(labels == before + 1 && label_value == expected, reason);
    Require(writes == 1 && saves == 1 && current == saved_bits_value && persisted == current
                && settings_values_slider_value == original_slider_value,
            "the real label-only callback never writes the model, persistence sink or native Slider value");
  };
  expect_label(.36f, "raw label listener after actual change displays the applied snapshot exactly once");
  callbacks.entries[0] = &foreign;
  expect_label(raw, "foreign callback ownership retains the original raw native label argument");
  callbacks.entries[0] = &setter;
  view->revoked        = true;
  expect_label(raw, "revoked pooled view retains native label behavior");
  view->revoked = false;
  view->state->Invalidate();
  expect_label(raw, "unknown snapshot does not fabricate a displayed zero");
  view->state->Bind();
  ++settings_consumer_visit;
  expect_label(raw, "departed page cannot apply its historical slider display snapshot");
  view->visit  = settings_consumer_visit;
  state.thread = std::thread::id{};
  expect_label(raw, "foreign thread takes the native label path");
  state.thread      = std::this_thread::get_id();
  warp_root_failure = true;
  expect_label(raw, "failed managed custody retains native label behavior");
  warp_root_failure = false;
  widget.context    = replacement;
  expect_label(raw, "replaced native context is not the retained row owner");
  widget.context               = row;
  settings_consumer_on_context = [&] { widget.context = replacement; };
  expect_label(raw, "context getter reentry returning an old source is caught by the post-query identity check");
  widget.context           = row;
  settings_values_on_count = [&] { ++settings_consumer_visit; };
  expect_label(raw, "page departure during real callback-list ownership observation retires the display snapshot");
  view->visit              = settings_consumer_visit;
  settings_values_on_count = [&] { widget.context = replacement; };
  expect_label(raw, "ownership observation that rebinds native context cannot apply an old display value");
  widget.context           = row;
  settings_values_on_count = [&] { state.views.erase(native_widget); };
  expect_label(raw, "ownership observation that retires the live view cannot publish from its retained shared record");
  state.views.insert_or_assign(native_widget, view);
  expect_label(.36f, "the original qualified view remains usable after refused/reentrant observations");
  state.views.clear();
  state.rows.clear();
  view.reset();
  state.active = false;
  Require(warp_roots.size() == roots_before,
          "post-bind consumer fixture releases every admitted weak and temporary root");
  std::cout << "Settings slider consumer: snap/readback, late native label, foreign/stale/unknown/page/custody and "
               "reentrant ownership passed\n";
}
} // namespace
