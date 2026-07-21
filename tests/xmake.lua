-- Unit tests for pure mod logic (no IL2CPP / game dependencies)
target("stfc-mod-tests")
do
    set_kind("binary")
    set_default(false) -- don't build with `xmake` alone; use `xmake build stfc-mod-tests`
    set_languages("c++23")

    add_files("src/*.cc")
    add_includedirs("../mods/src", { public = true })

    -- Testable source files from the mod (pure logic only)
    add_files("../mods/src/testable_functions.cc")
    add_files("../mods/src/config_redaction.cc")
    add_files("../mods/src/config_release_validation.cc")
    add_files("../mods/src/config_sidecar.cc")
    add_files("../mods/src/config_schema.cc")
    add_files("../mods/src/diagnostics_file_policy.cc")
    add_files("../mods/src/patches/action_queue_repair_config.cc")
    add_files("../mods/src/patches/live_debug_event_store.cc")
    add_files("../mods/src/patches/live_debug_recent_event_requests.cc")
    add_files("../mods/src/patches/live_debug_fleet_serializers.cc")
    add_files("../mods/src/patches/fleet_runtime_diagnostics.cc")
    add_files("../mods/src/patches/live_debug_ui_serializers.cc")
    add_files("../mods/src/patches/live_debug_viewer_serializers.cc")
    add_files("../mods/src/patches/battle_log_decoder.cc")
    add_files("../mods/src/patches/fleet_deferred_action.cc")
    add_files("../mods/src/patches/fleet_input_policy.cc")
    add_files("../mods/src/patches/hook_install_audit.cc")
    add_files("../mods/src/patches/hook_registry.cc")
    add_files("../mods/src/patches/il2cpp_safety.cc")
    add_files("../mods/src/patches/input_binding/input_binding.cc")
    add_files("../mods/src/patches/input_binding/action_registry.cc")
    add_files("../mods/src/patches/input_binding/input_config_bridge.cc")
    add_files("../mods/src/patches/input_binding/input_dispatcher.cc")
    add_files("../mods/src/patches/input_binding/input_runtime_bindings.cc")
    add_files("../mods/src/patches/mod_impact_monitor.cc")
    add_files("../mods/src/patches/notification_queue.cc")
    add_files("../mods/src/patches/notification_policy.cc")
    add_files("../mods/src/patches/notification_text.cc")
    add_files("../mods/src/patches/patch_install_policy.cc")
    add_files("../mods/src/patches/sidecar_local_chunking.cc")
    add_files("../mods/src/patches/sidecar_local_ingest_policy.cc")
    add_files("../mods/src/patches/sync_transport_policy.cc")

    add_packages("doctest", "nlohmann_json", "toml++", "spdlog", "spud")

    add_defines("NOMINMAX")
    add_defines("STFC_MOD_TESTS")

    if is_plat("windows") then
        add_cxflags("/bigobj")
    end
end

target("battle-log-decode")
do
    set_kind("binary")
    set_default(false)
    set_languages("c++23")

    add_files("tools/battle_log_decode_tool.cc")
    add_files("../mods/src/patches/battle_log_decoder.cc")
    add_includedirs("../mods/src", { public = true })
    add_packages("nlohmann_json")
    add_defines("NOMINMAX")

    if is_plat("windows") then
        add_cxflags("/bigobj")
    end
end
