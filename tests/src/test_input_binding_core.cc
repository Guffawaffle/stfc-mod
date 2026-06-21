#include "test_pure_common.h"

#include "patches/input_binding/action_registry.h"

// ===========================================================================
// input_binding
// ===========================================================================

TEST_SUITE("input_binding")
{
  TEST_CASE("schema exposes unified action subset")
  {
    const auto specs = input_binding::ActionSpecs();
    CHECK(specs.size() == static_cast<size_t>(input_binding::InputActionId::Max));

    const auto* fleet_primary = input_binding::FindActionSpec(input_binding::InputActionId::FleetPrimary);
    REQUIRE(fleet_primary != nullptr);
    CHECK(fleet_primary->canonical_key == "fleet_primary");
    CHECK(fleet_primary->default_bind == "SPACE|MOUSE1");

    const auto* hotkeys_enable = input_binding::FindActionSpec("hotkeys_enable");
    REQUIRE(hotkeys_enable != nullptr);
    CHECK(hotkeys_enable->default_bind == "CTRL-ALT-=");

    const auto* select_ship1 = input_binding::FindActionSpec(input_binding::InputActionId::SelectShip1);
    REQUIRE(select_ship1 != nullptr);
    CHECK(select_ship1->canonical_key == "select_ship1");
    CHECK(select_ship1->default_bind == "1");

    const auto* show_chat = input_binding::FindActionSpec(input_binding::InputActionId::ShowChat);
    REQUIRE(show_chat != nullptr);
    CHECK(show_chat->canonical_key == "show_chat");
    CHECK(show_chat->default_bind == "C");

    const auto* zoom_preset1 = input_binding::FindActionSpec(input_binding::InputActionId::ZoomPreset1);
    REQUIRE(zoom_preset1 != nullptr);
    CHECK(zoom_preset1->canonical_key == "zoom_preset1");
    CHECK(zoom_preset1->default_bind == "F1");

    const auto* set_zoom_default = input_binding::FindActionSpec(input_binding::InputActionId::SetZoomDefault);
    REQUIRE(set_zoom_default != nullptr);
    CHECK(set_zoom_default->canonical_key == "set_zoom_default");
    CHECK(set_zoom_default->default_bind == "CTRL-=");

    for (const auto& spec : specs) {
      const auto binding = input_binding::ParseBinding(spec.default_bind);
      INFO(spec.canonical_key);
      if (spec.default_bind == std::string_view{"NONE"}) {
        CHECK(binding.unbound);
      } else {
        CHECK(binding.has_valid_chord());
      }
      CHECK_FALSE(binding.has_warnings());
      CHECK_FALSE(binding.has_errors());
    }
  }

  TEST_CASE("action registry backs compatibility schema lookups")
  {
    const auto registry = input_binding::ActionRegistry();
    const auto specs    = input_binding::ActionSpecs();

    CHECK(registry.size() == static_cast<size_t>(input_binding::InputActionId::Max));
    CHECK(specs.data() == registry.data());
    CHECK(specs.size() == registry.size());

    for (const auto& action : registry) {
      const auto* by_id     = input_binding::FindAction(action.id);
      const auto* by_key    = input_binding::FindAction(action.canonical_key);
      const auto* legacy_id = input_binding::FindActionSpec(action.id);
      INFO(action.canonical_key);
      REQUIRE(by_id != nullptr);
      CHECK(by_id == &action);
      CHECK(by_key == by_id);
      CHECK(legacy_id == by_id);
      CHECK(action.native_consume == input_binding::NativeConsumePolicy::WhenHandled);
      CHECK(action.rebindable);
      CHECK(action.visible_in_ui);
    }
  }

  TEST_CASE("action registry owns table dispatch metadata")
  {
    const auto* q_trials = input_binding::FindAction(input_binding::InputActionId::ShowQTrials);
    REQUIRE(q_trials != nullptr);
    CHECK(q_trials->executor == input_binding::ActionExecutor::TableDispatch);
    CHECK(q_trials->game_function == GameFunction::ShowQTrials);
    CHECK(input_binding::ActionGameFunction(input_binding::InputActionId::ShowQTrials) == GameFunction::ShowQTrials);

    const auto* station_exterior = input_binding::FindAction(input_binding::InputActionId::ShowStationExterior);
    REQUIRE(station_exterior != nullptr);
    CHECK(station_exterior->executor == input_binding::ActionExecutor::TableDispatch);
    CHECK(station_exterior->game_function == GameFunction::ShoWStationExterior);

    const auto* cargo_default = input_binding::FindAction(input_binding::InputActionId::ToggleCargoDefault);
    REQUIRE(cargo_default != nullptr);
    CHECK(cargo_default->category == input_binding::ActionCategory::Ships);
    CHECK(cargo_default->executor == input_binding::ActionExecutor::TableDispatch);
    CHECK(cargo_default->game_function == GameFunction::ToggleCargoDefault);

    const auto* fleet_service = input_binding::FindAction(input_binding::InputActionId::FleetService);
    REQUIRE(fleet_service != nullptr);
    CHECK(fleet_service->executor == input_binding::ActionExecutor::FleetSpace);
    CHECK(fleet_service->game_function == GameFunction::Max);
  }

  TEST_CASE("key lookup covers keyboard mouse and function keys")
  {
    CHECK(input_binding::LookupKey("space") == KeyCode::Space);
    CHECK(input_binding::LookupKey("V") == KeyCode::V);
    CHECK(input_binding::LookupKey("]") == KeyCode::RightBracket);
    CHECK(input_binding::LookupKey("MOUSE1") == KeyCode::Mouse1);
    CHECK(input_binding::LookupKey("F12") == KeyCode::F12);
    CHECK_FALSE(input_binding::LookupKey("NOT_A_KEY").has_value());
  }

  TEST_CASE("modifier masks satisfy logical and physical requirements")
  {
    const auto ctrl           = input_binding::ModifierMask::Logical(input_binding::ModifierGroup::Ctrl);
    auto       held_left_ctrl = input_binding::ModifierMask::FromPressedKey(KeyCode::LeftControl);
    CHECK(ctrl.IsSatisfiedBy(held_left_ctrl));
    CHECK(ctrl.IsExactMatch(held_left_ctrl));

    held_left_ctrl.Merge(input_binding::ModifierMask::FromPressedKey(KeyCode::LeftShift));
    CHECK(ctrl.IsSatisfiedBy(held_left_ctrl));
    CHECK_FALSE(ctrl.IsExactMatch(held_left_ctrl));

    const auto left_ctrl = input_binding::ModifierMask::Physical(input_binding::PhysicalModifier::LeftControl);
    CHECK(left_ctrl.IsSatisfiedBy(input_binding::ModifierMask::FromPressedKey(KeyCode::LeftControl)));
    CHECK_FALSE(left_ctrl.IsSatisfiedBy(input_binding::ModifierMask::FromPressedKey(KeyCode::RightControl)));
  }

  TEST_CASE("chord parser handles simple modified and mouse chords")
  {
    auto chord = input_binding::ParseChord("a");
    REQUIRE(chord.valid);
    CHECK(chord.key == KeyCode::A);
    CHECK(chord.modifiers.empty());

    chord = input_binding::ParseChord("CTRL-SHIFT-F9");
    REQUIRE(chord.valid);
    CHECK(chord.key == KeyCode::F9);
    auto held = input_binding::ModifierMask::FromPressedKey(KeyCode::LeftControl);
    held.Merge(input_binding::ModifierMask::FromPressedKey(KeyCode::RightShift));
    CHECK(chord.Matches(KeyCode::F9, held));

    chord = input_binding::ParseChord("MOUSE1");
    REQUIRE(chord.valid);
    CHECK(chord.key == KeyCode::Mouse1);
  }

  TEST_CASE("chord matching rejects extra modifiers by default")
  {
    auto chord = input_binding::ParseChord("CTRL-A");
    REQUIRE(chord.valid);

    auto held = input_binding::ModifierMask::FromPressedKey(KeyCode::LeftControl);
    CHECK(chord.Matches(KeyCode::A, held));

    held.Merge(input_binding::ModifierMask::FromPressedKey(KeyCode::LeftShift));
    CHECK_FALSE(chord.Matches(KeyCode::A, held));
    CHECK(chord.Matches(KeyCode::A, held, true));
  }

  TEST_CASE("binding parser handles none multibind and partial invalid tokens")
  {
    auto binding = input_binding::ParseBinding("NONE");
    CHECK(binding.unbound);
    CHECK(binding.DisplayString() == "NONE");

    binding = input_binding::ParseBinding("SPACE|INVALID|MOUSE1");
    CHECK(binding.has_valid_chord());
    CHECK(binding.has_errors() == false);
    REQUIRE(binding.diagnostics.size() == 1);
    CHECK(binding.diagnostics[0].severity == input_binding::DiagnosticSeverity::Warning);
    CHECK(binding.DisplayString() == "SPACE | MOUSE1");

    binding = input_binding::ParseBinding("CTRL-SHIFT");
    CHECK_FALSE(binding.has_valid_chord());
    CHECK(binding.has_errors());

    binding = input_binding::ParseBinding("CTRL-NOTREAL-A");
    CHECK(binding.has_valid_chord());
    CHECK(binding.has_warnings());
    CHECK_FALSE(binding.has_errors());
  }

  TEST_CASE("modifier-only chords are invalid and never registered")
  {
    for (const auto* text : {"ALT", "CTRL-ALT", "SHIFT-CTRL", "LALT-RCTRL"}) {
      CAPTURE(text);
      const auto chord = input_binding::ParseChord(text);
      CHECK_FALSE(chord.valid);
      CHECK(chord.key == KeyCode::None);
    }

    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::HotkeysDisable, "CTRL-ALT"},
    };
    const auto compiled = input_binding::CompileBindingSet(overrides);

    CHECK(compiled.has_errors());
    CHECK(compiled.bound_chord_count == 94);

    auto modifiers = input_binding::ModifierMask{};
    modifiers.AddLogical(input_binding::ModifierGroup::Ctrl);
    modifiers.AddLogical(input_binding::ModifierGroup::Alt);
    const auto matches = compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Minus, modifiers);
    CHECK(matches.empty());
  }

  TEST_CASE("binding index matches by key trigger and priority order")
  {
    input_binding::BindingIndex index;
    index.Register(input_binding::InputActionId::FleetPrimary, input_binding::ParseChord("SPACE"),
                   input_binding::TriggerMode::Down, 10);
    index.Register(input_binding::InputActionId::FleetQueueClear, input_binding::ParseChord("SPACE"),
                   input_binding::TriggerMode::Down, 20);
    index.Register(input_binding::InputActionId::ZoomIn, input_binding::ParseChord("SPACE"),
                   input_binding::TriggerMode::Pressed, 30);

    CHECK(index.size() == 3);

    const auto held    = input_binding::ModifierMask{};
    auto       matches = index.Match(input_binding::TriggerMode::Down, KeyCode::Space, held);
    REQUIRE(matches.size() == 2);
    CHECK(matches[0] == input_binding::InputActionId::FleetQueueClear);
    CHECK(matches[1] == input_binding::InputActionId::FleetPrimary);

    matches = index.Match(input_binding::TriggerMode::Pressed, KeyCode::Space, held);
    REQUIRE(matches.size() == 1);
    CHECK(matches[0] == input_binding::InputActionId::ZoomIn);
  }

  TEST_CASE("compiler builds default index without diagnostics")
  {
    const auto compiled = input_binding::CompileBindingSet();

    CHECK(compiled.bound_chord_count == 95);
    CHECK(compiled.index.size() == 95);
    CHECK_FALSE(compiled.has_warnings());
    CHECK_FALSE(compiled.has_errors());
    CHECK_FALSE(compiled.has_conflicts());

    const auto held    = input_binding::ModifierMask{};
    auto       matches = compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Space, held);
    REQUIRE(matches.size() == 3);
    CHECK(matches[0] == input_binding::InputActionId::FleetPrimary);
    CHECK(matches[1] == input_binding::InputActionId::FleetQueueAdd);
    CHECK(matches[2] == input_binding::InputActionId::FleetRecallCancel);

    matches = compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Q, held);
    CHECK(matches.empty());

    matches = compiled.index.Match(input_binding::TriggerMode::Pressed, KeyCode::Q, held);
    REQUIRE(matches.size() == 1);
    CHECK(matches[0] == input_binding::InputActionId::ZoomIn);
  }

  TEST_CASE("compiler accepts unbound overrides")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::FleetPrimary, "NONE"},
    };

    const auto compiled = input_binding::CompileBindingSet(overrides);
    CHECK_FALSE(compiled.has_errors());
    CHECK(compiled.bound_chord_count == 93);

    const auto matches =
        compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Space, input_binding::ModifierMask{});
    REQUIRE(matches.size() == 2);
    CHECK(matches[0] == input_binding::InputActionId::FleetQueueAdd);
    CHECK(matches[1] == input_binding::InputActionId::FleetRecallCancel);
  }

  TEST_CASE("compiler carries action scoped parser diagnostics")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::FleetPrimary, "CTRL-NOTREAL-A"},
    };

    const auto compiled = input_binding::CompileBindingSet(overrides);
    CHECK(compiled.has_warnings());
    CHECK_FALSE(compiled.has_errors());
    REQUIRE(compiled.diagnostics.size() == 1);
    CHECK(compiled.diagnostics[0].action == input_binding::InputActionId::FleetPrimary);
  }

  TEST_CASE("compiler reports same group chord conflicts")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::FleetSecondary, "SPACE"},
    };

    const auto compiled = input_binding::CompileBindingSet(overrides);
    CHECK(compiled.has_errors());
    CHECK(compiled.has_conflicts());
    REQUIRE(compiled.conflicts.size() == 1);
    CHECK(compiled.conflicts[0].action_a == input_binding::InputActionId::FleetPrimary);
    CHECK(compiled.conflicts[0].action_b == input_binding::InputActionId::FleetSecondary);

    const auto matches =
        compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Space, input_binding::ModifierMask{});
    REQUIRE(matches.size() == 4);
    CHECK(matches[0] == input_binding::InputActionId::FleetPrimary);
    CHECK(matches[1] == input_binding::InputActionId::FleetQueueAdd);
    CHECK(matches[2] == input_binding::InputActionId::FleetRecallCancel);
    CHECK(matches[3] == input_binding::InputActionId::FleetSecondary);
  }

  TEST_CASE("compiler reports logical and physical modifier overlaps")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::FleetPrimary, "CTRL-SPACE"},
        input_binding::BindingOverride{input_binding::InputActionId::FleetSecondary, "LCTRL-SPACE"},
        input_binding::BindingOverride{input_binding::InputActionId::SelectCurrent, "NONE"},
    };

    const auto compiled = input_binding::CompileBindingSet(overrides);
    CHECK(compiled.has_errors());
    CHECK(compiled.has_conflicts());
    REQUIRE(compiled.conflicts.size() == 1);

    const auto held    = input_binding::ModifierMask::FromPressedKey(KeyCode::LeftControl);
    const auto matches = compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Space, held);
    REQUIRE(matches.size() == 2);
    CHECK(matches[0] == input_binding::InputActionId::FleetPrimary);
    CHECK(matches[1] == input_binding::InputActionId::FleetSecondary);
  }

  TEST_CASE("config bridge resolves defaults into canonical bindings")
  {
    const toml::table config;
    const auto        bridge = input_binding::ResolveInputBindingConfig(config);

    const auto fleet_primary = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::FleetPrimary;
    });
    REQUIRE(fleet_primary != bridge.bindings.end());
    CHECK(fleet_primary->binding == "SPACE|MOUSE1");
    CHECK(fleet_primary->source_key == "default");
    CHECK(bridge.compatibility_warnings.empty());
  }

  TEST_CASE("config bridge accepts migrated ship selection aliases")
  {
    const auto config = toml::parse(R"(
[shortcuts]
select_ship1 = "CTRL-1"
select_current = "CTRL-SPACE"
)");

    const auto bridge  = input_binding::ResolveInputBindingConfig(config);
    const auto ship1   = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::SelectShip1;
    });
    const auto current = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::SelectCurrent;
    });

    REQUIRE(ship1 != bridge.bindings.end());
    REQUIRE(current != bridge.bindings.end());
    CHECK(ship1->binding == "CTRL-1");
    CHECK(ship1->source_key == "[shortcuts].select_ship1");
    CHECK(current->binding == "CTRL-SPACE");
    CHECK(current->source_key == "[shortcuts].select_current");
  }

  // -------------------------------------------------------------------------
  // Empty / whitespace binding handling (issue #95)
  // -------------------------------------------------------------------------

  TEST_CASE("empty binding string disables the action via NONE sentinel")
  {
    const auto config = toml::parse(R"(
[shortcuts]
select_ship1 = ""
)");

    const auto bridge = input_binding::ResolveInputBindingConfig(config);
    const auto ship1  = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::SelectShip1;
    });

    REQUIRE(ship1 != bridge.bindings.end());
    CHECK(ship1->binding == "NONE");
    CHECK(ship1->source_key == "[shortcuts].select_ship1");
  }

  TEST_CASE("whitespace-only binding string is treated as empty (NONE sentinel)")
  {
    const auto config = toml::parse(R"(
[shortcuts]
select_ship1 = "   "
)");

    const auto bridge = input_binding::ResolveInputBindingConfig(config);
    const auto ship1  = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::SelectShip1;
    });

    REQUIRE(ship1 != bridge.bindings.end());
    CHECK(ship1->binding == "NONE");
  }

  TEST_CASE("array binding ignores empty items, keeps valid items, and warns")
  {
    const auto config = toml::parse(R"(
[shortcuts]
select_ship1 = ["", "1"]
)");

    const auto bridge = input_binding::ResolveInputBindingConfig(config);
    const auto ship1  = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::SelectShip1;
    });

    REQUIRE(ship1 != bridge.bindings.end());
    CHECK(ship1->binding == "1");

    const bool warned_for_empty_item =
        std::ranges::any_of(bridge.compatibility_warnings, [](const std::string& warning) {
          return warning.find("select_ship1[0]") != std::string::npos
                 && warning.find("empty after trimming") != std::string::npos;
        });
    CHECK(warned_for_empty_item);
  }

  TEST_CASE("array binding with no valid items disables the action and warns")
  {
    const auto config = toml::parse(R"(
[shortcuts]
select_ship1 = ["", "   "]
)");

    const auto bridge = input_binding::ResolveInputBindingConfig(config);
    const auto ship1  = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::SelectShip1;
    });

    REQUIRE(ship1 != bridge.bindings.end());
    CHECK(ship1->binding == "NONE");
    CHECK(ship1->source_key == "[shortcuts].select_ship1");

    const bool warned_for_no_valid_items =
        std::ranges::any_of(bridge.compatibility_warnings, [](const std::string& warning) {
          return warning.find("select_ship1") != std::string::npos
                 && warning.find("no valid string items") != std::string::npos;
        });
    CHECK(warned_for_no_valid_items);
  }

  TEST_CASE("explicit invalid binding string disables instead of falling back to default")
  {
    const auto config = toml::parse(R"(
[shortcuts]
zoom_preset1 = "NOT_A_KEY"
)");

    const auto bridge       = input_binding::ResolveInputBindingConfig(config);
    const auto zoom_preset1 = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::ZoomPreset1;
    });

    REQUIRE(zoom_preset1 != bridge.bindings.end());
    CHECK(zoom_preset1->binding == "NONE");
    CHECK(zoom_preset1->source_key == "[shortcuts].zoom_preset1");
  }

  TEST_CASE("config bridge accepts migrated chat and officer canvas aliases")
  {
    const auto config = toml::parse(R"(
[shortcuts]
show_chat = "SHIFT-C"
show_chatside1 = "ALT-C"
select_chatglobal = "CTRL-4"
move_left = "LEFT"
move_right = "RIGHT"
)");

    const auto bridge    = input_binding::ResolveInputBindingConfig(config);
    const auto show_chat = std::ranges::find_if(
        bridge.bindings, [](const auto& binding) { return binding.action == input_binding::InputActionId::ShowChat; });
    const auto side_chat   = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::ShowChatSide1;
    });
    const auto chat_global = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::SelectChatGlobal;
    });
    const auto move_left   = std::ranges::find_if(
        bridge.bindings, [](const auto& binding) { return binding.action == input_binding::InputActionId::MoveLeft; });
    const auto move_right = std::ranges::find_if(
        bridge.bindings, [](const auto& binding) { return binding.action == input_binding::InputActionId::MoveRight; });

    REQUIRE(show_chat != bridge.bindings.end());
    REQUIRE(side_chat != bridge.bindings.end());
    REQUIRE(chat_global != bridge.bindings.end());
    REQUIRE(move_left != bridge.bindings.end());
    REQUIRE(move_right != bridge.bindings.end());
    CHECK(show_chat->binding == "SHIFT-C");
    CHECK(show_chat->source_key == "[shortcuts].show_chat");
    CHECK(side_chat->binding == "ALT-C");
    CHECK(side_chat->source_key == "[shortcuts].show_chatside1");
    CHECK(chat_global->binding == "CTRL-4");
    CHECK(chat_global->source_key == "[shortcuts].select_chatglobal");
    CHECK(move_left->binding == "LEFT");
    CHECK(move_left->source_key == "[shortcuts].move_left");
    CHECK(move_right->binding == "RIGHT");
    CHECK(move_right->source_key == "[shortcuts].move_right");
  }

  TEST_CASE("dispatcher snapshot generates candidates and respects layer filtering")
  {
    const auto       compiled = input_binding::CompileBindingSet();
    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::Alpha1, {}, true, false},
    };

    auto plan = input_binding::PlanDispatchSnapshot(compiled, input_binding::InputPhase::Frame,
                                                    input_binding::ActiveLayers::All(), key_states);
    REQUIRE(plan.candidates.size() == 1);
    REQUIRE(plan.winners.size() == 1);
    CHECK(plan.candidates[0].action == input_binding::InputActionId::SelectShip1);
    CHECK(plan.winners[0].action == input_binding::InputActionId::SelectShip1);

    plan = input_binding::PlanDispatchSnapshot(compiled, input_binding::InputPhase::Frame,
                                               input_binding::ActiveLayers::Only(input_binding::InputLayer::Global),
                                               key_states);
    CHECK(plan.candidates.empty());
    CHECK(plan.winners.empty());
  }

  TEST_CASE("dispatcher watched keys and conflict winners cover migrated ship selection")
  {
    const auto       compiled = input_binding::CompileBindingSet();
    const std::array watched_actions{
        input_binding::InputActionId::SelectShip1,
        input_binding::InputActionId::SelectCurrent,
    };

    const auto watched_keys =
        input_binding::WatchedKeysForActions(compiled, input_binding::InputPhase::Frame, watched_actions);
    CHECK(std::ranges::find(watched_keys, KeyCode::Alpha1) != watched_keys.end());
    CHECK(std::ranges::find(watched_keys, KeyCode::Space) != watched_keys.end());

    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::Alpha1, {}, true, false},
        input_binding::DispatchKeyState{KeyCode::Space, {}, true, false},
    };

    const auto plan = input_binding::PlanDispatchSnapshot(compiled, input_binding::InputPhase::Frame,
                                                          input_binding::ActiveLayers::All(), key_states);
    REQUIRE(plan.candidates.size() == 4);
    REQUIRE(plan.winners.size() == 3);
    CHECK(plan.winners[0].action == input_binding::InputActionId::SelectShip1);
    CHECK(plan.winners[1].action == input_binding::InputActionId::FleetQueueAdd);
    CHECK(plan.winners[2].action == input_binding::InputActionId::FleetRecallCancel);
  }

  TEST_CASE("dispatcher watches remaining frame router actions and keeps context groups independent")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::ShowChat, "B"},
        input_binding::BindingOverride{input_binding::InputActionId::ShowBookmarks, "B"},
        input_binding::BindingOverride{input_binding::InputActionId::MoveLeft, "B"},
    };
    const auto compiled = input_binding::CompileBindingSet(overrides);
    CHECK_FALSE(compiled.has_errors());

    const std::array watched_actions{
        input_binding::InputActionId::ShowChat,
        input_binding::InputActionId::SelectChatGlobal,
        input_binding::InputActionId::MoveLeft,
        input_binding::InputActionId::ShowBookmarks,
    };
    const auto watched_keys =
        input_binding::WatchedKeysForActions(compiled, input_binding::InputPhase::Frame, watched_actions);
    CHECK(std::ranges::find(watched_keys, KeyCode::B) != watched_keys.end());
    CHECK(std::ranges::find(watched_keys, KeyCode::Alpha1) != watched_keys.end());

    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::B, {}, true, false},
    };
    const auto plan = input_binding::PlanDispatchSnapshot(compiled, input_binding::InputPhase::Frame,
                                                          input_binding::ActiveLayers::All(), key_states);

    CHECK(std::ranges::any_of(
        plan.winners, [](const auto& winner) { return winner.action == input_binding::InputActionId::ShowChat; }));
    CHECK(std::ranges::any_of(
        plan.winners, [](const auto& winner) { return winner.action == input_binding::InputActionId::ShowBookmarks; }));
    CHECK(std::ranges::any_of(
        plan.winners, [](const auto& winner) { return winner.action == input_binding::InputActionId::MoveLeft; }));
  }

  TEST_CASE("config bridge prefers canonical input bindings over legacy shortcuts")
  {
    const auto config = toml::parse(R"(
[input.bindings]
fleet_primary = ["CTRL-A", "MOUSE4"]

[shortcuts]
action_primary = "SPACE|MOUSE1"
)");

    const auto bridge        = input_binding::ResolveInputBindingConfig(config);
    const auto fleet_primary = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::FleetPrimary;
    });
    REQUIRE(fleet_primary != bridge.bindings.end());
    CHECK(fleet_primary->binding == "CTRL-A|MOUSE4");
    CHECK(fleet_primary->source_key == "[input.bindings].fleet_primary");
    CHECK_FALSE(bridge.compatibility_warnings.empty());
  }

  TEST_CASE("config bridge treats explicit empty strings as unbound instead of falling back to defaults")
  {
    const auto config = toml::parse(R"(
[input.bindings]
show_bookmarks = ""
)");

    const auto bridge         = input_binding::ResolveInputBindingConfig(config);
    const auto show_bookmarks = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::ShowBookmarks;
    });

    REQUIRE(show_bookmarks != bridge.bindings.end());
    CHECK(show_bookmarks->binding == "NONE");
    CHECK(show_bookmarks->source_key == "[input.bindings].show_bookmarks");
    CHECK(bridge.compatibility_warnings.empty());

    const auto       compiled = input_binding::CompileBindingSet(bridge.AsOverrides());
    const std::array key_states{
        input_binding::DispatchKeyState{KeyCode::B, {}, true, false},
    };
    const auto plan = input_binding::PlanDispatchSnapshot(compiled, input_binding::InputPhase::Frame,
                                                          input_binding::ActiveLayers::All(), key_states);

    CHECK_FALSE(
        plan.winner_lookup.Contains(input_binding::InputActionId::ShowBookmarks, input_binding::InputLayer::Global));
  }

  TEST_CASE("config bridge resolves legacy fleet aliases to separate canonical actions")
  {
    const auto config = toml::parse(R"(
[shortcuts]
action_primary = "CTRL-A"
action_queue = "Q"
action_recall_cancel = "SPACE|MOUSE1"
)");

    const auto bridge              = input_binding::ResolveInputBindingConfig(config);
    const auto fleet_primary       = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::FleetPrimary;
    });
    const auto fleet_queue_add     = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::FleetQueueAdd;
    });
    const auto fleet_recall_cancel = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::FleetRecallCancel;
    });
    REQUIRE(fleet_primary != bridge.bindings.end());
    REQUIRE(fleet_queue_add != bridge.bindings.end());
    REQUIRE(fleet_recall_cancel != bridge.bindings.end());
    CHECK(fleet_primary->binding == "CTRL-A");
    CHECK(fleet_primary->source_key == "[shortcuts].action_primary");
    CHECK(fleet_queue_add->binding == "Q");
    CHECK(fleet_queue_add->source_key == "[shortcuts].action_queue");
    CHECK(fleet_recall_cancel->binding == "SPACE|MOUSE1");
    CHECK(fleet_recall_cancel->source_key == "[shortcuts].action_recall_cancel");
    CHECK(bridge.compatibility_warnings.empty());
  }

  TEST_CASE("config bridge accepts hotkeys enable compatibility alias")
  {
    const auto config = toml::parse(R"(
[shortcuts]
set_hotkeys_enabled = "CTRL-ALT-F1"
)");

    const auto bridge         = input_binding::ResolveInputBindingConfig(config);
    const auto hotkeys_enable = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::HotkeysEnable;
    });
    REQUIRE(hotkeys_enable != bridge.bindings.end());
    CHECK(hotkeys_enable->binding == "CTRL-ALT-F1");
    CHECK(hotkeys_enable->source_key == "[shortcuts].set_hotkeys_enabled");
    REQUIRE(bridge.compatibility_warnings.size() == 1);
  }

  TEST_CASE("config bridge accepts migrated zoom aliases")
  {
    const auto config = toml::parse(R"(
[shortcuts]
zoom_preset1 = "CTRL-F1"
zoom_min = "BACKSPACE"
set_zoom_default = "CTRL-="
)");

    const auto bridge       = input_binding::ResolveInputBindingConfig(config);
    const auto zoom_preset1 = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::ZoomPreset1;
    });
    const auto zoom_min     = std::ranges::find_if(
        bridge.bindings, [](const auto& binding) { return binding.action == input_binding::InputActionId::ZoomMin; });
    const auto set_zoom_default = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::SetZoomDefault;
    });

    REQUIRE(zoom_preset1 != bridge.bindings.end());
    REQUIRE(zoom_min != bridge.bindings.end());
    REQUIRE(set_zoom_default != bridge.bindings.end());
    CHECK(zoom_preset1->binding == "CTRL-F1");
    CHECK(zoom_preset1->source_key == "[shortcuts].zoom_preset1");
    CHECK(zoom_min->binding == "BACKSPACE");
    CHECK(zoom_min->source_key == "[shortcuts].zoom_min");
    CHECK(set_zoom_default->binding == "CTRL-=");
    CHECK(set_zoom_default->source_key == "[shortcuts].set_zoom_default");
  }

  TEST_CASE("config bridge runtime config emits canonical bindings and sources")
  {
    const auto config = toml::parse(R"(
[input.bindings]
fleet_service = "CTRL-R"
)");

    const auto bridge  = input_binding::ResolveInputBindingConfig(config);
    const auto compile = input_binding::CompileBindingSet(bridge.AsOverrides());
    const auto runtime = input_binding::BuildInputBindingRuntimeConfig(bridge, compile);

    REQUIRE(runtime["bindings"].as_table() != nullptr);
    REQUIRE(runtime["binding_sources"].as_table() != nullptr);
    CHECK(runtime["bindings"]["fleet_service"].value_or(std::string{}) == "CTRL-R");
    CHECK(runtime["binding_sources"]["fleet_service"].value_or(std::string{}) == "[input.bindings].fleet_service");
  }
}

// ===========================================================================
// fleet_input_policy
// ===========================================================================
