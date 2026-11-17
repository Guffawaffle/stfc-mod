//! The repo-owned pure TOML preparation header and optional standalone producers.
//! Only the header enters the mod; optional C ABI products remain separate.
const std = @import("std");
const Ctx = @import("ctx.zig").Ctx;
const Lib = @import("ctx.zig").Lib;
const compat = @import("compat.zig");

pub const pinned_revision = "45d1deaffddc3706e5c8232cf746ca46ef67be91";

pub const Options = struct {
    enabled: bool,
    source: ?[]const u8,
    allow_dirty: bool,

    pub fn parse(b: *std.Build) Options {
        return .{
            .enabled = b.option(bool, "toml", "Expose standalone shared TOML build products; no product activation") orelse false,
            .source = b.option([]const u8, "toml-source", "Explicit absolute owning mod checkout containing shared/toml"),
            .allow_dirty = b.option(bool, "toml-allow-dirty", "Permit and report an explicit development source differing from its pin") orelse false,
        };
    }
};

/// Only include directories are consumed. Standalone producers keep the current
/// component's header-only parser strategy instead of exporting the foundation's
/// separately compiled TOML parser symbols from their C ABI boundary.
pub const Dependencies = struct { toml: Lib, json: Lib };
pub const Outputs = struct { core: Lib, native: Lib };
const Source = struct { root: std.Build.LazyPath, revision: []const u8, state: []const u8 };

/// Exact downstream inline preparation engine. Its host owns I/O and conflicts;
/// reuse the single compiled toml++ provider, with no native C ABI or exports.
pub fn editor(ctx: *const Ctx, toml: Lib) Lib {
    return .{
        .include_dirs = ctx.dirs(&.{ctx.b.path("third_party/stfc_toml/include")}),
        .dependencies = ctx.libs(&.{toml}),
    };
}

pub fn add(ctx: *const Ctx, options: Options, deps: Dependencies) ?Outputs {
    const b = ctx.b;
    const inputs_step = b.step("shared-toml-inputs", "Validate explicitly selected shared TOML source without compiling it");
    const static_step = b.step("shared-toml-static", "Build the standalone shared TOML C ABI archive; no mod linkage");
    const native_step = b.step("shared-toml-native", "Build the standalone three-export TOML DLL/dylib");
    const build_tests = b.step("build-shared-toml-tests", "Compile static and dynamic ABI fixtures without running them");
    const test_step = b.step("test-shared-toml", "Run the original full editor fixture and isolated shared ABI client");
    if (!options.enabled) {
        const disabled = b.addFail("Shared TOML not selected; use -Dtoml=true -Dtoml-source=<absolute-owning-mod-checkout>");
        inline for (.{ inputs_step, static_step, native_step, build_tests, test_step }) |step| step.dependOn(&disabled.step);
        return null; // No source path probe, Git invocation, dependency use or file read.
    }
    const source = verifySource(b, options);
    if (ctx.isWindows()) {
        if (ctx.target.result.cpu.arch != .x86_64)
            std.debug.panic("Shared TOML Windows producer currently targets x86_64-windows-msvc", .{});
    } else if (ctx.isMacos()) {
        if (ctx.target.result.cpu.arch != .aarch64 and ctx.target.result.cpu.arch != .x86_64)
            std.debug.panic("Shared TOML macOS producer supports arm64 and x86_64", .{});
    } else std.debug.panic("M1b standalone native TOML products are scoped to Windows/macOS", .{});

    var core: Lib = .{
        .include_dirs = ctx.dirs(&.{source.root.path(b, "shared/toml/include")}),
        // The pinned header has BUILD vs dllimport, no STATIC spelling. This is
        // how the current direct-link fixture avoids dllimport. DO NOT attach
        // this archive to a product: Windows consumers inherit three exports.
        // A reviewed STFC_TOML_STATIC source contract is needed before that use.
        .defines = &.{.{ "STFC_TOML_BUILD", "1" }},
    };
    const static_module = module(ctx, source, deps);
    static_module.addCSourceFile(.{
        .file = source.root.path(b, "shared/toml/src/native.cc"),
        .flags = producerFlags(ctx),
    });
    core.artifact = ctx.staticLib("stfc-toml-core", static_module);
    static_step.dependOn(&b.addInstallArtifact(core.artifact.?, .{}).step);

    const native_module = module(ctx, source, deps);
    native_module.addCSourceFile(.{
        .file = source.root.path(b, "shared/toml/src/native.cc"),
        .flags = producerFlags(ctx),
    });
    const native_artifact = b.addLibrary(.{ .name = "stfc-toml-native", .linkage = .dynamic, .root_module = native_module });
    ctx.applyCompile(native_artifact);
    native_artifact.rdynamic = false;
    if (ctx.isWindows()) {
        const definitions = b.addWriteFiles();
        const definition = definitions.add("stfc-toml-native.def", "LIBRARY stfc-toml-native\nEXPORTS\n stfc_toml_abi_version\n stfc_toml_execute\n stfc_toml_free\n");
        native_artifact.win32_module_definition = definition;
        native_artifact.step.dependOn(&definitions.step);
    } else {
        native_artifact.install_name = "@rpath/libstfc-toml-native.dylib";
    }
    const native: Lib = .{ .artifact = native_artifact, .include_dirs = core.include_dirs };
    native_step.dependOn(&b.addInstallArtifact(native_artifact, .{}).step);
    // These explicitly selected producer steps preserve licensing without adding
    // either product or its notices to the normal mod install/release closure.
    inline for (.{ static_step, native_step }) |step| {
        step.dependOn(&b.addInstallFileWithDir(source.root.path(b, "LICENSE"), .prefix, "share/stfc-toml-native/LICENSE").step);
        step.dependOn(&b.addInstallFileWithDir(source.root.path(b, "shared/toml/THIRD_PARTY_NOTICES.txt"), .prefix, "share/stfc-toml-native/THIRD_PARTY_NOTICES.txt").step);
    }

    const test_ctx = ctx.withOptimize(ctx.options.test_optimize orelse ctx.optimize);
    const fixture_module = module(test_ctx, source, deps);
    fixture_module.addCSourceFile(.{
        .file = source.root.path(b, "shared/toml/tests/editor_test.cc"),
        .flags = test_ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
    });
    core.linkInto(fixture_module);
    const fixture = test_ctx.executable("shared-toml-editor-tests", fixture_module);
    build_tests.dependOn(&b.addInstallArtifact(fixture, .{}).step);
    test_step.dependOn(&b.addRunArtifact(fixture).step);

    // This client does not link core/native/toml++, so successful calls must
    // cross the actual shared-library allocation/free boundary.
    const client_module = test_ctx.createModule(true);
    client_module.addIncludePath(source.root.path(b, "shared/toml/include"));
    for (deps.json.include_dirs) |dir| client_module.addIncludePath(dir);
    client_module.addCSourceFile(.{ .file = b.path("tests/shared_toml_abi.cc"), .flags = test_ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
    const client = test_ctx.executable("shared-toml-shared-abi-tests", client_module);
    build_tests.dependOn(&b.addInstallArtifact(client, .{}).step);
    build_tests.dependOn(&native_artifact.step);
    const run_client = b.addRunArtifact(client);
    run_client.addArtifactArg(native_artifact);
    test_step.dependOn(&run_client.step);

    // No b.installArtifact, getInstallStep dependency, Feature descriptor,
    // mod.add dependency or Runtime call: merely defining these cannot activate it.
    return .{ .core = core, .native = native };
}

fn verifySource(b: *std.Build, options: Options) Source {
    const requested = options.source orelse
        std.debug.panic("Shared TOML requires explicit -Dtoml-source=<absolute-owning-mod-checkout>", .{});
    if (!std.fs.path.isAbsolute(requested)) std.debug.panic("Shared TOML source must be absolute", .{});
    compat.poisonCache(b); // Git/index/untracked state is not a source-file dependency.
    const revision = std.mem.trim(u8, b.run(&.{ "git", "-C", requested, "rev-parse", "--verify", "HEAD" }), " \t\r\n");
    if (revision.len != 40) std.debug.panic("Shared TOML source HEAD is not a full commit", .{});
    for (revision) |char| if (!std.ascii.isHex(char) or (char >= 'A' and char <= 'F'))
        std.debug.panic("Shared TOML source HEAD is not lowercase hexadecimal", .{});
    const dirty = std.mem.trim(u8, b.run(&.{ "git", "-C", requested, "status", "--porcelain", "--untracked-files=all" }), " \t\r\n").len != 0;
    const differs = !std.mem.eql(u8, revision, pinned_revision) or dirty;
    if (differs and !options.allow_dirty)
        std.debug.panic("Shared TOML differs from immutable pin {s}; explicit development needs -Dtoml-allow-dirty=true", .{pinned_revision});
    const state = if (dirty) "development-dirty" else if (differs) "development-clean" else "pinned";
    std.log.info("Shared TOML build-only input: root={s}; revision={s}; state={s}", .{ requested, revision, state });
    const source: Source = .{ .root = .{ .cwd_relative = requested }, .revision = revision, .state = state };
    inline for (.{ "shared/toml/include/stfc_toml/c_api.h", "shared/toml/include/stfc_toml/editor.h", "shared/toml/src/native.cc", "shared/toml/tests/editor_test.cc", "shared/toml/THIRD_PARTY_NOTICES.txt", "LICENSE" }) |path|
        _ = compat.readFile(b, source.root.path(b, path), .limited(2 * 1024 * 1024));
    return source;
}

fn module(ctx: *const Ctx, source: Source, deps: Dependencies) *std.Build.Module {
    const out = ctx.createModule(true);
    out.addIncludePath(source.root.path(ctx.b, "shared/toml/include"));
    for (deps.toml.include_dirs) |dir| out.addIncludePath(dir);
    for (deps.json.include_dirs) |dir| out.addIncludePath(dir);
    out.addCMacro("STFC_TOML_BUILD", "1");
    out.addCMacro("TOML_HEADER_ONLY", "1");
    out.addCMacro("TOML_EXCEPTIONS", "1");
    out.addCMacro("NOMINMAX", "1");
    return out;
}

fn producerFlags(ctx: *const Ctx) []const []const u8 {
    return ctx.cxxFlags("c++23", &.{ "-fvisibility=hidden", "-fvisibility-inlines-hidden" });
}
