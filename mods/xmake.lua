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
    -- Regenerate embedded_loading_image.h if --bg_image=<path> was passed to xmake f
    before_build(function(target)
        local img_path = get_config("bg_image")
        if not img_path or img_path == "" then
            return
        end
        if not os.isfile(img_path) then
            raise("bg_image: file not found: " .. img_path)
        end
        local fh = assert(io.open(img_path, "rb"))
        local data = fh:read("*all")
        fh:close()
        local size = #data
        local out_path = path.join(target:scriptdir(), "src/patches/parts/embedded_loading_image.h")
        local out = assert(io.open(out_path, "w"))
        out:write("// Auto-generated embedded loading screen image\n")
        out:write("// Source: " .. img_path .. "\n")
        out:write("// Size: " .. tostring(size) .. " bytes\n")
        out:write("\n#pragma once\n\n")
        out:write("static const unsigned char g_embeddedLoadingImage[] = {\n")
        local col = 0
        for i = 1, size do
            if col == 0 then out:write("  ") end
            out:write(string.format("0x%02x", data:byte(i)))
            if i < size then out:write(",") end
            col = col + 1
            if col == 16 then
                out:write("\n")
                col = 0
            end
        end
        if col ~= 0 then out:write("\n") end
        out:write("};\n")
        out:write("static const size_t g_embeddedLoadingImage_SIZE = " .. tostring(size) .. ";\n")
        out:close()
        cprint("${green}[bg_image] Generated embedded_loading_image.h from '%s' (%d bytes)${clear}", img_path, size)
    end)

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
