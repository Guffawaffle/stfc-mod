-- STFC Profiles is one immutable source dependency. Development may explicitly
-- embed an existing working checkout; no silent sibling path is searched.
includes("../stfc-profiles.pin.lua")
local pin = stfc_profiles_pin()
local revision = pin.revision
local digest = pin.archive_sha256
if not revision or #revision ~= 40 then
    error("Invalid immutable STFC Profiles source revision")
end
local override = get_config("stfc_profiles_source")
if override and #override > 0 then
    includes(path.join(path.absolute(override), "xmake/library.lua"))
else
    package("stfc-profiles")
        set_homepage("https://github.com/Guffawaffle/stfc-profiles")
        set_description("Shared immutable-ID profile catalog and protected preferences")
        set_license("GPL-3.0")
        add_urls("https://codeload.github.com/Guffawaffle/stfc-profiles/tar.gz/" .. revision,
            {filename = "stfc-profiles-" .. revision .. ".tar.gz"})
        add_versions("0.3.0-" .. revision, digest)
        add_deps("nlohmann_json 3.12.0")
        add_deps("libarchive 3.8.9", {configs = {shared = false, xz = true, zlib = true,
            openssl3 = false, bzip2 = false, lz4 = false, lzo = false, zstd = false}})
        on_load(function(package)
            if #digest ~= 64 or not digest:match("^[0-9a-f]+$") then
                raise("STFC Profiles immutable archive SHA256 is not pinned")
            end
            if package:is_plat("windows") then
                package:add("syslinks", "crypt32", "bcrypt", "shell32", "ole32", "advapi32", "winhttp")
            elseif package:is_plat("macosx") then
                package:add("frameworks", "Security", "CoreFoundation")
                package:add("syslinks", "proc")
            end
        end)
        on_install(function(package)
            -- Build only the published embeddable core. The full product project
            -- would resolve standalone-only SPUD/spdlog in a nested private cache.
            io.writefile("xmake.lua", [[
set_project("stfc-profiles-core")
set_version("0.3.0")
set_languages("c++23")
set_runtimes("MT")
add_rules("mode.debug", "mode.release")
includes("xmake/library.lua")
]])
            import("package.tools.xmake").install(package, {}, {target = "stfc-profiles-core"})
            local source = path.join(package:installdir(), "share/stfc-profiles")
            os.mkdir(source)
            os.cp("adapters", source)
            os.cp("cli", source)
        end)
        on_fetch(function(package)
            return {includedirs = path.join(package:installdir(), "include"),
                    linkdirs = path.join(package:installdir(), "lib"), links = "stfc-profiles-core",
                    syslinks = package:get("syslinks"), frameworks = package:get("frameworks")}
        end)
    package_end()
    add_requires("stfc-profiles 0.3.0-" .. revision, {system = false})
end

rule("stfc.profiles.source")
    on_load(function(target)
        local source = get_config("stfc_profiles_source")
        local actual_revision = pin.revision
        local state = "pinned"
        if source and #source > 0 then
            source = path.absolute(source)
            actual_revision = os.iorunv("git", {"-C", source, "rev-parse", "HEAD"}):trim()
            local status = os.iorunv("git", {"-C", source, "status", "--porcelain", "--untracked-files=all"}):trim()
            if (actual_revision ~= pin.revision or #status > 0) and not get_config("stfc_profiles_allow_dirty") then
                raise("STFC Profiles override differs from its immutable pin. Development requires --stfc_profiles_allow_dirty=y")
            end
            state = "checkout"
            if actual_revision ~= pin.revision or #status > 0 then
                state = "development"
                cprint("${yellow}STFC Profiles development override: %s at %s; changes: %s", source, actual_revision,
                    #status > 0 and "present" or "none")
            end
        else
            source = path.join(target:pkg("stfc-profiles"):installdir(), "share/stfc-profiles")
        end
        target:add("defines", "STFC_PROFILES_SOURCE_REVISION=\"" .. actual_revision .. "\"")
        target:add("defines", "STFC_PROFILES_SOURCE_STATE=\"" .. state .. "\"")
        target:data_set("stfc.profiles.source", source)
        target:add("files", path.join(source, "adapters/community_mod/profile_isolation.cc"))
        target:add("files", path.join(source, "adapters/community_mod/process_admission.cc"))
        target:add("files", path.join(source, "adapters/community_mod/browser_guardian.cc"))
    end)
rule_end()

-- The Mac sign-in guardian and launcher consume the exact same pinned CLI.
if is_plat("macosx") then
    target("stfc-profiles")
        set_kind("binary")
        set_exceptions("cxx")
        if override and #override > 0 then
            add_deps("stfc-profiles-core")
            add_files(path.join(path.absolute(override), "cli/main.cc"))
        else
            add_packages("stfc-profiles")
            on_load(function(target)
                target:add("files", path.join(target:pkg("stfc-profiles"):installdir(), "share/stfc-profiles/cli/main.cc"))
            end)
        end
    target_end()
end
