#include "test_pure_common.h"

#include "config.h"

#include <set>

// ===========================================================================
// config_schema
// ===========================================================================

TEST_SUITE("config_schema")
{
  TEST_CASE("preferred boolean path reads nested TOML")
  {
    auto config = toml::parse(R"(
[notifications.system]
enabled = true
)");

    config_schema::BoolSetting setting{
        "notifications.system.enabled",
        false,
        {},
        "Deprecated compatibility gate for OS notifications.",
    };

    auto result = config_schema::read_bool(config, setting);
    CHECK(result.value == true);
    CHECK(result.used_default == false);
    CHECK(result.source_path == "notifications.system.enabled");
    CHECK(result.diagnostics.empty());
  }

  TEST_CASE("deprecated boolean alias remains compatible")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_enabled = true
)");

    const std::array<std::string_view, 1> aliases{"notifications.notifications_enabled"};
    config_schema::BoolSetting            setting{
        "notifications.system.enabled",
        false,
        aliases,
        "Deprecated compatibility gate for OS notifications.",
    };

    auto result = config_schema::read_bool(config, setting);
    CHECK(result.value == true);
    CHECK(result.used_default == false);
    CHECK(result.source_path == "notifications.notifications_enabled");
    REQUIRE(result.diagnostics.size() == 1);
    CHECK(result.diagnostics[0].source_path == "notifications.notifications_enabled");
  }

  TEST_CASE("preferred compatibility path wins over older flat alias")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_enabled = false

[notifications.system]
enabled = true
)");

    const std::array<std::string_view, 1> aliases{"notifications.notifications_enabled"};
    config_schema::BoolSetting            setting{
        "notifications.system.enabled",
        false,
        aliases,
        "Deprecated compatibility gate for OS notifications.",
    };

    auto result = config_schema::read_bool(config, setting);
    CHECK(result.value == true);
    CHECK(result.source_path == "notifications.system.enabled");
    REQUIRE(result.diagnostics.size() == 1);
    CHECK(result.diagnostics[0].source_path == "notifications.notifications_enabled");
  }

  TEST_CASE("invalid boolean type falls back to default")
  {
    auto config = toml::parse(R"(
[notifications.system]
enabled = "yes"
)");

    config_schema::BoolSetting setting{
        "notifications.system.enabled",
        false,
        {},
        "Deprecated compatibility gate for OS notifications.",
    };

    auto result = config_schema::read_bool(config, setting);
    CHECK(result.value == false);
    CHECK(result.used_default == true);
    REQUIRE(result.diagnostics.size() == 1);
    CHECK(result.diagnostics[0].source_path == "notifications.system.enabled");
  }

  TEST_CASE("write boolean emits nested TOML path")
  {
    toml::table config;
    config_schema::write_bool(config, "notifications.audio.fleet.arrived_in_system", true);
    CHECK(config["notifications"]["audio"]["fleet"]["arrived_in_system"].value_or(false) == true);
  }

  TEST_CASE("notification catalog has complete unique canonical keys and explicit sounds")
  {
    std::set<std::string_view> keys;
    CHECK(notification_event_catalog().size() == static_cast<size_t>(NotificationKind::Count));

    for (const auto& spec : notification_event_catalog()) {
      CHECK(keys.insert(spec.canonical_key).second);
      CHECK_FALSE(spec.canonical_key.starts_with("notifications_"));
      CHECK(spec.catalog_sound != NotificationSound::None);
    }

    CHECK(keys.contains("armada_created"));
    CHECK(keys.contains("fleet_arrived_in_system"));
    CHECK(keys.contains("fleet_repair_complete"));
  }

  TEST_CASE("canonical booleans and inline policies have complete event semantics")
  {
    auto config = toml::parse(R"(
[notifications]
victory = true
defeat = false
fleet_arrived_in_system = { system = false, audio = true, sound = "arrival" }
)");

    toml::table runtime;
    notification_policy_load(config, runtime, NotificationConfig{});

    const auto victory = notification_policy_for(NotificationKind::BattleVictory);
    CHECK(victory.system);
    CHECK_FALSE(victory.audio);
    CHECK(victory.sound == NotificationSound::Success);

    const auto defeat = notification_policy_for(NotificationKind::BattleDefeat);
    CHECK_FALSE(defeat.system);
    CHECK_FALSE(defeat.audio);

    const auto arrival = notification_policy_for(NotificationKind::FleetArrivedInSystem);
    CHECK_FALSE(arrival.system);
    CHECK(arrival.audio);
    CHECK(arrival.sound == NotificationSound::Arrival);

    CHECK(runtime["notifications"]["victory"]["system"].value_or(false));
    CHECK(runtime["notifications"]["fleet_arrived_in_system"]["audio"].value_or(false));
    CHECK(runtime["notifications"]["provenance"]["victory"]["source_kind"].value_or(std::string{}) == "canonical");
  }

  TEST_CASE("every catalog event accepts false true and extended canonical policies")
  {
    toml::table true_notifications;
    for (const auto& spec : notification_event_catalog()) {
      true_notifications.insert_or_assign(spec.canonical_key, true);
    }

    toml::table true_config;
    true_config.insert_or_assign("notifications", std::move(true_notifications));
    toml::table runtime;
    notification_policy_load(true_config, runtime, NotificationConfig{});

    for (const auto& spec : notification_event_catalog()) {
      const auto& policy = notification_policy_for(spec.kind);
      CHECK(policy.system);
      CHECK_FALSE(policy.audio);
      CHECK(policy.sound == spec.catalog_sound);
    }

    toml::table false_notifications;
    for (const auto& spec : notification_event_catalog()) {
      false_notifications.insert_or_assign(spec.canonical_key, false);
    }
    toml::table false_config;
    false_config.insert_or_assign("notifications", std::move(false_notifications));
    notification_policy_load(false_config, runtime, NotificationConfig{});

    for (const auto& spec : notification_event_catalog()) {
      CHECK_FALSE(notification_policy_has_delivery(spec.kind));
    }

    toml::table extended_notifications;
    for (const auto& spec : notification_event_catalog()) {
      toml::table policy;
      policy.insert_or_assign("system", true);
      policy.insert_or_assign("audio", true);
      policy.insert_or_assign("sound", notification_sound_name(spec.catalog_sound));
      policy.is_inline(true);
      extended_notifications.insert_or_assign(spec.canonical_key, std::move(policy));
    }
    toml::table extended_config;
    extended_config.insert_or_assign("notifications", std::move(extended_notifications));
    notification_policy_load(extended_config, runtime, NotificationConfig{});

    for (const auto& spec : notification_event_catalog()) {
      const auto& policy = notification_policy_for(spec.kind);
      CHECK(policy.system);
      CHECK(policy.audio);
      CHECK(policy.sound == spec.catalog_sound);
    }
  }

  TEST_CASE("every catalog event retains deprecated event-table and flat policy compatibility")
  {
    toml::table events;
    for (const auto& spec : notification_event_catalog()) {
      if (!events.contains(spec.legacy_category)) {
        events.insert_or_assign(spec.legacy_category, toml::table{});
      }

      toml::table policy;
      policy.insert_or_assign("system", true);
      policy.insert_or_assign("audio", true);
      policy.insert_or_assign("sound", notification_sound_name(spec.catalog_sound));
      policy.is_inline(true);
      events[spec.legacy_category].as_table()->insert_or_assign(spec.legacy_key, std::move(policy));
    }

    toml::table event_notifications;
    event_notifications.insert_or_assign("events", std::move(events));
    toml::table event_config;
    event_config.insert_or_assign("notifications", std::move(event_notifications));

    toml::table runtime;
    notification_policy_load(event_config, runtime, NotificationConfig{});
    for (const auto& spec : notification_event_catalog()) {
      const auto& policy = notification_policy_for(spec.kind);
      CHECK(policy.system);
      CHECK(policy.audio);
      CHECK(policy.sound == spec.catalog_sound);
    }

    toml::table flat_notifications;
    for (const auto& spec : notification_event_catalog()) {
      flat_notifications.insert_or_assign("notifications_" + std::string(spec.canonical_key), true);
      flat_notifications.insert_or_assign("notifications_audio_" + std::string(spec.canonical_key), true);
    }
    toml::table flat_config;
    flat_config.insert_or_assign("notifications", std::move(flat_notifications));

    notification_policy_load(flat_config, runtime, NotificationConfig{});
    for (const auto& spec : notification_event_catalog()) {
      const auto& policy = notification_policy_for(spec.kind);
      CHECK(policy.system);
      CHECK(policy.audio);
      CHECK(policy.sound == spec.catalog_sound);
    }
  }

  TEST_CASE("canonical policy ignores deprecated master gates")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_enabled = false
notifications_audio_enabled = false
victory = { system = true, audio = true, sound = "success" }
)");

    toml::table runtime;
    notification_policy_load(config, runtime, NotificationConfig{});

    const auto victory = notification_policy_for(NotificationKind::BattleVictory);
    CHECK(victory.system);
    CHECK(victory.audio);
    CHECK(victory.sound == NotificationSound::Success);
    CHECK(runtime["notifications"]["provenance"]["victory"]["deprecated_inputs"].value_or(false));
  }

  TEST_CASE("canonical inline policy never inherits deprecated event values")
  {
    auto config = toml::parse(R"(
[notifications]
fleet_arrived_in_system = { audio = true }
notifications_fleet_arrived_in_system = true

[notifications.events.fleet]
arrived_in_system = { system = true, audio = false, sound = "alarm" }
)");

    NotificationConfig legacy;
    legacy.fleet_arrived_in_system = true;

    toml::table runtime;
    notification_policy_load(config, runtime, legacy);

    const auto arrival = notification_policy_for(NotificationKind::FleetArrivedInSystem);
    CHECK_FALSE(arrival.system);
    CHECK(arrival.audio);
    CHECK(arrival.sound == NotificationSound::Arrival);
    CHECK(runtime["notifications"]["provenance"]["fleet_arrived_in_system"]["deprecated_inputs"].value_or(false));
    CHECK(runtime["notifications"]["provenance"]["fleet_arrived_in_system"]["removal_target"].value_or(std::string{})
          == "3.0.0");
    const auto* ignored_sources =
        runtime["notifications"]["provenance"]["fleet_arrived_in_system"]["ignored_sources"].as_array();
    REQUIRE(ignored_sources != nullptr);
    CHECK(ignored_sources->size() >= 2);
  }

  TEST_CASE("invalid canonical values fall back to safe event catalog fields")
  {
    auto config = toml::parse(R"(
[notifications]
victory = "yes"
notifications_victory = true
defeat = { system = "yes", audio = true, sound = "klaxon", mystery = true }
)");

    toml::table runtime;
    notification_policy_load(config, runtime, NotificationConfig{});

    const auto victory = notification_policy_for(NotificationKind::BattleVictory);
    CHECK_FALSE(victory.system);
    CHECK_FALSE(victory.audio);
    CHECK(victory.sound == NotificationSound::Success);

    const auto defeat = notification_policy_for(NotificationKind::BattleDefeat);
    CHECK_FALSE(defeat.system);
    CHECK(defeat.audio);
    CHECK(defeat.sound == NotificationSound::Warning);
    CHECK(runtime["notifications"]["provenance"]["victory"]["source_kind"].value_or(std::string{})
          == "invalid-canonical-fallback");
    CHECK(runtime["notifications"]["provenance"]["defeat"]["diagnostic_count"].value_or(0) == 3);
    const auto* diagnostics = runtime["notifications"]["provenance"]["defeat"]["diagnostics"].as_array();
    REQUIRE(diagnostics != nullptr);
    CHECK(diagnostics->size() == 3);
    CHECK_FALSE(runtime["notifications"]["provenance"]["defeat"]["diagnostics_truncated"].value_or(true));
  }

  TEST_CASE("deprecated schema remains compatible and explicit masters only gate legacy delivery")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_enabled = false
notifications_audio_enabled = true
notifications_victory = true

[notifications.events.battle]
victory = { system = true, audio = true, sound = "ping" }
)");

    NotificationConfig legacy;
    legacy.SetToastStateEnabled(Victory, true);

    toml::table runtime;
    notification_policy_load(config, runtime, legacy);

    const auto victory = notification_policy_for(NotificationKind::BattleVictory);
    CHECK_FALSE(victory.system);
    CHECK(victory.audio);
    CHECK(victory.sound == NotificationSound::Ping);
    CHECK(runtime["notifications"]["provenance"]["victory"]["source_kind"]
              .value_or(std::string{})
              .starts_with("deprecated"));
    CHECK_FALSE(runtime["notifications"].as_table()->contains("notifications_enabled"));
    CHECK_FALSE(runtime["notifications"].as_table()->contains("events"));
  }

  TEST_CASE("deprecated fleet splits incoming aliases and UI allowlists remain compatible")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_incoming_attack = true
notifications_incoming_attack_faction = false

[notifications.system.fleet]
arrived_in_system = true
arrived_at_destination = true
started_mining = true
node_depleted = true
docked = true
repair_complete = true

[notifications.audio.fleet]
arrived_in_system = true

[ui]
notify_on_banner_types = "Victory"
)");

    NotificationConfig legacy;
    legacy.SetToastStateEnabled(Victory, true);

    toml::table runtime;
    notification_policy_load(config, runtime, legacy);

    CHECK(notification_policy_system_enabled(NotificationKind::BattleIncomingAttackPlayer));
    CHECK_FALSE(notification_policy_system_enabled(NotificationKind::BattleIncomingAttackHostile));
    CHECK(notification_policy_system_enabled(NotificationKind::BattleVictory));
    CHECK(notification_policy_system_enabled(NotificationKind::FleetArrivedInSystem));
    CHECK(notification_policy_audio_enabled(NotificationKind::FleetArrivedInSystem));
    CHECK(notification_policy_system_enabled(NotificationKind::FleetArrivedAtDestination));
    CHECK(notification_policy_system_enabled(NotificationKind::FleetStartedMining));
    CHECK(notification_policy_system_enabled(NotificationKind::FleetNodeDepleted));
    CHECK(notification_policy_system_enabled(NotificationKind::FleetDocked));
    CHECK(notification_policy_system_enabled(NotificationKind::FleetRepairComplete));

    auto deprecated_ui_alias = toml::parse(R"(
[ui]
notify_banner_types = "Victory"
)");
    notification_policy_load(deprecated_ui_alias, runtime, legacy);
    CHECK(notification_policy_system_enabled(NotificationKind::BattleVictory));
  }

  TEST_CASE("deprecated split master wins conflicts and records diagnostic facts")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_enabled = false
notifications_victory = true

[notifications.system]
enabled = true
)");

    toml::table runtime;
    notification_policy_load(config, runtime, NotificationConfig{});

    CHECK(notification_policy_system_enabled(NotificationKind::BattleVictory));
    CHECK(runtime["notifications"]["provenance"]["victory"]["conflict"].value_or(false));
    CHECK(runtime["notifications"]["provenance"]["victory"]["diagnostic_count"].value_or(0) >= 1);
    const auto* diagnostics = runtime["notifications"]["provenance"]["victory"]["diagnostics"].as_array();
    REQUIRE(diagnostics != nullptr);
    CHECK(diagnostics->size() >= 1);
  }

  TEST_CASE("sparse deprecated event input does not activate unrelated historical defaults")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_victory = false
)");

    NotificationConfig legacy;
    legacy.SetToastStateEnabled(Defeat, true);

    toml::table runtime;
    notification_policy_load(config, runtime, legacy);

    CHECK_FALSE(notification_policy_has_delivery(NotificationKind::BattleVictory));
    CHECK_FALSE(notification_policy_has_delivery(NotificationKind::BattleDefeat));
    CHECK(runtime["notifications"]["provenance"]["victory"]["source_kind"]
              .value_or(std::string{})
              .starts_with("deprecated"));
    CHECK(runtime["notifications"]["provenance"]["defeat"]["source_kind"].value_or(std::string{}) == "catalog-default");
  }

  TEST_CASE("deprecated master-only config retains historical event defaults")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_enabled = true
)");

    NotificationConfig legacy;
    legacy.SetToastStateEnabled(Defeat, true);

    toml::table runtime;
    notification_policy_load(config, runtime, legacy);

    CHECK(notification_policy_system_enabled(NotificationKind::BattleDefeat));
    CHECK(runtime["notifications"]["provenance"]["defeat"]["system_source"]
              .value_or(std::string{})
              .find("notifications.notifications_enabled=true")
          != std::string::npos);
  }

  TEST_CASE("deprecated standard event retains default cue without a global sound")
  {
    auto config = toml::parse(R"(
[notifications]
notifications_standard = true
)");

    toml::table runtime;
    notification_policy_load(config, runtime, NotificationConfig{});

    const auto standard = notification_policy_for(NotificationKind::ExperimentalStandard);
    CHECK(standard.system);
    CHECK(standard.sound == NotificationSound::Default);
  }

  TEST_CASE("incoming attack policy equivalence compares channels and effective sound")
  {
    auto config = toml::parse(R"(
[notifications]
incoming_attack_player = { system = false, audio = true, sound = "alarm" }
incoming_attack_hostile = { system = true, audio = false, sound = "warning" }
)");

    toml::table runtime;
    notification_policy_load(config, runtime, NotificationConfig{});
    CHECK_FALSE(notification_policy_delivery_equivalent(NotificationKind::BattleIncomingAttackPlayer,
                                                        NotificationKind::BattleIncomingAttackHostile));

    auto equivalent = toml::parse(R"(
[notifications]
incoming_attack_player = { system = true, audio = true, sound = "alarm" }
incoming_attack_hostile = { system = true, audio = true, sound = "alarm" }
)");
    notification_policy_load(equivalent, runtime, NotificationConfig{});
    CHECK(notification_policy_delivery_equivalent(NotificationKind::BattleIncomingAttackPlayer,
                                                  NotificationKind::BattleIncomingAttackHostile));
  }

  TEST_CASE("unknown root keys are warned into the runtime resolution summary")
  {
    auto config = toml::parse(R"(
[notifications]
fleet_arrived_in_sytem = true
)");

    toml::table runtime;
    notification_policy_load(config, runtime, NotificationConfig{});

    CHECK(runtime["notifications"]["resolution"]["unknown_root_key_count"].value_or(0) == 1);
    const auto* unknown_root_keys = runtime["notifications"]["resolution"]["unknown_root_keys"].as_array();
    REQUIRE(unknown_root_keys != nullptr);
    CHECK(unknown_root_keys->size() == 1);
    CHECK_FALSE(runtime["notifications"]["resolution"]["unknown_root_keys_truncated"].value_or(true));
  }

  TEST_CASE("runtime provenance marks bounded diagnostic and ignored-source truncation")
  {
    auto diagnostics_config = toml::parse(R"(
[notifications]
victory = { x1 = true, x2 = true, x3 = true, x4 = true, x5 = true, x6 = true, x7 = true, x8 = true, x9 = true, x10 = true }
)");

    toml::table runtime;
    notification_policy_load(diagnostics_config, runtime, NotificationConfig{});
    CHECK(runtime["notifications"]["provenance"]["victory"]["diagnostic_count"].value_or(0) == 10);
    const auto* diagnostics = runtime["notifications"]["provenance"]["victory"]["diagnostics"].as_array();
    REQUIRE(diagnostics != nullptr);
    CHECK(diagnostics->size() == 8);
    CHECK(runtime["notifications"]["provenance"]["victory"]["diagnostics_truncated"].value_or(false));

    auto ignored_config = toml::parse(R"(
[notifications]
incoming_attack_player = true
notifications_incoming_attack_player = true
notifications_audio_incoming_attack_player = true
notifications_incoming_attack = true
notifications_enabled = true
notifications_audio_enabled = true

[notifications.system]
enabled = true

[notifications.system.battle]
incoming_attack_player = true

[notifications.audio]
enabled = true

[notifications.audio.battle]
incoming_attack_player = true

[notifications.events.battle]
incoming_attack_player = { system = true, audio = true, sound = "alarm" }

[ui]
notify_on_banner_types = "IncomingAttack"
notify_banner_types = "IncomingAttack"
)");

    notification_policy_load(ignored_config, runtime, NotificationConfig{});
    const auto* ignored_sources =
        runtime["notifications"]["provenance"]["incoming_attack_player"]["ignored_sources"].as_array();
    REQUIRE(ignored_sources != nullptr);
    CHECK(ignored_sources->size() == 8);
    CHECK(
        runtime["notifications"]["provenance"]["incoming_attack_player"]["ignored_sources_truncated"].value_or(false));
  }

  TEST_CASE("fresh generated notification config contains only canonical disabled events")
  {
    toml::table generated;
    generated.insert_or_assign("notifications", toml::table{{"notifications_enabled", true}});

    notification_policy_prepare_generated_config(generated);

    const auto* notifications = generated["notifications"].as_table();
    REQUIRE(notifications != nullptr);
    CHECK(notifications->size() == notification_event_catalog().size());
    for (const auto& spec : notification_event_catalog()) {
      const auto* node = notifications->get(spec.canonical_key);
      REQUIRE(node != nullptr);
      CHECK(node->value<bool>().value_or(true) == false);
    }
    CHECK_FALSE(notifications->contains("notifications_enabled"));
    CHECK_FALSE(notifications->contains("system"));
    CHECK_FALSE(notifications->contains("audio"));
    CHECK_FALSE(notifications->contains("events"));
  }
}

TEST_SUITE("legacy_notification_allowlist")
{
  TEST_CASE("ALL token is accepted case-insensitively with surrounding whitespace")
  {
    CHECK(legacy_notification_allowlist_requests_all("ALL"));
    CHECK(legacy_notification_allowlist_requests_all(" all "));
    CHECK(legacy_notification_allowlist_requests_all("\tAll\r\n"));
    CHECK_FALSE(legacy_notification_allowlist_requests_all("FleetBattle"));
    CHECK_FALSE(legacy_notification_allowlist_requests_all("ALLIES"));
  }
}

// ===========================================================================
// battle_log_decoder
// ===========================================================================

TEST_SUITE("hotkey_disable_shortcut_alias")
{
  TEST_CASE("canonical key wins")
  {
    HotkeyDisableShortcutAliasInput input;
    input.has_canonical = true;
    input.canonical     = "CTRL-D";
    input.default_value = "CTRL-ALT-MINUS";

    const auto decision = resolve_hotkey_disable_shortcut_alias(input);
    CHECK(decision.key == "set_hotkeys_disable");
    CHECK(decision.source_key == "set_hotkeys_disable");
    CHECK(decision.value == "CTRL-D");
    CHECK_FALSE(decision.used_deprecated_alias);
    CHECK_FALSE(decision.has_conflicting_alias);
  }

  TEST_CASE("deprecated typo remains compatible")
  {
    HotkeyDisableShortcutAliasInput input;
    input.has_deprecated_typo = true;
    input.deprecated_typo     = "CTRL-T";
    input.default_value       = "CTRL-ALT-MINUS";

    const auto decision = resolve_hotkey_disable_shortcut_alias(input);
    CHECK(decision.key == "set_hotkeys_disable");
    CHECK(decision.source_key == "set_hotkeys_disble");
    CHECK(decision.value == "CTRL-T");
    CHECK(decision.used_deprecated_alias);
    CHECK(decision.saw_deprecated_alias);
    CHECK_FALSE(decision.has_conflicting_alias);
  }

  TEST_CASE("conflicting canonical and deprecated values use canonical")
  {
    HotkeyDisableShortcutAliasInput input;
    input.has_canonical       = true;
    input.canonical           = "CTRL-D";
    input.has_deprecated_typo = true;
    input.deprecated_typo     = "CTRL-T";
    input.default_value       = "CTRL-ALT-MINUS";

    const auto decision = resolve_hotkey_disable_shortcut_alias(input);
    CHECK(decision.source_key == "set_hotkeys_disable");
    CHECK(decision.value == "CTRL-D");
    CHECK_FALSE(decision.used_deprecated_alias);
    CHECK(decision.saw_deprecated_alias);
    CHECK(decision.has_conflicting_alias);
  }

  TEST_CASE("legacy disabled spelling is accepted as an alias")
  {
    HotkeyDisableShortcutAliasInput input;
    input.has_legacy_disabled = true;
    input.legacy_disabled     = "CTRL-L";
    input.default_value       = "CTRL-ALT-MINUS";

    const auto decision = resolve_hotkey_disable_shortcut_alias(input);
    CHECK(decision.source_key == "set_hotkeys_disabled");
    CHECK(decision.value == "CTRL-L");
    CHECK(decision.used_deprecated_alias);
  }
}

// ===========================================================================
// bounded TTL deduper
// ===========================================================================
