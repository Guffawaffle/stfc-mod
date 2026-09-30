target("mods")
do
    add_rules("stfc.identity")
    add_ldflags("-v")
    set_kind("static")

    -- Regenerate embedded image headers when their source image changes.
    before_build(function(target)
        local function embed_image(input_file, output_file, symbol, custom_background)
            if not os.isfile(input_file) then
                raise("[error] missing file: " .. input_file)
                return
            end

            local previous_custom_background = false
            if os.isfile(output_file) then
                local previous = io.open(output_file, "r")
                if previous then
                    previous_custom_background = previous:read("*line") == "// custom loading background"
                    previous:close()
                end
            end

            if os.isfile(output_file) and not custom_background and not previous_custom_background
                and os.mtime(output_file) >= os.mtime(input_file) then
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

            if custom_background then out:write("// custom loading background\n") end
            out:write("#pragma once\n\n")
            out:write(string.format("static const unsigned char %s[] = {\n", symbol))

            for i = 1, size do
                out:write(string.format("0x%02X", data:byte(i)))
                if i < size then out:write(",") end
                if i % 16 == 0 then
                    out:write("\n")
                elseif i < size then
                    out:write(" ")
                end
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

        local embedded_bg_override = get_config("bg_image")
        local loading = embedded_bg_override
        if not loading or loading == "" then
            loading = path.join(assets, "loadingscreen.png")
        end

        embed_image(
            loading,
            path.join(outdir, "embedded_loading_image.h"),
            "g_embeddedLoadingImage",
            embedded_bg_override and embedded_bg_override ~= ""
        )

        embed_image(
            path.join(assets, "stfc-mod-logo.png"),
            path.join(outdir, "embedded_logo_image.h"),
            "g_embeddedLogoImage"
        )

        embed_image(
            path.join(assets, "guffawaffle-build-badge.png"),
            path.join(outdir, "embedded_cc_logo_image.h"),
            "g_embeddedCCLogoImage"
        )
    end)

    -- C++ sources
    -- Override only the cxx file rule on Windows CI. This leaves protobuf's
    -- separate .proto rule and generated-object build path untouched.
    if is_plat("windows") and os.getenv("STFC_MSVC_SCCACHE") == "1" then
        add_rules("stfc.cxx.sccache", {override = true})
    end
    add_files("src/**.cc")
    add_rules("stfc.profiles.source")
    if get_config("stfc_profiles_source") and #get_config("stfc_profiles_source") > 0 then
        add_deps("stfc-profiles-core")
    else
        add_packages("stfc-profiles")
    end
    add_headerfiles("src/**.h")
    add_includedirs("src", { public = true })

    -- Packages
    add_packages("spud", "nlohmann_json", "protobuf", "libil2cpp", "eastl", "toml++", "spdlog", "simdutf", "libcurl", "capstone", "cpr")
    if os.getenv("STFC_PROTOBUF_SCCACHE") == "1" then
        add_rules("stfc.protobuf.cpp.sccache")
    else
        add_rules("protobuf.cpp")
    end
    add_files("src/prime/proto/*.proto")

    set_exceptions("cxx")
    add_defines("NOMINMAX")

    if is_mode("releasedbg") then
        add_defines("_MODDBG")  -- enable your debug flag
    end

    local embedded_bg_override = get_config("bg_image")
    if get_config("use_original_bg") and (not embedded_bg_override or embedded_bg_override == "") then
        add_defines("_USE_ORIGINAL_BG")
    end

    -- Platform-specific settings
    if is_plat("windows") then
        add_cxflags("/bigobj")
        add_linkdirs("src/il2cpp")
        add_syslinks("winmm", "mfuuid", "shlwapi", "ole32")
    elseif is_plat("macosx") then
        add_cxflags("-fms-extensions")
        -- Add Objective-C++ source
        add_files("src/*.mm")
        -- Link Cocoa framework
        add_frameworks("Cocoa", "UserNotifications", {public = true})
    end

    set_policy("build.optimization.lto", true)
end
