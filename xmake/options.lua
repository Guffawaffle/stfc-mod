option("bg_image")
    set_showmenu(true)
    set_description("Path to a PNG to embed as the loading screen background (regenerates embedded_loading_image.h)")
    set_default("")
option_end()

option("use_original_bg")
    set_showmenu(true)
    set_description("Use the game's original loading background by default; custom loader_image or bg_image still overrides it")
    set_default(true)
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
    set_description("Build correlation identifier embedded in Windows DLL provenance")
option_end()

option("stfc_build_channel")
    set_default(os.getenv("STFC_BUILD_CHANNEL") or "local")
    set_showmenu(true)
    set_description("Build channel embedded in Windows DLL provenance")
option_end()

option("stfc_profiles_source")
    set_showmenu(true)
    set_default("")
    set_description("Explicit STFC Profiles source checkout for development; empty uses the immutable source archive")
option_end()

option("stfc_profiles_allow_dirty")
    set_showmenu(true)
    set_default(false)
    set_description("Explicitly permit a recorded development STFC Profiles checkout that differs from the immutable pin")
option_end()

option("stfc_aai")
    -- play-dev includes AAI by default. Its live TOML toggle controls use;
    -- --stfc_aai=n still builds without inspecting private AAI sources.
    set_default(true)
    set_showmenu(true)
    set_description("Include private After-Action Intelligence source (requires authenticated submodule checkout)")
option_end()

option("stfc_aai_source")
    set_default("")
    set_showmenu(true)
    set_description("Explicit AAI development checkout; empty uses the pinned submodule")
option_end()

option("stfc_aai_allow_dirty")
    set_default(false)
    set_showmenu(true)
    set_description("Allow a recorded AAI development revision or dirty tree differing from the pin")
option_end()

-- AAI's optional source include consumes this flag. Declare it here so XMake
-- recognizes it before resolving a private/development source checkout.
option("stfc_aai_ui_lab")
    set_default(false)
    set_showmenu(true)
    set_description("Include development-only native AAI visual studies")
option_end()
