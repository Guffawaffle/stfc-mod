-- @file xmake.lua
-- @brief Root build configuration for the STFC community patch.
--
-- Declares project-wide settings (C++23, static runtime, dependencies),
-- conditionally includes platform-specific targets (win-proxy-dll on Windows,
-- macos-dylib / macos-loader / macos-launcher on macOS), and pulls in the
-- shared "mods" static library that contains all patch logic.

-- ─── Project Settings ────────────────────────────────────────────────────────

set_project("stfc-community-mod")

set_languages("c++23")

set_runtimes("MT") -- Set the default build to multi-threaded static

-- ─── Third-Party Dependencies ────────────────────────────────────────────────

add_requires("eastl")
add_requires("spdlog")
add_requires("toml++")
add_requires("nlohmann_json")
add_requires("cpr")
add_requireconfs("cpr.libcurl", { configs = { zlib = true } })
add_requires("protobuf 32.1")

-- ─── Platform-Specific Targets & Dependencies ────────────────────────────────

if is_plat("windows") then
    includes("win-proxy-dll")
    add_links('rpcrt4')
    add_links('runtimeobject')
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

-- ─── Local / Vendored Packages ───────────────────────────────────────────────

package("libil2cpp")
on_fetch(function(package, opt)
    return { includedirs = path.join(os.scriptdir(), "third_party/libil2cpp") }
end)
package_end()

add_requires("spud v0.2.0-2")
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
