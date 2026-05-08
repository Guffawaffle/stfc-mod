#include <doctest/doctest.h>

#include "patches/input_binding/input_binding.h"
#include "patches/input_binding/input_dispatcher.h"
#include "testable_functions.h"

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
    CHECK(plan.winners[1].action == input_binding::InputActionId::LogDebug);
    CHECK(plan.winners[2].action == input_binding::InputActionId::FleetPrimary);
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

  TEST_CASE("snapshot dispatcher plans down and held keys through one dispatch pass")
  {
    const auto compiled = input_binding::CompileBindingSet();

    auto global_modifiers = input_binding::ModifierMask{};
    global_modifiers.AddLogical(input_binding::ModifierGroup::Ctrl);
    global_modifiers.AddLogical(input_binding::ModifierGroup::Alt);

    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::Minus, global_modifiers, true, true},
        input_binding::DispatchKeyState{KeyCode::Q, {}, false, true},
    };

    auto plan = input_binding::PlanDispatchSnapshot(compiled,
                                                    input_binding::InputPhase::Frame,
                                                    input_binding::ActiveLayers::All(),
                                                    key_states);
    REQUIRE(plan.winners.size() == 1);
    CHECK(plan.winners[0].action == input_binding::InputActionId::HotkeysDisable);

    plan = input_binding::PlanDispatchSnapshot(compiled,
                                               input_binding::InputPhase::NavigationZoomUpdate,
                                               input_binding::ActiveLayers::Only(input_binding::InputLayer::Zoom),
                                               key_states);
    REQUIRE(plan.winners.size() == 1);
    CHECK(plan.winners[0].action == input_binding::InputActionId::ZoomIn);
  }

  TEST_CASE("snapshot dispatcher applies conflict groups across simultaneous keys")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::FleetPrimary, "SPACE"},
        input_binding::BindingOverride{input_binding::InputActionId::FleetQueueClear, "MOUSE1"},
    };
    const auto compiled = input_binding::CompileBindingSet(overrides);

    auto global_modifiers = input_binding::ModifierMask{};
    global_modifiers.AddLogical(input_binding::ModifierGroup::Ctrl);
    global_modifiers.AddLogical(input_binding::ModifierGroup::Alt);

    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::Minus, global_modifiers, true, true},
        input_binding::DispatchKeyState{KeyCode::Equals, global_modifiers, true, true},
        input_binding::DispatchKeyState{KeyCode::Space, {}, true, true},
        input_binding::DispatchKeyState{KeyCode::Mouse1, {}, true, true},
    };

    const auto plan = input_binding::PlanDispatchSnapshot(compiled,
                                                          input_binding::InputPhase::Frame,
                                                          input_binding::ActiveLayers::All(),
                                                          key_states);
    REQUIRE(plan.candidates.size() == 4);
    REQUIRE(plan.winners.size() == 2);
    CHECK(plan.winners[0].conflict_group == input_binding::ConflictGroup::GlobalControl);
    CHECK(plan.winners[1].action == input_binding::InputActionId::FleetQueueClear);
  }

  TEST_CASE("snapshot dispatcher keeps one frame-dispatch winner across simultaneous keys")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::UiScaleUp, "F1"},
        input_binding::BindingOverride{input_binding::InputActionId::LogDebug, "F2"},
    };
    const auto compiled = input_binding::CompileBindingSet(overrides);

    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::F1, {}, false, true},
        input_binding::DispatchKeyState{KeyCode::F2, {}, true, true},
    };

    const auto plan = input_binding::PlanDispatchSnapshot(compiled,
                                                          input_binding::InputPhase::Frame,
                                                          input_binding::ActiveLayers::All(),
                                                          key_states);
    REQUIRE(plan.candidates.size() == 2);
    REQUIRE(plan.winners.size() == 1);
    CHECK(plan.winners[0].action == input_binding::InputActionId::UiScaleUp);
  }

  TEST_CASE("snapshot dispatcher preserves section before config and log table priority")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::ShowSettings, "F1"},
        input_binding::BindingOverride{input_binding::InputActionId::TogglePreviewLocate, "F2"},
        input_binding::BindingOverride{input_binding::InputActionId::LogDebug, "F3"},
    };
    const auto compiled = input_binding::CompileBindingSet(overrides);

    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::F1, {}, true, true},
        input_binding::DispatchKeyState{KeyCode::F2, {}, true, true},
        input_binding::DispatchKeyState{KeyCode::F3, {}, true, true},
    };

    const auto plan = input_binding::PlanDispatchSnapshot(compiled,
                                                          input_binding::InputPhase::Frame,
                                                          input_binding::ActiveLayers::All(),
                                                          key_states);
    REQUIRE(plan.candidates.size() == 3);
    REQUIRE(plan.winners.size() == 1);
    CHECK(plan.winners[0].action == input_binding::InputActionId::ShowSettings);
  }

  TEST_CASE("watched keys are limited to requested actions and phase")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::HotkeysDisable, "F1|CTRL-MOUSE1"},
        input_binding::BindingOverride{input_binding::InputActionId::HotkeysEnable, "F2"},
        input_binding::BindingOverride{input_binding::InputActionId::ZoomIn, "Q"},
    };
    const auto compiled = input_binding::CompileBindingSet(overrides);

    const std::array actions{
        input_binding::InputActionId::HotkeysDisable,
        input_binding::InputActionId::HotkeysEnable,
    };

    const auto watched_keys = input_binding::WatchedKeysForActions(compiled,
                                                                   input_binding::InputPhase::Frame,
                                                                   actions);
    REQUIRE(watched_keys.size() == 3);
    CHECK(watched_keys[0] == KeyCode::F1);
    CHECK(watched_keys[1] == KeyCode::Mouse1);
    CHECK(watched_keys[2] == KeyCode::F2);

    const auto zoom_keys = input_binding::WatchedKeysForActions(compiled,
                                                                input_binding::InputPhase::NavigationZoomUpdate,
                                                                std::array{input_binding::InputActionId::ZoomIn});
    REQUIRE(zoom_keys.size() == 1);
    CHECK(zoom_keys[0] == KeyCode::Q);
  }

  TEST_CASE("snapshot dispatcher winners can drive startup hotkey decisions")
  {
    const auto compiled = input_binding::CompileBindingSet();

    auto modifiers = input_binding::ModifierMask{};
    modifiers.AddLogical(input_binding::ModifierGroup::Ctrl);
    modifiers.AddLogical(input_binding::ModifierGroup::Alt);

    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::Minus, modifiers, true, true},
        input_binding::DispatchKeyState{KeyCode::Equals, modifiers, false, false},
    };
    const auto plan = input_binding::PlanDispatchSnapshot(compiled,
                                                          input_binding::InputPhase::Frame,
                                                          input_binding::ActiveLayers::Only(input_binding::InputLayer::Global),
                                                          key_states);

    bool disable_hotkeys_pressed = false;
    bool enable_hotkeys_pressed = false;
    for (const auto& winner : plan.winners) {
      if (winner.action == input_binding::InputActionId::HotkeysDisable) {
        disable_hotkeys_pressed = true;
      } else if (winner.action == input_binding::InputActionId::HotkeysEnable) {
        enable_hotkeys_pressed = true;
      }
    }

    CHECK(hotkey_router_startup_action(disable_hotkeys_pressed, enable_hotkeys_pressed, false, true)
          == HotkeyRouterStartupAction::DisableHotkeys);
  }
}