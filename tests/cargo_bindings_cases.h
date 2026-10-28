#pragma once

// Production cargo hooks against the existing controlled runtime. Formatting and
// original rendering bodies are supplied by this fixture; no game UI is run.
#include "../src/mod/features/cargo.cc"

namespace
{
struct CargoNativeText {
  Il2CppObject  header;
  Il2CppString* identifier = nullptr;
  int32_t       decimals   = 3;
};
struct CargoNativeBar {
  Il2CppObject     header;
  CargoNativeText* label   = nullptr;
  Il2CppObject*    context = nullptr;
  bool             snap    = false;
};
void CargoNativeFormat(ColourTextLocalizer*, bool, il2cpp::ArrayObject*, const MethodInfo*) {}
void CargoNativeSetWidget(LegacyComparableProgressBar*, const MethodInfo*) {}

void CargoNativeCases(std::string_view mode)
{
  auto& text               = Define("Digit.Client.UI", "TextLocalizer", IL2CPP_TYPE_CLASS, &object_class);
  auto& colour             = Define("Digit.Client.UI", "ColourTextLocalizer", IL2CPP_TYPE_CLASS, &text);
  auto& progress           = Define("Digit.PrimeServer.Models", "IProgressData", IL2CPP_TYPE_CLASS, &object_class);
  auto& cargo              = Define("Digit.PrimeServer.Models", "CargoProgressData", IL2CPP_TYPE_CLASS, &progress);
  auto& base               = Define("Digit.Prime.Stats", "LegacyProgressBar", IL2CPP_TYPE_CLASS, &object_class);
  auto& bar_class          = Define("Digit.Prime.Stats", "LegacyComparableProgressBar", IL2CPP_TYPE_CLASS, &base);
  text.klass.instance_size = colour.klass.instance_size = sizeof(CargoNativeText);
  base.klass.instance_size = bar_class.klass.instance_size = sizeof(CargoNativeBar);
  AddField(text, "m_identifier", string_class, offsetof(CargoNativeText, identifier));
  AddField(text, "m_significantDecimals", int_class, offsetof(CargoNativeText, decimals));
  AddField(base, "_currentValue_X_of_Y", text, offsetof(CargoNativeBar, label));
  AddField(base, "m_context", mode == "cargo-context-pointer" ? intptr_class : progress,
           offsetof(CargoNativeBar, context));
  AddField(base, "_snapToValue", mode == "cargo-snap-invalid" ? int_class : bool_class, offsetof(CargoNativeBar, snap));
  auto& set_owner = mode == "cargo-inherited-hook" ? base : bar_class;
  AddMethod(set_owner, "SetWidgetData", reinterpret_cast<void*>(&CargoNativeSetWidget), T(void_class), {});
  AddMethod(colour, "SetLocalTextParameters", reinterpret_cast<void*>(&CargoNativeFormat), T(void_class),
            {mode == "cargo-format-signature" ? T(int_class) : T(bool_class), T(object_array_class)});

  const auto make_string = [](std::u16string_view value) {
    auto* result = static_cast<Il2CppString*>(std::calloc(1, sizeof(Il2CppString) + value.size() * sizeof(Il2CppChar)));
    result->object.klass = &string_class.klass;
    result->length       = static_cast<int32_t>(value.size());
    std::copy(value.begin(), value.end(), result->chars);
    return std::unique_ptr<Il2CppString, decltype(&std::free)>(result, &std::free);
  };
  auto            identifier = make_string(u"shared_x_of_y_x_coloured");
  auto            unrelated  = make_string(u"shared_x_of_y_x_colourEd");
  auto            lookalike  = make_string(u"\u0173hared_x_of_y_x_coloured");
  CargoNativeText label{{&colour.klass}, identifier.get(), 3};
  Il2CppObject    cargo_context{&cargo.klass}, other_context{&object_class.klass};
  CargoNativeBar  bar{{&bar_class.klass}, &label, &cargo_context, false};
  auto*           native_bar  = reinterpret_cast<LegacyComparableProgressBar*>(&bar);
  auto*           native_text = reinterpret_cast<ColourTextLocalizer*>(&label);
  auto&           ui          = Config::Get().ui;
  ui.format_cargo_values = ui.instant_cargo_counter = true;

  if (mode == "cargo-context-pointer" || mode == "cargo-snap-invalid") {
    bar.context = reinterpret_cast<Il2CppObject*>(uintptr_t{1});
    Require(!LegacyComparableProgressBar::CargoFieldsAvailable() && !IsCargoText(native_bar) && !InstallInstantCargo(),
            "missing snap or native-pointer context metadata declines before reading an unsafe cargo object");
    int originals = 0;
    LegacyComparableProgressBar_SetWidgetData_Hook([&](auto*) { ++originals; }, native_bar);
    Require(originals == 1 && !bar.snap, "unavailable cargo contract retains the native renderer exactly once");
    return;
  }
  if (mode == "cargo-inherited-hook") {
    Require(!InstallInstantCargo(), "inherited common Widget/ProgressBar entry is not detoured for cargo");
    return;
  }
  if (mode == "cargo-format-signature") {
    Require(!InstallCargoFormat(), "changed parse-ID parameter type rejects the cargo format hook");
    return;
  }

  Require(TextLocalizer::CargoFormatFieldsAvailable() && LegacyComparableProgressBar::CargoFieldsAvailable(),
          "canonical cargo field contracts qualify complete metadata");
  int        format_originals = 0;
  const auto format           = [&](auto* receiver, bool parse, void* args) {
    Require(receiver == native_text && parse && args == nullptr, "format preserves original receiver and arguments");
    ++format_originals;
  };
  ui.cargo_significant_decimals = 2;
  ColourTextLocalizer_SetLocalTextParameters_Hook(format, native_text, true, nullptr);
  Require(label.decimals == 2 && format_originals == 1, "cargo precision is set before the native format call");
  ui.cargo_significant_decimals = -8;
  ColourTextLocalizer_SetLocalTextParameters_Hook(format, native_text, true, nullptr);
  Require(label.decimals == 0, "negative cargo precision clamps to zero without changing configuration");
  ui.cargo_significant_decimals = 42;
  ColourTextLocalizer_SetLocalTextParameters_Hook(format, native_text, true, nullptr);
  Require(label.decimals == 6 && ui.cargo_significant_decimals == 42, "large cargo precision clamps only presentation");
  ui.format_cargo_values        = false;
  ui.cargo_significant_decimals = 1;
  ColourTextLocalizer_SetLocalTextParameters_Hook(format, native_text, true, nullptr);
  Require(label.decimals == 6, "disabled formatting does not mutate already-bound native fields");
  ui.format_cargo_values = true;
  for (auto* value : {unrelated.get(), lookalike.get(), static_cast<Il2CppString*>(nullptr)}) {
    label.identifier = value;
    ColourTextLocalizer_SetLocalTextParameters_Hook(format, native_text, true, nullptr);
    Require(label.decimals == 6, "noncargo, Unicode lookalike and absent identifiers keep native precision");
  }
  label.identifier            = identifier.get();
  int        render_originals = 0;
  const auto render           = [&](auto* receiver) {
    Require(receiver == native_bar && bar.snap, "native cargo render observes snap while retaining exact receiver");
    ++render_originals;
  };
  for (const bool previous : {false, true}) {
    bar.snap = previous;
    LegacyComparableProgressBar_SetWidgetData_Hook(render, native_bar);
    Require(bar.snap == previous, "cargo snap restores both original prefab flag states");
  }
  bar.snap = false;
  try {
    LegacyComparableProgressBar_SetWidgetData_Hook([](auto*) { throw std::runtime_error("native render failure"); },
                                                   native_bar);
    Require(false, "the controlled original should throw");
  } catch (const std::runtime_error&) {
    Require(!bar.snap, "snap restoration survives an unwinding original rather than contaminating the pooled bar");
  }
  const auto native = [&](auto*) {
    ++render_originals;
    Require(!bar.snap, "ordinary bar retains snap=false");
  };
  ui.instant_cargo_counter = false;
  LegacyComparableProgressBar_SetWidgetData_Hook(native, native_bar);
  ui.instant_cargo_counter = true;
  bar.context              = &other_context;
  LegacyComparableProgressBar_SetWidgetData_Hook(native, native_bar);
  bar.context = nullptr;
  LegacyComparableProgressBar_SetWidgetData_Hook(native, native_bar);
  bar.context      = &cargo_context;
  label.identifier = unrelated.get();
  LegacyComparableProgressBar_SetWidgetData_Hook(native, native_bar);
  Require(render_originals == 6 && format_originals == 7, "all guarded paths preserve one original invocation");
  std::cout << "Cargo production adapters: precision, live/disabled/reused labels, exact context and snap restoration "
               "passed\n";
}
} // namespace
