#include <doctest/doctest.h>

#include "patches/input_binding/input_binding.h"
#include "patches/input_binding/input_dispatcher.h"

#include <array>

TEST_SUITE("input_dispatcher")
{
  TEST_CASE("dispatcher filters by phase and active layer")
  {
    const auto compiled = input_binding::CompileBindingSet();

    input_binding::DispatchRequest request;
    request.trigger_mode = input_binding::TriggerMode::Pressed;
    request.phase = input_binding::InputPhase::NavigationZoomUpdate;
    request.key = KeyCode::Q;
    request.active_layers = input_binding::ActiveLayers::Only(input_binding::InputLayer::Zoom);

    auto plan = input_binding::PlanDispatch(compiled, request);
    REQUIRE(plan.winners.size() == 1);
    CHECK(plan.winners[0].action == input_binding::InputActionId::ZoomIn);

    request.phase = input_binding::InputPhase::Frame;
    plan = input_binding::PlanDispatch(compiled, request);
    CHECK(plan.empty());
  }

  TEST_CASE("dispatcher keeps one winner per conflict group but preserves cross group winners")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::FleetPrimary, "SPACE"},
        input_binding::BindingOverride{input_binding::InputActionId::FleetService, "SPACE"},
        input_binding::BindingOverride{input_binding::InputActionId::HotkeysDisable, "SPACE"},
        input_binding::BindingOverride{input_binding::InputActionId::LogDebug, "SPACE"},
    };

    const auto compiled = input_binding::CompileBindingSet(overrides);

    input_binding::DispatchRequest request;
    request.trigger_mode = input_binding::TriggerMode::Down;
    request.phase = input_binding::InputPhase::Frame;
    request.key = KeyCode::Space;

    const auto plan = input_binding::PlanDispatch(compiled, request);
    REQUIRE(plan.candidates.size() == 4);
    REQUIRE(plan.winners.size() == 3);
    CHECK(plan.winners[0].action == input_binding::InputActionId::HotkeysDisable);
    CHECK(plan.winners[1].action == input_binding::InputActionId::FleetPrimary);
    CHECK(plan.winners[2].action == input_binding::InputActionId::LogDebug);
  }

  TEST_CASE("dispatcher combines original call decisions conservatively")
  {
    const std::array allow_only{
        input_binding::ExecutionDecision::NoOpinion,
        input_binding::ExecutionDecision::AllowOriginal,
    };
    CHECK(input_binding::CombineExecutionDecisions(allow_only) == input_binding::ExecutionDecision::AllowOriginal);

    const std::array suppresses{
        input_binding::ExecutionDecision::AllowOriginal,
        input_binding::ExecutionDecision::SuppressOriginal,
    };
    CHECK(input_binding::CombineExecutionDecisions(suppresses)
          == input_binding::ExecutionDecision::SuppressOriginal);

    const std::array no_opinion{
        input_binding::ExecutionDecision::NoOpinion,
        input_binding::ExecutionDecision::NoOpinion,
    };
    CHECK(input_binding::CombineExecutionDecisions(no_opinion) == input_binding::ExecutionDecision::NoOpinion);
  }
}