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
    add_files("../mods/src/patches/live_debug_event_store.cc")
    add_files("../mods/src/patches/live_debug_recent_event_requests.cc")
    add_files("../mods/src/patches/live_debug_fleet_serializers.cc")
    add_files("../mods/src/patches/live_debug_ui_serializers.cc")
    add_files("../mods/src/patches/live_debug_viewer_serializers.cc")
    add_files("../mods/src/patches/battle_log_decoder.cc")
    add_files("../mods/src/patches/notification_queue.cc")
    add_files("../mods/src/patches/notification_text.cc")

    add_packages("doctest", "nlohmann_json")

    add_defines("NOMINMAX")

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
