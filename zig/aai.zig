//! Pinned AAI producers and explicitly selected native host integration.
const std = @import("std");
const Ctx = @import("ctx.zig").Ctx;
const Lib = @import("ctx.zig").Lib;
const compat = @import("compat.zig");

pub const pinned_revision = "4bece2e5f1afa7c7343f009e83398742a0f86114";

pub const Options = struct {
    enabled: bool,
    native: bool,
    host: bool,
    ui_lab: bool,
    source: ?[]const u8,
    allow_dirty: bool,

    pub fn parse(b: *std.Build) Options {
        const host = b.option(bool, "aai-host", "Link the pinned native AAI product into the mod") orelse false;
        return .{
            .enabled = b.option(bool, "aai", "Expose opt-in AAI build targets") orelse host,
            .native = b.option(bool, "aai-native", "Expose the native AAI archive") orelse host,
            .host = host,
            .ui_lab = b.option(bool, "aai-ui-lab", "Include the pinned development workshop and native lab probes") orelse false,
            .source = b.option([]const u8, "aai-source", "Explicit absolute AAI checkout; immutable pin checked"),
            .allow_dirty = b.option(bool, "aai-allow-dirty", "Permit and report an explicit development checkout") orelse false,
        };
    }
};

pub const Dependencies = struct {
    json: Lib,
    il2cpp: Lib,
    spud: Lib,
    spdlog: Lib,
    toml: Lib,
    /// Supplied only when native was explicitly selected, from a reviewed source pin.
    simdutf: ?Lib = null,
};

pub const Outputs = struct {
    core: Lib,
    native: ?Lib,
};

const Source = struct {
    root: std.Build.LazyPath,
    revision: []const u8,
    state: []const u8,
};

/// Named producer steps alone never activate AAI. Only explicit aai-host links
/// its native output into the mod's existing DLL/dylib.
pub fn add(ctx: *const Ctx, options: Options, deps: Dependencies) ?Outputs {
    const b = ctx.b;
    if (options.host and (!options.enabled or !options.native))
        std.debug.panic("aai-host requires aai and aai-native; do not silently build a host without its owner", .{});
    const core_step = b.step("aai-core", "Build the pinned pure AAI analytical archive");
    const cli_step = b.step("aai-cli", "Build aai-replay, aai-combat-query and the analysis profiler");
    const native_step = b.step("aai-native", "Build the pinned native AAI archive without activating it");
    const inputs_step = b.step("aai-inputs", "Validate explicit AAI source identity without building AAI");
    const build_tests = b.step("aai-tests", "Build the pinned AAI analytical, lifecycle and component fixtures");
    const run_tests = b.step("test-aai", "Run the pinned AAI analytical, lifecycle and component fixtures");
    const host_tests = b.step("test-aai-host", "Run the production host adapter with synthetic AAI install/frame services");
    if (!options.host) host_tests.dependOn(&b.addFail("Select -Daai-host=true for host adapter fixtures").step);
    if (!options.enabled) {
        const disabled = b.addFail("AAI is not selected; use -Daai=true -Daai-source=<absolute-checkout>");
        core_step.dependOn(&disabled.step);
        cli_step.dependOn(&disabled.step);
        native_step.dependOn(&disabled.step);
        inputs_step.dependOn(&disabled.step);
        build_tests.dependOn(&disabled.step);
        run_tests.dependOn(&disabled.step);
        return null; // No private path probe, Git command, dependency fetch or source read.
    }
    const source = verifySource(b, options);
    const core = addCore(ctx, source, deps.json);
    core_step.dependOn(&b.addInstallArtifact(core.artifact.?, .{}).step);
    core.addBuildDependencies(core_step);
    addFixtures(ctx, source, core, build_tests, run_tests);

    inline for (.{
        .{ "aai-replay", "tools/aai_replay.cc" },
        .{ "aai-combat-query", "tools/aai_combat_query.cc" },
        .{ "aai-analysis-profile", "tools/aai_analysis_profile.cc" },
    }) |entry| {
        const module = ctx.createModule(true);
        core.linkInto(module);
        module.addCSourceFile(.{ .file = source.root.path(b, entry[1]), .flags = ctx.cxxFlags("c++23", &.{}) });
        const exe = ctx.executable(entry[0], module);
        cli_step.dependOn(&b.addInstallArtifact(exe, .{}).step);
    }

    if (!options.native) {
        native_step.dependOn(&b.addFail("Native AAI not selected; add -Daai-native=true and its pinned simdutf dependency").step);
        return .{ .core = core, .native = null };
    }
    if (!ctx.isWindows() and !ctx.isMacos())
        std.debug.panic("Native AAI requires Windows/macOS; keep -Daai-native=false for portable core/CLI targets", .{});
    const native = addNative(ctx, source, deps, options);
    if (options.host) addHostFixture(ctx, source, deps.spdlog, deps.toml, host_tests);
    native_step.dependOn(&b.addInstallArtifact(native.artifact.?, .{}).step);
    native.addBuildDependencies(native_step);
    return .{ .core = core, .native = native };
}

fn verifySource(b: *std.Build, options: Options) Source {
    const requested = options.source orelse
        std.debug.panic("AAI requires explicit -Daai-source=<absolute-checkout>; no sibling/default lookup is performed", .{});
    if (!std.fs.path.isAbsolute(requested))
        std.debug.panic("AAI source override must be absolute: {s}", .{requested});
    // Git observes .git/index/ignored state outside the source file graph. Never
    // let a cached configure step reuse an earlier clean revision decision.
    compat.poisonCache(b);
    const revision = std.mem.trim(u8, b.run(&.{ "git", "-C", requested, "rev-parse", "--verify", "HEAD" }), " \t\r\n");
    if (revision.len != 40) std.debug.panic("AAI HEAD is not a full 40-character commit", .{});
    for (revision) |ch| if (!std.ascii.isHex(ch) or (ch >= 'A' and ch <= 'F'))
        std.debug.panic("AAI HEAD is not a lowercase hexadecimal commit", .{});
    // Match the existing embed contract: dirty includes untracked, non-ignored
    // files. Failure to inspect Git is a build failure, never assumed clean.
    const dirty = std.mem.trim(u8, b.run(&.{
        "git", "-C", requested, "status", "--porcelain", "--untracked-files=all",
    }), " \t\r\n").len != 0;
    const differs = !std.mem.eql(u8, revision, pinned_revision) or dirty;
    if (differs and !options.allow_dirty)
        std.debug.panic("AAI differs from immutable pin {s}; explicit development requires -Daai-allow-dirty=true", .{pinned_revision});
    const state = if (dirty) "development-dirty" else if (differs) "development-clean" else "pinned";
    std.log.info("AAI build-only input: root={s}; revision={s}; state={s}", .{ requested, revision, state });
    const source: Source = .{ .root = .{ .cwd_relative = requested }, .revision = revision, .state = state };
    // Required marker is read only after enablement and immutable-pin checks.
    _ = compat.readFile(b, source.root.path(b, "include/stfc_aai/overlay.h"), .limited(64 * 1024));
    return source;
}

fn addCore(ctx: *const Ctx, source: Source, json: Lib) Lib {
    const b = ctx.b;
    var lib: Lib = .{
        .include_dirs = ctx.dirs(&.{source.root.path(b, "include")}),
        .dependencies = ctx.libs(&.{json}),
        .system_libs = if (ctx.isWindows()) &.{"bcrypt"} else &.{},
    };
    const module = ctx.createModule(true);
    lib.includeInto(module);
    json.includeInto(module);
    module.addCSourceFiles(.{
        .root = source.root,
        // Same implementation used by pinned xmake.lua's selected-ship CLI entries.
        .files = &.{
            "src/battle_replay.cc",    "src/battle_evidence.cc",        "src/catalog_annotations.cc",
            "src/battle_snapshots.cc", "src/selected_ship_evidence.cc", "src/analysis.cc",
            "src/combat_ledger.cc",    "src/ship_timeline.cc",          "src/combat_history.cc",
            "src/combat_queries.cc",
        },
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    lib.artifact = ctx.staticLib("stfc-aai-core", module);
    return lib;
}

fn addFixtures(
    build_ctx: *const Ctx,
    source: Source,
    core: Lib,
    build_step: *std.Build.Step,
    run_step: *std.Build.Step,
) void {
    const ctx = build_ctx.withOptimize(build_ctx.options.test_optimize orelse build_ctx.optimize);
    const b = ctx.b;
    // The pinned Xmake test inventory, mirrored explicitly so build coverage is
    // reviewable. Worker/probe implementation is fixture-only, never the native
    // product's dependency. All tests retain their source assertions.
    const worker_module = ctx.createModule(true);
    core.includeInto(worker_module);
    for (core.dependencies) |dep| dep.includeInto(worker_module);
    worker_module.addCMacro("STFC_AAI_LAB_PROBES", "1");
    worker_module.addCSourceFiles(.{
        .root = source.root,
        .files = &.{ "src/native_observation.cc", "src/native_analysis_lab.cc", "src/crew_effects.cc", "src/tactical_presentation.cc" },
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    const workers: Lib = .{ .artifact = ctx.staticLib("stfc-aai-fixture-workers", worker_module), .dependencies = ctx.libs(&.{core}) };
    inline for (.{
        "battle_log_analysis_data", "battle_log_analysis_input", "battle_log_analysis_scroll", "game_strings",
        "report_view_discovery",    "attack_normalization",      "officer_normalization",      "battle_progression",
        "crew_row_state",           "effect_sources",            "consumable_names",           "hook_installation",
        "combat_positions",         "exact_observation_sum",     "modifier_relevance",         "modifier_domains",
        "tactical_selection",       "tactical_history_geometry", "stfckit_numbers",            "stfckit_surfaces",
        "stfckit_components",       "stfckit_fixtures",          "stfckit_aai_examples",       "battle_evidence",
        "combat_capacity",          "report_health",             "battle_replay",              "catalog_annotations",
        "battle_snapshots",         "selected_ship_evidence",    "analysis",                   "native_observation",
        "combat_ledger",            "ship_timeline",             "interval_audit",             "combat_queries",
        "combat_history",           "native_capacity",           "report_vitals_context",      "native_analysis_lab",
        "tactical_presentation",    "crew_effects",
    }) |name| {
        const module = ctx.createModule(true);
        workers.linkInto(module);
        core.includeInto(module);
        if (comptime std.mem.eql(u8, name, "native_analysis_lab")) module.addCMacro("STFC_AAI_LAB_PROBES", "1");
        module.addCSourceFile(.{
            .file = source.root.path(b, b.fmt("tests/{s}_test.cc", .{name})),
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        const exe = ctx.executable(b.fmt("aai-{s}-tests", .{name}), module);
        build_step.dependOn(&b.addInstallArtifact(exe, .{}).step);
        run_step.dependOn(&b.addRunArtifact(exe).step);
    }
}

fn addNative(ctx: *const Ctx, source: Source, deps: Dependencies, options: Options) Lib {
    const b = ctx.b;
    const unicode = deps.simdutf orelse
        std.debug.panic("Native AAI requires reviewed, pinned simdutf v8.2.0; no conversion fallback is substituted", .{});
    var lib: Lib = .{
        .include_dirs = ctx.dirs(&.{source.root.path(b, "include")}),
        .dependencies = ctx.libs(&.{ deps.json, deps.il2cpp, deps.spud, deps.spdlog, unicode }),
        .system_libs = if (ctx.isWindows()) &.{"bcrypt"} else &.{},
        .defines = if (options.host) b.allocator.dupe([2][]const u8, &.{
            .{ "STFC_AAI_ENABLED", "1" },
            .{ "STFC_AAI_SOURCE_REVISION", quoted(b, source.revision) },
            .{ "STFC_AAI_SOURCE_STATE", quoted(b, source.state) },
        }) catch @panic("OOM") else &.{},
    };
    const module = ctx.createModule(true);
    lib.includeInto(module);
    for (lib.dependencies) |dep| dep.includeInto(module);
    module.addCMacro("STFC_AAI_SOURCE_REVISION", quoted(b, source.revision));
    module.addCMacro("STFC_AAI_SOURCE_STATE", quoted(b, source.state));
    if (options.ui_lab) {
        module.addCMacro("STFC_AAI_UI_LAB", "1");
        module.addCMacro("STFC_AAI_LAB_PROBES", "1");
    }
    // Only the hook consumer acquires the host adapter. Its analytics, native
    // decoder and other UI TUs remain byte-for-byte pinned source.
    const native_flags = ctx.cxxFlags("c++23", &.{ "-fms-extensions", "-Wno-invalid-offsetof" });
    const platform_flags = ctx.cxxFlags("c++23", &.{
        "-fms-extensions", "-Wno-invalid-offsetof", "-include", b.pathFromRoot("src/mod/aai_native_platform.h"),
    });
    const hook_flags = if (options.host) std.mem.concat(b.allocator, []const u8, &.{
        platform_flags, &.{ "-include", b.pathFromRoot("src/mod/aai_host_detour.h") },
    }) catch @panic("OOM") else platform_flags;
    module.addCSourceFile(.{ .file = source.root.path(b, "src/battle_log_analysis.cc"), .flags = hook_flags });
    module.addCSourceFiles(.{
        .root = source.root,
        // Exact complete list from pinned xmake/embed.lua; no parallel parser.
        .files = &.{
            "src/ui_officer_fixture.cc",    "src/battle_evidence.cc",
            "src/catalog_annotations.cc",   "src/battle_replay.cc",
            "src/battle_snapshots.cc",      "src/selected_ship_evidence.cc",
            "src/analysis.cc",              "src/native_observation.cc",
            "src/native_analysis_lab.cc",   "src/crew_effects.cc",
            "src/combat_ledger.cc",         "src/ship_timeline.cc",
            "src/combat_history.cc",        "src/combat_queries.cc",
            "src/tactical_presentation.cc",
        },
        .flags = native_flags,
    });
    lib.artifact = ctx.staticLib("stfc-aai-native", module);
    return lib;
}

fn addHostFixture(build_ctx: *const Ctx, source: Source, spdlog: Lib, toml: Lib, step: *std.Build.Step) void {
    const ctx = build_ctx.withOptimize(build_ctx.options.test_optimize orelse build_ctx.optimize);
    const b = ctx.b;
    const module = ctx.createModule(true);
    module.addIncludePath(b.path("src/mod"));
    module.addIncludePath(b.path("src/libs/il2cpp/include"));
    module.addIncludePath(b.path("src/libs/unity/include"));
    if (ctx.isWindows()) module.addLibraryPath(b.path("src/libs/il2cpp"));
    module.addIncludePath(b.path("third_party/libil2cpp"));
    module.addIncludePath(b.path("third_party/spud/include"));
    module.addIncludePath(source.root.path(b, "include"));
    module.addCMacro("STFC_AAI_ENABLED", "1");
    module.addCMacro("STFC_AAI_SOURCE_REVISION", quoted(b, source.revision));
    module.addCMacro("STFC_AAI_SOURCE_STATE", quoted(b, source.state));
    spdlog.linkInto(module);
    toml.includeInto(module);
    module.addCSourceFiles(.{
        .files = &.{ "tests/aai_host_test.cc", "src/mod/features/aai.cc" },
        .flags = ctx.cxxFlags("c++23", &.{ "-UNDEBUG", "-Wno-invalid-offsetof" }),
    });
    const exe = ctx.executable("aai-host-tests", module);
    inline for (.{ "complete", "observer-missing", "back-missing", "both-missing", "unsafe-contained" }) |mode| {
        const run = b.addRunArtifact(exe);
        run.addArg(mode);
        step.dependOn(&run.step);
    }
}

/// Public simdutf CMake target is one TU with include/ public and src/ private.
/// Caller supplies a hash-verified v8.2.0 package root, never a guessed cache path.
pub fn simdutf(ctx: *const Ctx, root: std.Build.LazyPath) Lib {
    var lib: Lib = .{ .include_dirs = ctx.dirs(&.{root.path(ctx.b, "include")}) };
    const module = ctx.createModule(true);
    lib.includeInto(module);
    module.addIncludePath(root.path(ctx.b, "src"));
    // Zig's baseline explicitly disables evex512. simdutf 8.2's existing
    // Icelake target region enables AVX512 but predates Clang 21's separate
    // encoding feature, so its intrinsics otherwise fail to inline. This
    // TU-local flag lets those attributed functions use their required width;
    // ordinary baseline functions retain identical features (no AVX/AVX2/512).
    // Keep runtime dispatch and all kernels intact; never enable global AVX512.
    const extra: []const []const u8 = if (ctx.target.result.cpu.arch == .x86_64)
        &.{ "-Xclang", "-target-feature", "-Xclang", "+evex512" }
    else
        &.{};
    module.addCSourceFile(.{ .file = root.path(ctx.b, "src/simdutf.cpp"), .flags = ctx.cxxFlags("c++17", extra) });
    lib.artifact = ctx.staticLib("simdutf", module);
    return lib;
}

fn quoted(b: *std.Build, value: []const u8) []const u8 {
    return std.json.Stringify.valueAlloc(b.allocator, value, .{}) catch @panic("OOM");
}
