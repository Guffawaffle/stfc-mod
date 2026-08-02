-- @file xmake.lua
-- @brief Root build configuration for the STFC community patch.
--
-- Declares project-wide settings (C++23, static runtime, dependencies),
-- conditionally includes platform-specific targets (win-proxy-dll on Windows,
-- macos-dylib / macos-loader / macos-launcher on macOS), and pulls in the
-- shared "mods" static library that contains all patch logic.

-- ─── Project Settings ────────────────────────────────────────────────────────

set_project("stfc-community-mod")

option("bg_image")
    set_showmenu(true)
    set_description("Path to a PNG to embed as the loading screen background (regenerates embedded_loading_image.h)")
    set_default("")
option_end()

option("use_original_bg")
    set_showmenu(true)
    set_description("Keep the original in-game loading screen background (logos remain enabled)")
    set_default(false)
option_end()

set_languages("c++23")

set_runtimes("MT") -- Set the default build to multi-threaded static

-- ─── Third-Party Dependencies ────────────────────────────────────────────────

add_requires("eastl")
add_requires("spdlog")
add_requires("toml++")
add_requires("nlohmann_json")
add_requires("cpr")
add_requireconfs("cpr.libcurl", { configs = { zlib = true } })
add_requires("protobuf 35.1")

-- ─── Platform-Specific Targets & Dependencies ────────────────────────────────

if is_plat("windows") then
    includes("win-proxy-dll")
    add_links('rpcrt4')
    add_links('runtimeobject')
    add_links('winmm')
end

if is_plat("macosx") then
    add_requires("inifile-cpp")
    add_requires("librsync")
    add_requires("PLzmaSDK")
    includes("macos-dylib")
    includes("macos-loader")
    includes("macos-launcher")
end

-- ─── Build Modes ─────────────────────────────────────────────────────────────

add_rules("mode.debug")
add_rules("mode.release")
add_rules("mode.releasedbg")

option("stfc_public_release")
    set_default(false)
    set_showmenu(true)
    set_description("Build the public release profile without dev-only science hooks")
option_end()

option("stfc_release_tag")
    set_default("")
    set_showmenu(true)
    set_description("Optional release tag to expose in runtime diagnostics, sync payloads, and product metadata")
option_end()

option("stfc_distribution_id")
    set_default(os.getenv("STFC_DISTRIBUTION_ID") or "guffawaffle.stfc-community-mod")
    set_showmenu(true)
    set_description("Distribution lineage embedded in Windows DLL provenance")
option_end()

option("stfc_source_state_id")
    set_default(os.getenv("STFC_SOURCE_STATE_ID") or "unknown")
    set_showmenu(true)
    set_description("Clean commit or dirty source fingerprint embedded in Windows DLL provenance")
option_end()

option("stfc_base_commit")
    set_default(os.getenv("STFC_BASE_COMMIT") or "unknown")
    set_showmenu(true)
    set_description("Base source commit embedded in Windows DLL provenance")
option_end()

option("stfc_build_invocation_id")
    set_default(os.getenv("STFC_BUILD_INVOCATION_ID") or "xmake-direct")
    set_showmenu(true)
    set_description("Build or AX cycle correlation identifier embedded in Windows DLL provenance")
option_end()

option("stfc_build_channel")
    set_default(os.getenv("STFC_BUILD_CHANNEL") or "local")
    set_showmenu(true)
    set_description("Build channel embedded in Windows DLL provenance")
option_end()

local function c_string_define(value)
    return value:gsub("\\", "\\\\"):gsub("\"", "\\\"")
end

local function identity_define(option_name, fallback)
    local value = get_config(option_name)
    if not value or value == "" then
        value = fallback
    end
    if #value > 160 then
        raise(option_name .. " must not exceed 160 characters")
    end
    if not value:match("^[%w%._:+/%-]+$") then
        raise(option_name .. " contains unsupported identity characters")
    end
    return c_string_define(value)
end

local stfc_release_tag = get_config("stfc_release_tag")
if stfc_release_tag and stfc_release_tag ~= "" then
    add_defines("STFC_RELEASE_TAG=\"" .. c_string_define(stfc_release_tag) .. "\"")
end

add_defines("STFC_DISTRIBUTION_ID=\"" ..
            identity_define("stfc_distribution_id", "guffawaffle.stfc-community-mod") .. "\"")
add_defines("STFC_SOURCE_STATE_ID=\"" .. identity_define("stfc_source_state_id", "unknown") .. "\"")
add_defines("STFC_BASE_COMMIT=\"" .. identity_define("stfc_base_commit", "unknown") .. "\"")
add_defines("STFC_BUILD_INVOCATION_ID=\"" .. identity_define("stfc_build_invocation_id", "xmake-direct") .. "\"")
add_defines("STFC_BUILD_CHANNEL=\"" .. identity_define("stfc_build_channel", "local") .. "\"")
add_defines("STFC_BUILD_MODE=\"" .. identity_define("mode", "unknown") .. "\"")

-- ─── Local / Vendored Packages ───────────────────────────────────────────────

package("libil2cpp")
on_fetch(function(package, opt)
    return { includedirs = path.join(os.scriptdir(), "third_party/libil2cpp") }
end)
package_end()

add_requires("spud v0.2.0-3")
add_requires("libil2cpp")
add_requires("simdutf", { system = false })
add_requires("doctest")

-- ─── Sub-Targets ─────────────────────────────────────────────────────────────

-- includes("launcher")
includes("mods")
includes("tests")

-- ─── Package Repositories ────────────────────────────────────────────────────

-- add_repositories("local-repo build")
add_repositories("stfc-community-mod-repo xmake-packages")
