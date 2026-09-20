target("keyboard-layout-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("keyboard_layout_tests.cc")
    add_includedirs("../mods/src")
    if is_plat("windows") then
        add_syslinks("user32")
    end
end

target("miner-opc-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("miner_opc_tests.cc")
    add_includedirs("../mods/src")
end

target("ship-name-matching-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("ship_name_matching_tests.cc")
    add_includedirs("../mods/src")
    add_packages("libil2cpp", "eastl", "spdlog", "simdutf")
    if is_plat("windows") then
        add_linkdirs("../mods/src/il2cpp")
    end
end

target("shortcut-layout-dispatch-tests")
do
    set_kind("binary")
    set_default(false)
    add_deps("mods")
    add_files("shortcut_hint_cache.cc")
    add_packages("libil2cpp", "eastl", "toml++", "spdlog")
end

target("keyboard-chord-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("keyboard_chord_tests.cc")
    add_includedirs("../mods/src")
    if is_plat("windows") then
        add_syslinks("user32")
    end
end

if is_plat("macosx") then
    target("macos-hook-extent-tests")
    do
        set_kind("binary")
        set_default(false)
        add_files("macos_hook_extent_tests.cc", "../mods/src/patches/native_hook_extent.cc")
        add_includedirs("../mods/src")
        add_packages("spud", "spdlog")
        set_policy("build.optimization.lto", false)
    end
end

if is_plat("windows", "macosx") then
    target("notification-audio-file-tests")
    do
        set_kind("binary")
        set_default(false)
        add_files("notification_audio_files_test.cc", "../mods/src/patches/notification_audio.cc",
                  "../mods/src/patches/notification_audio_files.cc")
        add_includedirs("../mods/src")
        add_packages("spdlog")
        add_defines("NOMINMAX")
        set_exceptions("cxx")
        if is_plat("windows") then
            add_files("../mods/src/notification_audio_windows.cc")
            add_syslinks("winmm", "mfuuid", "shlwapi", "ole32")
        else
            add_files("../mods/src/notification_audio_mac.mm")
            add_frameworks("Cocoa")
        end
    end
end
