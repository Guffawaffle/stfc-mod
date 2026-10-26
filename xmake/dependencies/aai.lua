-- AAI is a private, downstream-only source include. Disabled builds do not
-- inspect its checkout, load its build script, or require private credentials.
if get_config("stfc_aai") then
    includes("../stfc-aai.pin.lua")
    local pin = stfc_aai_pin()
    if not pin.revision or #pin.revision ~= 40 or not pin.revision:match("^[0-9a-f]+$") then
        raise("Invalid immutable AAI revision")
    end
    local override = get_config("stfc_aai_source")
    local source = override and #override > 0 and path.absolute(override)
        or path.join(os.projectdir(), "third_party/stfc-aai")
    if not os.isfile(path.join(source, "include/stfc_aai/overlay.h")) then
        raise("AAI source missing: git submodule update --init --checkout third_party/stfc-aai, or explicitly select --stfc_aai_source")
    end
    includes(path.join(source, "xmake/embed.lua"))
    rule("stfc.aai.identity")
        on_load(function(target)
            local revision = os.iorunv("git", {"-C", source, "rev-parse", "HEAD"}):trim()
            local status = os.iorunv("git", {"-C", source, "status", "--porcelain", "--untracked-files=all"}):trim()
            local development = override and #override > 0
            local differs = revision ~= pin.revision or #status > 0
            if differs and (not development or not get_config("stfc_aai_allow_dirty")) then
                raise("AAI checkout differs from its immutable pin. Development requires explicit --stfc_aai_source and --stfc_aai_allow_dirty=y")
            end
            local state = development and (#status > 0 and "development-dirty" or "development-clean") or "pinned"
            if development then
                cprint("${yellow}AAI explicit development source: %s; revision=%s; state=%s", source, revision, state)
            end
            target:add("defines", "STFC_AAI_ENABLED")
            target:add("defines", "STFC_AAI_SOURCE_REVISION=\"" .. revision .. "\"")
            target:add("defines", "STFC_AAI_SOURCE_STATE=\"" .. state .. "\"")
            -- AAI's two native hook sites use the host's installation policy.
            -- Preinclude only that consumer TU: keep the private pin, generic
            -- analytics and standalone replay source entirely unchanged.
            target:fileconfig_add(path.join(source, "src/battle_log_analysis.cc"), {
                forceincludes = {path.join(os.projectdir(), "mods/src/patches/spud_install.h")}
            })
        end)
    rule_end()
end
