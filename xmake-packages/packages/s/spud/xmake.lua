package("spud")
add_deps("cmake")
set_sourcedir(path.join(os.scriptdir(), "spud-src"))

on_install(function(package)
    -- XMake may inject long CMake arguments into the source CMakeLists.txt. Build
    -- from a private copy so local package installs cannot dirty the checkout.
    local source_dir = path.join(package:scriptdir(), "spud-src")
    local build_source_dir = path.join(package:builddir(), "source")
    os.tryrm(build_source_dir)
    os.cp(source_dir .. path.sep(), build_source_dir)

    local oldir = os.cd(build_source_dir)
    local configs = {}
    table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
    table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
    table.insert(configs, "-DSPUD_BUILD_TESTS=OFF")
    table.insert(configs, "-DSPUD_NO_LTO=ON")
    table.insert(configs, "-DSPUD_DETOUR_TRACING=OFF")
    import("package.tools.cmake").install(package, configs)
    os.cd(oldir)
end)
