#include "test_pure_common.h"

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

    for (const auto& spec : specs) {
      const auto binding = input_binding::ParseBinding(spec.default_bind);
      INFO(spec.canonical_key);
      CHECK(binding.has_valid_chord());
      CHECK_FALSE(binding.has_warnings());
      CHECK_FALSE(binding.has_errors());
    }
  }

  TEST_CASE("key lookup covers keyboard mouse and function keys")
  {
    CHECK(input_binding::LookupKey("space") == KeyCode::Space);
    CHECK(input_binding::LookupKey("V") == KeyCode::V);
    CHECK(input_binding::LookupKey("MOUSE1") == KeyCode::Mouse1);
    CHECK(input_binding::LookupKey("F12") == KeyCode::F12);
    CHECK_FALSE(input_binding::LookupKey("NOT_A_KEY").has_value());
  }

  TEST_CASE("modifier masks satisfy logical and physical requirements")
  {
    const auto ctrl = input_binding::ModifierMask::Logical(input_binding::ModifierGroup::Ctrl);
    auto held_left_ctrl = input_binding::ModifierMask::FromPressedKey(KeyCode::LeftControl);
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

    const auto held = input_binding::ModifierMask{};
    auto matches = index.Match(input_binding::TriggerMode::Down, KeyCode::Space, held);
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

    CHECK(compiled.bound_chord_count == 58);
    CHECK(compiled.index.size() == 58);
    CHECK_FALSE(compiled.has_warnings());
    CHECK_FALSE(compiled.has_errors());
    CHECK_FALSE(compiled.has_conflicts());

    const auto held = input_binding::ModifierMask{};
    auto matches = compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Space, held);
    REQUIRE(matches.size() == 1);
    CHECK(matches[0] == input_binding::InputActionId::FleetPrimary);

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
    CHECK(compiled.bound_chord_count == 56);

    const auto matches = compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Space,
                                              input_binding::ModifierMask{});
    CHECK(matches.empty());
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
        input_binding::BindingOverride{input_binding::InputActionId::FleetService, "SPACE"},
    };

    const auto compiled = input_binding::CompileBindingSet(overrides);
    CHECK(compiled.has_errors());
    CHECK(compiled.has_conflicts());
    REQUIRE(compiled.conflicts.size() == 1);
    CHECK(compiled.conflicts[0].action_a == input_binding::InputActionId::FleetPrimary);
    CHECK(compiled.conflicts[0].action_b == input_binding::InputActionId::FleetService);

    const auto matches = compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Space,
                                              input_binding::ModifierMask{});
    REQUIRE(matches.size() == 2);
    CHECK(matches[0] == input_binding::InputActionId::FleetPrimary);
    CHECK(matches[1] == input_binding::InputActionId::FleetService);
  }

  TEST_CASE("compiler reports logical and physical modifier overlaps")
  {
    const std::array overrides{
        input_binding::BindingOverride{input_binding::InputActionId::FleetPrimary, "CTRL-SPACE"},
        input_binding::BindingOverride{input_binding::InputActionId::FleetService, "LCTRL-SPACE"},
    };

    const auto compiled = input_binding::CompileBindingSet(overrides);
    CHECK(compiled.has_errors());
    CHECK(compiled.has_conflicts());
    REQUIRE(compiled.conflicts.size() == 1);

    const auto held = input_binding::ModifierMask::FromPressedKey(KeyCode::LeftControl);
    const auto matches = compiled.index.Match(input_binding::TriggerMode::Down, KeyCode::Space, held);
    REQUIRE(matches.size() == 2);
    CHECK(matches[0] == input_binding::InputActionId::FleetPrimary);
    CHECK(matches[1] == input_binding::InputActionId::FleetService);
  }

  TEST_CASE("config bridge resolves defaults into canonical bindings")
  {
    const toml::table config;
    const auto bridge = input_binding::ResolveInputBindingConfig(config);

    const auto fleet_primary = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::FleetPrimary;
    });
    REQUIRE(fleet_primary != bridge.bindings.end());
    CHECK(fleet_primary->binding == "SPACE|MOUSE1");
    CHECK(fleet_primary->source_key == "default");
    CHECK(bridge.compatibility_warnings.empty());
  }

  TEST_CASE("config bridge prefers canonical input bindings over legacy shortcuts")
  {
    const auto config = toml::parse(R"(
[input.bindings]
fleet_primary = ["CTRL-A", "MOUSE4"]

[shortcuts]
action_primary = "SPACE|MOUSE1"
)");

    const auto bridge = input_binding::ResolveInputBindingConfig(config);
    const auto fleet_primary = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::FleetPrimary;
    });
    REQUIRE(fleet_primary != bridge.bindings.end());
    CHECK(fleet_primary->binding == "CTRL-A|MOUSE4");
    CHECK(fleet_primary->source_key == "[input.bindings].fleet_primary");
    CHECK_FALSE(bridge.compatibility_warnings.empty());
  }

  TEST_CASE("config bridge resolves legacy fleet aliases with precedence and conflict warnings")
  {
    const auto config = toml::parse(R"(
[shortcuts]
action_primary = "CTRL-A"
action_queue = "Q"
action_recall_cancel = "SPACE|MOUSE1"
)");

    const auto bridge = input_binding::ResolveInputBindingConfig(config);
    const auto fleet_primary = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::FleetPrimary;
    });
    REQUIRE(fleet_primary != bridge.bindings.end());
    CHECK(fleet_primary->binding == "CTRL-A");
    CHECK(fleet_primary->source_key == "[shortcuts].action_primary");
    CHECK(bridge.compatibility_warnings.size() >= 2);
  }

  TEST_CASE("config bridge accepts hotkeys enable compatibility alias")
  {
    const auto config = toml::parse(R"(
[shortcuts]
set_hotkeys_enabled = "CTRL-ALT-F1"
)");

    const auto bridge = input_binding::ResolveInputBindingConfig(config);
    const auto hotkeys_enable = std::ranges::find_if(bridge.bindings, [](const auto& binding) {
      return binding.action == input_binding::InputActionId::HotkeysEnable;
    });
    REQUIRE(hotkeys_enable != bridge.bindings.end());
    CHECK(hotkeys_enable->binding == "CTRL-ALT-F1");
    CHECK(hotkeys_enable->source_key == "[shortcuts].set_hotkeys_enabled");
    REQUIRE(bridge.compatibility_warnings.size() == 1);
  }

  TEST_CASE("config bridge runtime config emits canonical bindings and sources")
  {
    const auto config = toml::parse(R"(
[input.bindings]
fleet_service = "CTRL-R"
)");

    const auto bridge = input_binding::ResolveInputBindingConfig(config);
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
