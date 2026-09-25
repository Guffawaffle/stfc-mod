target("audio-coalescing-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("audio_coalescing_test.cc")
    add_includedirs("../mods/src")
    set_exceptions("cxx")
end

target("audio-dispatch-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("audio_dispatch_test.cc", "../mods/src/patches/notification_audio.cc")
    add_includedirs("../mods/src")
    add_packages("spdlog")
    add_defines("NOTIFICATION_AUDIO_TEST", "NOMINMAX")
    set_exceptions("cxx")
end

target("settings-persistence-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("hud_settings_persistence_test.cc", "../mods/src/runtime_config_writer.cc",
              "../mods/src/toml_editor.cc", "../mods/src/config_save.cc")
    add_includedirs("../mods/src")
    add_packages("toml++")
    add_defines("NOMINMAX")
    set_exceptions("cxx")
end

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
