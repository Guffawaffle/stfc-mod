-- @file xmake.lua
-- @brief Build target for the "mods" static library.
--
-- Compiles all patch logic (C++ sources, protobuf definitions) into a static
-- library consumed by both the Windows proxy DLL and the macOS dylib targets.
-- Platform-specific flags handle bigobj on MSVC and Objective-C++ on macOS.

target("mods")
do
    add_ldflags("-v")
    set_kind("static")

    local public_release = has_config("stfc_public_release")

    -- C++ sources
    add_files("src/**.cc")
    add_headerfiles("src/**.h")
    add_includedirs("src", { public = true })

    if public_release then
        remove_files("src/patches/parts/action_queue_repair.cc")
        remove_files("src/patches/refinery_diagnostics.cc")
        remove_files("src/patches/object_tracker_public_stubs.cc")
        remove_files("src/patches/parts/testing_config_override.cc")
        remove_files("src/patches/parts/live_debug.cc")
        remove_files("src/patches/parts/live_debug_connector.cc")
        remove_files("src/patches/live_debug_event_dispatcher.cc")
        remove_files("src/patches/live_debug_event_store.cc")
        remove_files("src/patches/live_debug_fleet_change_events.cc")
        remove_files("src/patches/live_debug_fleet_runtime_serializers.cc")
        remove_files("src/patches/live_debug_navhook_trace_sink.cc")
        remove_files("src/patches/live_debug_observation_compare.cc")
        remove_files("src/patches/live_debug_recent_event_requests.cc")
        remove_files("src/patches/live_debug_state_results.cc")
        remove_files("src/patches/live_debug_ui_change_events.cc")
        remove_files("src/patches/live_debug_ui_runtime_observers.cc")
        remove_files("src/patches/live_debug_ui_serializers.cc")
        remove_files("src/patches/live_debug_viewer_runtime.cc")
        remove_files("src/patches/live_debug_viewer_serializers.cc")
        add_defines("STFC_PUBLIC_RELEASE=1")
        add_defines("STFC_ENABLE_DEV_SCIENCE_TOOLS=0")
    else
        add_defines("STFC_PUBLIC_RELEASE=0")
        add_defines("STFC_ENABLE_DEV_SCIENCE_TOOLS=1")
    end

    -- Packages
    add_packages("spud", "nlohmann_json", "protobuf", "libil2cpp", "eastl", "toml++", "spdlog", "simdutf", "libcurl", "capstone", "cpr")
    add_rules("protobuf.cpp")
    add_files("src/prime/proto/*.proto")

    set_exceptions("cxx")
    add_defines("NOMINMAX")
    
    if is_mode("releasedbg") then
        add_defines("_MODDBG")  -- enable your debug flag
    end

    -- Platform-specific settings
    if is_plat("windows") then
        add_cxflags("/bigobj")
        add_linkdirs("src/il2cpp")
    elseif is_plat("macosx") then
        add_cxflags("-fms-extensions")
        -- Add Objective-C++ source
        add_files("src/*.mm")
        -- Link Cocoa framework
        add_frameworks("Cocoa")
    end

    set_policy("build.optimization.lto", false)
end
