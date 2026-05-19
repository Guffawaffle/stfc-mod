#include "test_pure_common.h"

// ===========================================================================
// notifications_and_dedupe
// ===========================================================================

TEST_SUITE("bounded_ttl_deduper")
{
  using TestDeduper = BoundedTtlDeduper<std::string>;

  TEST_CASE("suppresses repeated key inside TTL and emits at boundary")
  {
    auto        at_ms = [](int64_t ms) { return TestDeduper::time_point{std::chrono::milliseconds(ms)}; };
    TestDeduper deduper(8);
    const auto  ttl = std::chrono::milliseconds(100);

    CHECK(deduper.should_emit("toast-1", at_ms(0), ttl).emitted);

    const auto duplicate = deduper.should_emit("toast-1", at_ms(99), ttl);
    CHECK_FALSE(duplicate.emitted);
    CHECK(duplicate.suppressed_by_window);

    const auto boundary = deduper.should_emit("toast-1", at_ms(100), ttl);
    CHECK(boundary.emitted);
    CHECK_FALSE(boundary.suppressed_by_window);
  }

  TEST_CASE("key replacement refreshes timestamp after expiry")
  {
    auto        at_ms = [](int64_t ms) { return TestDeduper::time_point{std::chrono::milliseconds(ms)}; };
    TestDeduper deduper(8);
    const auto  ttl = std::chrono::milliseconds(100);

    CHECK(deduper.should_emit("incoming", at_ms(0), ttl).emitted);
    CHECK(deduper.should_emit("incoming", at_ms(100), ttl).emitted);
    CHECK_FALSE(deduper.should_emit("incoming", at_ms(199), ttl).emitted);
    CHECK(deduper.should_emit("incoming", at_ms(200), ttl).emitted);
    CHECK(deduper.size() == 1);
  }

  TEST_CASE("prune removes expired entries without removing live entries")
  {
    auto        at_ms = [](int64_t ms) { return TestDeduper::time_point{std::chrono::milliseconds(ms)}; };
    TestDeduper deduper(8);
    const auto  ttl = std::chrono::milliseconds(100);

    CHECK(deduper.should_emit("old", at_ms(0), ttl).emitted);
    CHECK(deduper.should_emit("live", at_ms(50), ttl).emitted);

    CHECK(deduper.prune(at_ms(100)) == 1);
    CHECK_FALSE(deduper.contains("old"));
    CHECK(deduper.contains("live"));
  }

  TEST_CASE("max-entry eviction removes the oldest cached key")
  {
    auto        at_ms = [](int64_t ms) { return TestDeduper::time_point{std::chrono::milliseconds(ms)}; };
    TestDeduper deduper(2);
    const auto  ttl = std::chrono::seconds(10);

    CHECK(deduper.should_emit("first", at_ms(0), ttl).emitted);
    CHECK(deduper.should_emit("second", at_ms(1), ttl).emitted);

    const auto result = deduper.should_emit("third", at_ms(2), ttl);
    CHECK(result.emitted);
    CHECK(result.evicted_oldest);
    CHECK(result.cache_size == 2);
    CHECK_FALSE(deduper.contains("first"));
    CHECK(deduper.contains("second"));
    CHECK(deduper.contains("third"));
  }
}

// ===========================================================================
// notification text and queue helpers
// ===========================================================================


TEST_SUITE("notification_text")
{
  TEST_CASE("normalizes line feeds for Windows notification XML")
  {
    CHECK(notification_normalize_body("alpha\nbeta\r\ngamma\rdone") == "alpha\r\nbeta\r\ngamma\rdone");
    CHECK(notification_normalize_body(nullptr).empty());
    CHECK(notification_normalize_body("").empty());
  }

  TEST_CASE("flattens whitespace for queue summaries")
  { CHECK(notification_flatten_text("  alpha\n\tbeta  gamma \r\n") == "alpha beta gamma"); }

  TEST_CASE("escapes control whitespace for logs")
  { CHECK(notification_escape_text_for_log("a\r\nb\tc") == "a\\r\\nb\\tc"); }

  TEST_CASE("strips Unity rich text tags")
  { CHECK(notification_strip_unity_rich_text("<color=#fff>Alpha</color> <b>Beta</b>") == "Alpha Beta"); }

  TEST_CASE("chooses parsed body before localized fallbacks")
  {
    CHECK(notification_choose_body("parsed", "formatted", "raw") == "parsed");
    CHECK(notification_choose_body("", "formatted", "raw") == "formatted");
    CHECK(notification_choose_body("", "", "<b>raw</b>") == "raw");
    CHECK(notification_choose_body("", "", "", "fallback") == "fallback");
  }
}


TEST_SUITE("notification_queue")
{
  TEST_CASE("collapses same-title batches into a counted summary")
  {
    std::vector<NotificationQueueRequest> batch{
        {.source = "toast", .title = "Fleet Battle", .body = "one"},
        {.source = "toast", .title = "Fleet Battle", .body = "two"},
        {.source = "toast", .title = "Fleet Battle", .body = "three"},
    };

    auto collapsed = notification_queue_collapse_batch(std::move(batch), 2);
    CHECK(collapsed.title == "Fleet Battle (3)");
    CHECK(collapsed.body == "one\ntwo\n+1 more");
  }

  TEST_CASE("collapses mixed-title batches with title prefixes")
  {
    std::vector<NotificationQueueRequest> batch{
        {.source = "toast", .title = "Victory!", .body = "alpha"},
        {.source = "direct", .title = "Fleet Docked", .body = "beta"},
    };

    auto collapsed = notification_queue_collapse_batch(std::move(batch), 4);
    CHECK(collapsed.title == "2 Notifications");
    CHECK(collapsed.body == "Victory!: alpha\nFleet Docked: beta");
  }

  TEST_CASE("builds deterministic batch preview with limit")
  {
    std::vector<NotificationQueueRequest> batch{
        {.source = "toast", .title = "Victory!"},
        {.source = "direct", .title = ""},
        {.source = "toast", .title = "Defeat"},
    };

    CHECK(notification_queue_batch_preview(batch, 2) == "toast:Victory!, direct:(untitled), +1 more");
  }
}


TEST_SUITE("incoming_attack_policy")
{
  TEST_CASE("attacker fleet type maps to player hostile or unknown")
  {
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(1) == IncomingAttackPolicyAttackerKind::Player);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(2) == IncomingAttackPolicyAttackerKind::Hostile);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(3) == IncomingAttackPolicyAttackerKind::Hostile);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(4) == IncomingAttackPolicyAttackerKind::Hostile);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(6) == IncomingAttackPolicyAttackerKind::Hostile);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(0) == IncomingAttackPolicyAttackerKind::Unknown);
    CHECK(incoming_attack_policy_attacker_kind_from_fleet_type(5) == IncomingAttackPolicyAttackerKind::Unknown);
  }

  TEST_CASE("titles and fleet copy specialize by attacker kind")
  {
    CHECK(std::string(incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind::Hostile))
          == "Incoming Hostile Attack");
    CHECK(std::string(incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind::Player))
          == "Incoming Player Attack");
    CHECK(std::string(incoming_attack_policy_title_for_kind(IncomingAttackPolicyAttackerKind::Unknown))
          == "Incoming Attack!");

    CHECK(incoming_attack_policy_fleet_body("K'VORT", IncomingAttackPolicyAttackerKind::Hostile)
          == "Your K'VORT is being chased.");
    CHECK(incoming_attack_policy_fleet_body("K'VORT", IncomingAttackPolicyAttackerKind::Player)
          == "Your K'VORT is under attack by another player.");
    CHECK(incoming_attack_policy_fleet_body("", IncomingAttackPolicyAttackerKind::Unknown)
          == "Your fleet is under attack.");
  }

  TEST_CASE("station copy specializes by attacker kind")
  {
    CHECK(incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind::Hostile)
          == "Your station is under attack by a hostile.");
    CHECK(incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind::Player)
          == "Your station is under attack by another player.");
    CHECK(incoming_attack_policy_station_body(IncomingAttackPolicyAttackerKind::Unknown)
          == "Your station is under attack.");
  }

  TEST_CASE("dedupe key captures station fleet and global target identity")
  {
    const auto station =
        incoming_attack_policy_dedupe_key(123, 3, IncomingAttackPolicyAttackerKind::Player, "attacker");
    CHECK(station.target_kind == IncomingAttackPolicyTargetKind::Station);
    CHECK(station.target_id == 0);
    CHECK(station.attacker_identity == "attacker");

    const auto fleet = incoming_attack_policy_dedupe_key(123, 1, IncomingAttackPolicyAttackerKind::Hostile, "");
    CHECK(fleet.target_kind == IncomingAttackPolicyTargetKind::Fleet);
    CHECK(fleet.target_id == 123);

    const auto global = incoming_attack_policy_dedupe_key(0, 0, IncomingAttackPolicyAttackerKind::Unknown, "");
    CHECK(global.target_kind == IncomingAttackPolicyTargetKind::Global);
    CHECK(global.target_id == 0);
  }

  TEST_CASE("target type names remain stable")
  {
    CHECK(std::string(incoming_attack_policy_target_type_name(0)) == "None");
    CHECK(std::string(incoming_attack_policy_target_type_name(1)) == "Fleet");
    CHECK(std::string(incoming_attack_policy_target_type_name(2)) == "DockingPoint");
    CHECK(std::string(incoming_attack_policy_target_type_name(3)) == "Station");
    CHECK(std::string(incoming_attack_policy_target_type_name(99)) == "Unknown");
  }

  TEST_CASE("incoming attack toast states are consumed separately from visible notification policy")
  {
    CHECK(incoming_attack_policy_consumes_toast_state(5));
    CHECK(incoming_attack_policy_consumes_toast_state(6));
    CHECK_FALSE(incoming_attack_policy_consumes_toast_state(17));
    CHECK_FALSE(incoming_attack_policy_consumes_toast_state(10));
  }

  TEST_CASE("generic dedupe suppresses inside TTL and emits at window boundary")
  {
    IncomingAttackPolicyDeduper deduper;
    const auto key = incoming_attack_policy_dedupe_key(123, 1, IncomingAttackPolicyAttackerKind::Hostile, "");

    auto first = deduper.should_emit(key, 100);
    CHECK(first.emitted);
    CHECK_FALSE(first.suppressed_by_window);

    auto duplicate = deduper.should_emit(key, 109);
    CHECK_FALSE(duplicate.emitted);
    CHECK(duplicate.suppressed_by_window);

    auto boundary = deduper.should_emit(key, 110);
    CHECK(boundary.emitted);
    CHECK_FALSE(boundary.suppressed_by_window);
  }

  TEST_CASE("identified dedupe uses longer TTL")
  {
    IncomingAttackPolicyDeduper deduper;
    const auto key = incoming_attack_policy_dedupe_key(123, 1, IncomingAttackPolicyAttackerKind::Player, "player-1");
    CHECK(incoming_attack_policy_dedupe_window_seconds(key) == 120);

    CHECK(deduper.should_emit(key, 100).emitted);
    CHECK_FALSE(deduper.should_emit(key, 219).emitted);
    CHECK(deduper.should_emit(key, 220).emitted);
  }

  TEST_CASE("dedupe evicts oldest entry when max size is exceeded")
  {
    IncomingAttackPolicyDeduper deduper(2);
    const auto first  = incoming_attack_policy_dedupe_key(1, 1, IncomingAttackPolicyAttackerKind::Hostile, "a");
    const auto second = incoming_attack_policy_dedupe_key(2, 1, IncomingAttackPolicyAttackerKind::Hostile, "b");
    const auto third  = incoming_attack_policy_dedupe_key(3, 1, IncomingAttackPolicyAttackerKind::Hostile, "c");

    CHECK(deduper.should_emit(first, 100).emitted);
    CHECK(deduper.should_emit(second, 101).emitted);
    const auto result = deduper.should_emit(third, 102);
    CHECK(result.emitted);
    CHECK(result.evicted_oldest);
    CHECK(result.cache_size == 2);
    CHECK_FALSE(deduper.contains(first));
    CHECK(deduper.contains(second));
    CHECK(deduper.contains(third));
  }
}

// ===========================================================================
// toast_state_title
// ===========================================================================


TEST_SUITE("toast_state_title")
{
  TEST_CASE("known states return correct titles")
  {
    CHECK(std::string(toast_state_title(10)) == "Victory!");
    CHECK(std::string(toast_state_title(11)) == "Defeat");
    CHECK(std::string(toast_state_title(37)) == "Partial Victory");
    CHECK(std::string(toast_state_title(9)) == "Station Victory!");
    CHECK(std::string(toast_state_title(12)) == "Station Defeat");
    CHECK(std::string(toast_state_title(8)) == "Station Under Attack!");
    CHECK(std::string(toast_state_title(5)) == "Incoming Attack!");
    CHECK(std::string(toast_state_title(7)) == "Fleet Battle");
    CHECK(std::string(toast_state_title(18)) == "Armada Victory!");
    CHECK(std::string(toast_state_title(19)) == "Armada Defeated");
    CHECK(std::string(toast_state_title(15)) == "Armada Created");
    CHECK(std::string(toast_state_title(16)) == "Armada Canceled");
    CHECK(std::string(toast_state_title(28)) == "Achievement");
    CHECK(std::string(toast_state_title(29)) == "Assault Victory!");
    CHECK(std::string(toast_state_title(30)) == "Assault Defeat");
    CHECK(std::string(toast_state_title(40)) == "Fleet Preset Applied");
  }

  TEST_CASE("unknown state returns nullptr")
  {
    CHECK(toast_state_title(999) == nullptr);
    CHECK(toast_state_title(-1) == nullptr);
    CHECK(toast_state_title(13) == nullptr); // gap in enum (13 is unused)
  }
}


TEST_SUITE("toast_state_uses_battle_summary")
{
  TEST_CASE("battle-result toasts use battle-summary parsing")
  {
    CHECK(toast_state_uses_battle_summary(10));
    CHECK(toast_state_uses_battle_summary(11));
    CHECK(toast_state_uses_battle_summary(8));
    CHECK(toast_state_uses_battle_summary(18));
    CHECK(toast_state_uses_battle_summary(29));
  }

  TEST_CASE("incoming-attack toasts do not use battle-summary parsing")
  {
    CHECK_FALSE(toast_state_uses_battle_summary(5));
    CHECK_FALSE(toast_state_uses_battle_summary(6));
    CHECK_FALSE(toast_state_uses_battle_summary(17));
    CHECK_FALSE(toast_state_uses_battle_summary(0));
  }
}

// ===========================================================================
// strip_unity_rich_text
// ===========================================================================


TEST_SUITE("fleet_notification_formatting")
{
  TEST_CASE("duration formatting keeps short ETA readable")
  {
    CHECK(format_duration_short(0) == "");
    CHECK(format_duration_short(59) == "59s");
    CHECK(format_duration_short(96) == "1m 36s");
    CHECK(format_duration_short(3600) == "1h");
    CHECK(format_duration_short(3660) == "1h 1m");
  }

  TEST_CASE("cargo formatting clamps and rounds percentage")
  {
    CHECK(format_cargo_fill_text(-1.0f) == "");
    CHECK(format_cargo_fill_text(0.0f) == "Current Cargo: 0%");
    CHECK(format_cargo_fill_text(0.126f) == "Current Cargo: 13%");
    CHECK(format_cargo_fill_text(1.4f) == "Current Cargo: 100%");
  }

  TEST_CASE("started mining title and body use stacked layout")
  {
    CHECK(format_started_mining_title("K'VORT", "Parsteel") == "K'VORT started mining Parsteel");
    CHECK(format_started_mining_body("1m 36s", "Current Cargo: 0%") == "ETA 1m 36s\nCurrent Cargo: 0%");
  }

  TEST_CASE("started mining title and body omit optional details cleanly")
  {
    CHECK(format_started_mining_title("K'VORT", "") == "K'VORT started mining");
    CHECK(format_started_mining_body("", "") == "");
    CHECK(format_started_mining_title("", "Parsteel") == "Fleet started mining Parsteel");
    CHECK(format_started_mining_body("", "Current Cargo: 55%") == "Current Cargo: 55%");
  }

  TEST_CASE("node depleted body keeps resource and cargo context")
  {
    CHECK(format_node_depleted_body("K'VORT", "Parsteel", "Current Cargo: 100%")
          == "K'VORT depleted its Parsteel node. Current Cargo: 100%.");
    CHECK(format_node_depleted_body("?", "", "") == "Fleet depleted its node.");
  }

  TEST_CASE("fleet transition decisions classify arrivals and mining")
  {
    auto arrivedInSystem = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::Warping),
        static_cast<int>(FleetBarTransitionState::Impulsing),
        true,
        false,
        false,
        false,
        false,
        "K'VORT",
    });
    CHECK(arrivedInSystem.kind == FleetBarTransitionNotificationKind::ArrivedInSystem);
    CHECK(arrivedInSystem.title == "Fleet Arrived");
    CHECK(arrivedInSystem.body == "Your K'VORT has arrived in-system");

    auto arrivedAtDestination = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::Impulsing),
        static_cast<int>(FleetBarTransitionState::IdleInSpace),
        false,
        true,
        false,
        false,
        false,
        "K'VORT",
    });
    CHECK(arrivedAtDestination.kind == FleetBarTransitionNotificationKind::ArrivedAtDestination);
    CHECK(arrivedAtDestination.body == "Your K'VORT has arrived at its destination");

    auto startedMining = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::IdleInSpace),
        static_cast<int>(FleetBarTransitionState::Mining),
        false,
        false,
        true,
        false,
        false,
        "K'VORT",
        "Parsteel",
        "1m 36s",
        "Current Cargo: 13%",
    });
    CHECK(startedMining.kind == FleetBarTransitionNotificationKind::StartedMining);
    CHECK(startedMining.title == "K'VORT started mining Parsteel");
    CHECK(startedMining.body == "ETA 1m 36s\nCurrent Cargo: 13%");
    CHECK(startedMining.clear_mining_eta);
  }

  TEST_CASE("fleet transition decisions suppress ambiguous docking")
  {
    auto ambiguousDocked = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::CanManage),
        static_cast<int>(FleetBarTransitionState::Docked),
        false,
        false,
        false,
        true,
        true,
        "K'VORT",
    });
    CHECK(ambiguousDocked.kind == FleetBarTransitionNotificationKind::None);
    CHECK(ambiguousDocked.suppressed_ambiguous_docked);

    auto dockedFromSpace = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::Impulsing),
        static_cast<int>(FleetBarTransitionState::Docked),
        false,
        false,
        false,
        true,
        true,
        "K'VORT",
    });
    CHECK(dockedFromSpace.kind == FleetBarTransitionNotificationKind::Docked);
    CHECK(dockedFromSpace.title == "Fleet Docked");
    CHECK(dockedFromSpace.body == "Your K'VORT docked");

    auto repairComplete = fleet_bar_transition_notification_decision({
        static_cast<int>(FleetBarTransitionState::Repairing),
        static_cast<int>(FleetBarTransitionState::Docked),
        false,
        false,
        false,
        true,
        true,
        "K'VORT",
    });
    CHECK(repairComplete.kind == FleetBarTransitionNotificationKind::RepairComplete);
    CHECK(repairComplete.title == "Repair Complete");
    CHECK(repairComplete.body == "Your K'VORT finished repairs");
  }

  TEST_CASE("fleet runtime trigger sources stay high-signal")
  {
    CHECK(fleet_runtime_trigger_source_for_state_transition(static_cast<int>(FleetBarTransitionState::IdleInSpace),
                    static_cast<int>(FleetBarTransitionState::Mining))
      == std::string_view{"fleet-slot-mining-started"});
    CHECK(fleet_runtime_trigger_source_for_state_transition(static_cast<int>(FleetBarTransitionState::Mining),
                    static_cast<int>(FleetBarTransitionState::WarpCharging))
      == std::string_view{"fleet-slot-mining-stopped"});
    CHECK(fleet_runtime_trigger_source_for_state_transition(static_cast<int>(FleetBarTransitionState::IdleInSpace),
                    static_cast<int>(FleetBarTransitionState::Impulsing))
      == std::string_view{"fleet-slot-impulse-started"});
    CHECK(fleet_runtime_trigger_source_for_state_transition(static_cast<int>(FleetBarTransitionState::Warping),
                    static_cast<int>(FleetBarTransitionState::Impulsing))
      == std::string_view{"fleet-slot-arrived-in-system"});
    CHECK(fleet_runtime_trigger_source_for_state_transition(static_cast<int>(FleetBarTransitionState::Impulsing),
                    static_cast<int>(FleetBarTransitionState::IdleInSpace))
      == std::string_view{"fleet-slot-arrived-at-destination"});
    CHECK(fleet_runtime_trigger_source_for_state_transition(static_cast<int>(FleetBarTransitionState::Battling),
                    static_cast<int>(FleetBarTransitionState::Impulsing))
      == std::string_view{"fleet-slot-combat-ended"});
    CHECK(fleet_runtime_trigger_source_for_state_transition(static_cast<int>(FleetBarTransitionState::Docked),
                    static_cast<int>(FleetBarTransitionState::Docked))
      == nullptr);
  }

  TEST_CASE("fleet transition delivery policy separates OS and audio arrival toggles")
  {
    constexpr auto arrival = FleetBarTransitionNotificationKind::ArrivedInSystem;
    constexpr auto mining  = FleetBarTransitionNotificationKind::StartedMining;

    CHECK_FALSE(fleet_bar_transition_arrived_in_system_event_enabled(false, false, false));
    CHECK(fleet_bar_transition_arrived_in_system_event_enabled(true, false, false));
    CHECK_FALSE(fleet_bar_transition_arrived_in_system_event_enabled(false, false, true));
    CHECK(fleet_bar_transition_arrived_in_system_event_enabled(false, true, true));

    CHECK(fleet_bar_transition_should_notify_os(arrival, true));
    CHECK_FALSE(fleet_bar_transition_should_notify_os(arrival, false));
    CHECK(fleet_bar_transition_should_notify_os(mining, false));
    CHECK_FALSE(fleet_bar_transition_should_notify_os(FleetBarTransitionNotificationKind::None, true));

    CHECK(fleet_bar_transition_should_notify_audio(arrival, true, true));
    CHECK_FALSE(fleet_bar_transition_should_notify_audio(arrival, false, true));
    CHECK_FALSE(fleet_bar_transition_should_notify_audio(arrival, true, false));
    CHECK_FALSE(fleet_bar_transition_should_notify_audio(mining, true, true));
  }
}

// ===========================================================================
// BattleSummaryPreview::format_body
// ===========================================================================
