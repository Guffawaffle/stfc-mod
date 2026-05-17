#include "test_pure_common.h"

#include "config.h"

// ===========================================================================
// config_schema
// ===========================================================================

TEST_SUITE("config_schema")
{
  TEST_CASE("canonical boolean path reads nested TOML")
  {
    auto config = toml::parse(R"(
[notifications.system]
enabled = true
)");

    config_schema::BoolSetting setting{
        "notifications.system.enabled",
        false,
        {},
        "Master switch for OS notifications.",
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
        "Master switch for OS notifications.",
    };

    auto result = config_schema::read_bool(config, setting);
    CHECK(result.value == true);
    CHECK(result.used_default == false);
    CHECK(result.source_path == "notifications.notifications_enabled");
    REQUIRE(result.diagnostics.size() == 1);
    CHECK(result.diagnostics[0].source_path == "notifications.notifications_enabled");
  }

  TEST_CASE("canonical boolean path wins over deprecated alias")
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
        "Master switch for OS notifications.",
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
        "Master switch for OS notifications.",
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

  TEST_CASE("notification policy inline event table overrides legacy booleans")
  {
    auto config = toml::parse(R"(
[notifications.audio]
default_sound = "soft"

[notifications.events.fleet]
arrived_in_system = { system = false, audio = true, sound = "arrival" }
repair_complete = { system = true, audio = true, sound = "repair" }
)"
    );

    NotificationConfig notifications;
    notifications.enabled                       = true;
    notifications.audio_enabled                 = true;
    notifications.fleet_arrived_in_system       = true;
    notifications.audio_fleet_arrived_in_system = false;
    notifications.fleet_repair_complete         = false;

    toml::table runtime;
    notification_policy_load(config, runtime, notifications);

    const auto arrival = notification_policy_for(NotificationKind::FleetArrivedInSystem);
    CHECK_FALSE(arrival.system);
    CHECK(arrival.audio);
    CHECK(arrival.sound == NotificationSound::Arrival);

    const auto repair = notification_policy_for(NotificationKind::FleetRepairComplete);
    CHECK(repair.system);
    CHECK(repair.audio);
    CHECK(repair.sound == NotificationSound::Repair);
    CHECK(runtime["notifications"]["events"]["fleet"]["repair_complete"]["sound"].value_or(std::string{})
          == "repair");
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
