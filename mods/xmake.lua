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
    add_rules("stfc.runtime-identity")

    local public_release = has_config("stfc_public_release")
    local embedded_loading = get_config("bg_image")
    local force_original_bg = get_config("use_original_bg")
    if not force_original_bg and embedded_loading and embedded_loading ~= "" then
        add_defines("STFC_HAS_EMBEDDED_LOADING_IMAGE=1")
    else
        add_defines("STFC_HAS_EMBEDDED_LOADING_IMAGE=0")
    end

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
            local temporary_output = output_file .. ".tmp"

            -- Use binary mode so generated headers use deterministic LF endings
            -- on Windows as required by the repository's .gitattributes.
            local out = io.open(temporary_output, "wb")
            if not out then
                print("[error] cannot write: " .. temporary_output)
                return
            end

            out:write("#pragma once\n\n")
            out:write(string.format("static const unsigned char %s[] = {\n", symbol))

            for i = 1, size do
                out:write(string.format("0x%02X", data:byte(i)))
                if i < size then
                    out:write(",")
                    if i % 16 == 0 then
                        out:write("\n")
                    else
                        out:write(" ")
                    end
                end
            end

            out:write("\n};\n\n")
            out:write(string.format("static const size_t %s_SIZE = %d;\n", symbol, size))

            out:close()

            -- Older generated headers may contain CRLF endings or spaces before
            -- newlines. Preserve an otherwise equivalent tracked file so an
            -- ordinary build does not create a formatting-only diff.
            local function normalized_generated_text(file)
                if not os.isfile(file) then
                    return nil
                end

                local handle = io.open(file, "rb")
                if not handle then
                    return nil
                end

                local text = handle:read("*all")
                handle:close()
                return text:gsub("\r\n", "\n"):gsub("[ \t]+\n", "\n")
            end

            local unchanged = normalized_generated_text(output_file) == normalized_generated_text(temporary_output)
            if unchanged then
                os.rm(temporary_output)
            else
                os.mv(temporary_output, output_file)
            end

            cprint("${green}[embed] %s -> %s (%d bytes%s)${clear}",
                input_file,
                path.filename(output_file),
                size,
                unchanged and ", unchanged" or ""
            )
        end

        local outdir  = path.join(target:scriptdir(), "src/patches/parts")

        local loading = get_config("bg_image")
        if not get_config("use_original_bg") and loading and loading ~= "" then
            embed_image(
                loading,
                path.join(outdir, "embedded_loading_image.h"),
                "g_embeddedLoadingImage"
            )
        end
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

    if get_config("use_original_bg") then
        add_defines("_USE_ORIGINAL_BG")
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
