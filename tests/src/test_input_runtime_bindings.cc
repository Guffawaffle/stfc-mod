#include <doctest/doctest.h>

#include "patches/input_binding/input_dispatcher.h"
#include "patches/input_binding/input_runtime_bindings.h"

#include <array>

TEST_SUITE("input_runtime_bindings")
{
  TEST_CASE("runtime binding generation increments when the model changes")
  {
    const auto before = input_binding::RuntimeBindingGeneration();

    input_binding::SetRuntimeBindingModel(input_binding::CompileBindingSet());
    const auto after_defaults = input_binding::RuntimeBindingGeneration();
    CHECK(after_defaults == before + 1);

    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::HotkeysDisable, "F1"},
    };

    input_binding::SetRuntimeBindingModel(input_binding::CompileBindingSet(overrides));
    const auto after_override = input_binding::RuntimeBindingGeneration();
    CHECK(after_override == after_defaults + 1);

    input_binding::SetRuntimeBindingModel(input_binding::CompileBindingSet());
  }

  TEST_CASE("runtime binding model defaults to compiled action defaults")
  {
    input_binding::SetRuntimeBindingModel(input_binding::CompileBindingSet());

    auto modifiers = input_binding::ModifierMask{};
    modifiers.AddLogical(input_binding::ModifierGroup::Ctrl);
    modifiers.AddLogical(input_binding::ModifierGroup::Alt);

    input_binding::DispatchRequest request;
    request.trigger_mode = input_binding::TriggerMode::Down;
    request.phase = input_binding::InputPhase::Frame;
    request.key = KeyCode::Minus;
    request.held_modifiers = modifiers;

    const auto plan = input_binding::PlanDispatch(input_binding::RuntimeBindingModel(), request);
    REQUIRE(plan.winners.size() == 1);
    CHECK(plan.winners[0].action == input_binding::InputActionId::HotkeysDisable);
  }

  TEST_CASE("runtime binding model accepts compiled config overrides")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::HotkeysDisable, "F1"},
    };

    input_binding::SetRuntimeBindingModel(input_binding::CompileBindingSet(overrides));

    input_binding::DispatchRequest request;
    request.trigger_mode = input_binding::TriggerMode::Down;
    request.phase = input_binding::InputPhase::Frame;
    request.key = KeyCode::F1;

    const auto plan = input_binding::PlanDispatch(input_binding::RuntimeBindingModel(), request);
    REQUIRE(plan.winners.size() == 1);
    CHECK(plan.winners[0].action == input_binding::InputActionId::HotkeysDisable);

    input_binding::SetRuntimeBindingModel(input_binding::CompileBindingSet());
  }
}