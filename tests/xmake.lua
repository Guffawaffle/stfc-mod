add_requires("doctest")

target("stfc-mod-tests")
do
    set_kind("binary")
    set_default(false)

    add_files("src/**.cc")
    add_files("../mods/src/patches/key_parse.cc")
    add_includedirs("../mods/src")
    add_packages("doctest")

    set_exceptions("cxx")
    add_defines("NOMINMAX")
end
