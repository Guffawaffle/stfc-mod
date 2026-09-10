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

target("keyboard-chord-preview-tests")
do
    set_kind("binary")
    set_default(false)
    add_files("keyboard_chord_preview_tests.cc")
    add_includedirs("../mods/src")
    if is_plat("windows") then
        add_syslinks("user32")
    end
end
