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
    -- Regenerate embedded image headers before each build
    before_build(function(target)
        local function embed_image(input_file, output_file, symbol)
            if not os.isfile(input_file) then
                raise("[error] missing file: " .. input_file)
                return
            end

            local fh = io.open(input_file, "rb")
            if not fh then
                raise("[error] cannot open: " .. input_file)
                return
            end

            local data = fh:read("*all")
            fh:close()

            local size = #data

            os.mkdir(path.directory(output_file))

            local out = io.open(output_file, "w")
            if not out then
                print("[error] cannot write: " .. output_file)
                return
            end

            out:write("#pragma once\n\n")
            out:write(string.format("static const unsigned char %s[] = {\n", symbol))

            for i = 1, size do
                out:write(string.format("0x%02X", data:byte(i)))
                if i < size then out:write(", ") end
                if i % 16 == 0 then out:write("\n") end
            end

            out:write("\n};\n\n")
            out:write(string.format("static const size_t %s_SIZE = %d;\n", symbol, size))

            out:close()

            cprint("${green}[embed] %s -> %s (%d bytes)${clear}",
                input_file,
                path.filename(output_file),
                size
            )
        end

        local assets = path.join(target:scriptdir(), "../assets")
        local outdir  = path.join(target:scriptdir(), "src/patches/parts")

        local loading = get_config("bg_image")
        if not loading or loading == "" then
            loading = path.join(assets, "loadingscreen.png")
        end

        embed_image(
            loading,
            path.join(outdir, "embedded_loading_image.h"),
            "g_embeddedLoadingImage"
        )

        embed_image(
            path.join(assets, "stfc-mod-logo.png"),
            path.join(outdir, "embedded_logo_image.h"),
            "g_embeddedLogoImage"
        )

        embed_image(
            path.join(assets, "official-cc-logo.png"),
            path.join(outdir, "embedded_cc_logo_image.h"),
            "g_embeddedCCLogoImage"
        )
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
