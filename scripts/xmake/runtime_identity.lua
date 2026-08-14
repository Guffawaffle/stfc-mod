rule("stfc.runtime-identity")
    on_load(function(target)
        import("core.base.json")
        import("core.base.bytes")

        local function trimmed(value)
            return (value or ""):gsub("^%s+", ""):gsub("%s+$", "")
        end

        local function required_string(container, key, context)
            local value = container and container[key]
            if type(value) ~= "string" or trimmed(value) == "" then
                raise("[runtime-identity] " .. context .. "." .. key .. " must be a non-empty string")
            end
            return value
        end

        local project_root = os.projectdir()
        local manifest_path = path.join(project_root, "manifests/runtime_identity.v1.json")
        local manifest = json.loadfile(manifest_path)
        if type(manifest) ~= "table" or manifest.schemaVersion ~= 1 then
            raise("[runtime-identity] unsupported or missing manifest: " .. manifest_path)
        end

        local distribution_id = required_string(manifest, "distributionId", "identity")
        local display_name = required_string(manifest, "displayName", "identity")
        local unofficial_label = required_string(manifest, "unofficialLabel", "identity")
        local upstream_repository = required_string(manifest.upstream, "repository", "identity.upstream")
        local upstream_version = required_string(manifest.upstream, "version", "identity.upstream")
        local upstream_commit = required_string(manifest.upstream, "commit", "identity.upstream")
        if not upstream_commit:match("^[0-9a-fA-F]+$") or #upstream_commit ~= 40 then
            raise("[runtime-identity] identity.upstream.commit must be a 40-character commit SHA")
        end
        local upstream_base = upstream_repository .. "@" .. upstream_version .. "#" .. upstream_commit:lower()

        local build_class = trimmed(get_config("stfc_build_class"))
        local build_class_config = manifest.buildClasses and manifest.buildClasses[build_class]
        if type(build_class_config) ~= "table" then
            raise("[runtime-identity] stfc_build_class must be release, test, development, or local")
        end
        local build_class_label = required_string(
            build_class_config,
            "label",
            "identity.buildClasses." .. build_class
        )

        local function git_raw_output(arguments)
            if not os.exists(path.join(project_root, ".git")) then
                return ""
            end
            local command_arguments = {"-C", project_root}
            for _, argument in ipairs(arguments) do
                table.insert(command_arguments, argument)
            end
            return os.iorunv("git", command_arguments)
        end

        local function git_output(arguments)
            return trimmed(git_raw_output(arguments))
        end

        local function dirty_source_fingerprint(base_commit)
            local parts = {"base=" .. base_commit, git_raw_output({"diff", "--binary", "--no-ext-diff", "HEAD", "--"})}
            local untracked = git_output({"ls-files", "--others", "--exclude-standard"})
            for filename in untracked:gmatch("[^\r\n]+") do
                local blob_hash = git_output({"hash-object", "--no-filters", "--", filename})
                table.insert(parts, "untracked=" .. filename .. ":" .. blob_hash)
            end
            return hash.sha256(bytes(table.concat(parts, "\n")))
        end

        local git_head = git_output({"rev-parse", "HEAD"})
        local git_status = git_output({"status", "--porcelain=v1", "--untracked-files=all"})

        local base_commit = trimmed(get_config("stfc_base_commit"))
        if base_commit == "" then
            base_commit = git_head
        end
        if base_commit ~= "" and (not base_commit:match("^[0-9a-fA-F]+$") or #base_commit ~= 40) then
            raise("[runtime-identity] stfc_base_commit must be a 40-character commit SHA")
        end
        base_commit = base_commit:lower()

        local source_state_id = trimmed(get_config("stfc_source_state_id"))
        local source_reproducible = false
        if source_state_id == "" then
            if base_commit == "" then
                source_state_id = "unavailable"
            elseif git_status == "" then
                source_state_id = "git:" .. base_commit
                source_reproducible = true
            else
                source_state_id = "dirty-sha256:" .. dirty_source_fingerprint(base_commit)
            end
        else
            local source_commit = source_state_id:match("^git:([0-9a-fA-F]+)$")
            if source_commit then
                if #source_commit ~= 40 then
                    raise("[runtime-identity] git source identity must contain a 40-character commit SHA")
                end
                source_state_id = "git:" .. source_commit:lower()
                source_reproducible = true
            elseif source_state_id:match("^git:") then
                raise("[runtime-identity] git source identity must contain a 40-character hexadecimal commit SHA")
            end
        end

        local release_tag = trimmed(get_config("stfc_release_tag"))
        local test_target = trimmed(get_config("stfc_test_target"))
        local test_expiry = trimmed(get_config("stfc_test_expiry"))
        local support_boundary = trimmed(get_config("stfc_support_boundary"))

        if build_class == "release" then
            if not has_config("stfc_public_release") then
                raise("[runtime-identity] maintained release identity requires --stfc_public_release=y")
            end
            if release_tag == "" then
                raise("[runtime-identity] maintained release identity requires --stfc_release_tag")
            end
            if not source_reproducible or base_commit == "" then
                raise("[runtime-identity] maintained release identity requires a clean git source identity and base commit")
            end
            if git_head == "" or git_status ~= "" then
                raise("[runtime-identity] maintained release identity requires a clean Git checkout")
            end
            if base_commit ~= git_head or source_state_id ~= "git:" .. git_head then
                raise("[runtime-identity] release source identity and base commit must match checked-out HEAD")
            end
        elseif build_class == "test" then
            if test_target == "" or test_expiry == "" or support_boundary == "" then
                raise("[runtime-identity] test builds require target, expiry, and support-boundary values")
            end
        elseif test_target ~= "" or test_expiry ~= "" or support_boundary ~= "" then
            raise("[runtime-identity] test metadata is only valid with --stfc_build_class=test")
        end

        local function string_define(name, value)
            local literal = json.encode(value):gsub("\\/", "/")
            return "#define " .. name .. " " .. literal
        end

        local header_lines = {
            "#pragma once",
            "",
            string_define("STFC_DISTRIBUTION_ID", distribution_id),
            string_define("STFC_MOD_DISPLAY_NAME", display_name),
            string_define("STFC_UNOFFICIAL_LABEL", unofficial_label),
            string_define("STFC_BUILD_CLASS", build_class),
            string_define("STFC_BUILD_CLASS_LABEL", build_class_label),
            string_define("STFC_SOURCE_STATE_ID", source_state_id),
            string_define("STFC_BASE_COMMIT", base_commit ~= "" and base_commit or "not-recorded"),
            string_define("STFC_UPSTREAM_BASE", upstream_base),
            string_define("STFC_TEST_TARGET", test_target),
            string_define("STFC_TEST_EXPIRY", test_expiry),
            string_define("STFC_SUPPORT_BOUNDARY", support_boundary),
            string_define("STFC_SOURCE_REPRODUCIBLE_STR", source_reproducible and "true" or "false"),
            "#define STFC_SOURCE_REPRODUCIBLE " .. (source_reproducible and "1" or "0")
        }
        if release_tag ~= "" then
            table.insert(header_lines, string_define("STFC_RELEASE_TAG", release_tag))
        end
        table.insert(header_lines, "")

        local header_dir = path.join(target:autogendir(), "rules", "runtime_identity")
        local header_path = path.join(header_dir, "runtime_identity.generated.h")
        os.mkdir(header_dir)
        io.writefile(header_path, table.concat(header_lines, "\n"))

        target:add("includedirs", header_dir)
        target:add("defines", "STFC_RUNTIME_IDENTITY_GENERATED=1")
    end)
rule_end()
