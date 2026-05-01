-- @file xmake.lua
-- @brief Build target for the macOS CLI loader binary.
--
-- Produces the stfc-community-mod-loader executable, which reads the game
-- install path from launcher_settings.ini, sets up DYLD environment variables,
-- and exec's the game with the community patch dylib injected.

target("stfc-community-mod-loader")
do
    set_kind("binary")
    add_files("src/*.cc")
    add_files("src/*.mm")
    add_headerfiles("src/*.h")
    add_frameworks("Foundation")
    add_packages("inifile-cpp")
end
