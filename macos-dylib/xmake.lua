-- @file xmake.lua
-- @brief Build target for the macOS community patch shared library.
--
-- Produces libstfc-community-patch.dylib, which is injected into the game
-- process via DYLD_INSERT_LIBRARIES by the macOS loader/launcher.

target("stfc-community-patch")
do
    set_kind("shared")
    add_files("src/*.cc")
    add_deps("mods")
    set_exceptions("cxx")
    set_policy("build.optimization.lto", true)
end
