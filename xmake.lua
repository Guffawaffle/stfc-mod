-- @file xmake.lua
-- @brief Root build configuration for the STFC community patch.
--
-- Declares project-wide settings (C++23, static runtime, dependencies),
-- conditionally includes platform-specific targets (win-proxy-dll on Windows,
-- macos-dylib / macos-loader / macos-launcher on macOS), and pulls in the
-- shared "mods" static library that contains all patch logic.

-- ─── Project Settings ────────────────────────────────────────────────────────

set_project("stfc-community-mod")
includes("scripts/xmake/runtime_identity.lua")

option("bg_image")
    set_showmenu(true)
    set_description("Optional PNG to embed as a custom loading background; omitted builds preserve the game background")
    set_default("")
option_end()

option("use_original_bg")
    set_showmenu(true)
    set_description("Force the original in-game loading background and ignore custom background images")
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

option("stfc_build_class")
    set_default("local")
    set_showmenu(true)
    set_description("Runtime identity class: release, test, development, or local")
option_end()

option("stfc_source_state_id")
    set_default("")
    set_showmenu(true)
    set_description("Exact source identity (for example git:<sha> or dirty-sha256:<fingerprint>)")
option_end()

option("stfc_base_commit")
    set_default("")
    set_showmenu(true)
    set_description("Downstream base commit for runtime provenance")
option_end()

option("stfc_test_target")
    set_default("")
    set_showmenu(true)
    set_description("Target repository and PR/candidate for an unofficial test build")
option_end()

option("stfc_test_expiry")
    set_default("")
    set_showmenu(true)
    set_description("Expiry or supersession condition for an unofficial test build")
option_end()

option("stfc_support_boundary")
    set_default("")
    set_showmenu(true)
    set_description("Support boundary for an unofficial test build")
option_end()

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
