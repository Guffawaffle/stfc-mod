//! Pinned Profiles producers remain inert. Only explicit profiles-host links the reviewed host adapter into the mod.
const std = @import("std");
const Ctx = @import("ctx.zig").Ctx;
const Lib = @import("ctx.zig").Lib;
const compat = @import("compat.zig");

pub const pinned_revision = "c5c4ba911f16f3376e828761714d72a26ebe2a96";
pub const pinned_archive_sha256 = "f120590499bb51844c1a2cd274f5bd58dbae0626b72c2d9f800041fabd08fd63";

pub const Options = struct {
    enabled: bool,
    adapters: bool,
    host: bool,
    source: ?[]const u8,
    archive: ?[]const u8,
    prefix: ?[]const u8,
    receipt: ?[]const u8,
    python: []const u8,

    pub fn parse(b: *std.Build) Options {
        return .{
            .enabled = b.option(bool, "profiles", "Expose opt-in Profiles dependency targets; does not activate isolation") orelse false,
            .adapters = b.option(bool, "profiles-adapters", "Also compile/link all three inert community-mod adapters") orelse false,
            .host = b.option(bool, "profiles-host", "Link the reviewed Profiles host into the mod; requires profiles and profiles-adapters") orelse false,
            .source = b.option([]const u8, "profiles-source", "Absolute source root extracted from the pinned Profiles archive"),
            .archive = b.option([]const u8, "profiles-archive", "Absolute published Profiles source archive; SHA-256 checked"),
            .prefix = b.option([]const u8, "profiles-deps-prefix", "Absolute verified libarchive/xz/zlib static prefix"),
            .receipt = b.option([]const u8, "profiles-deps-receipt", "Absolute source/config/tool/output receipt for that prefix"),
            .python = b.option([]const u8, "profiles-python", "Python executable for source/prefix verification") orelse "python",
        };
    }
};

pub const Dependencies = struct { json: Lib, spud: Lib, spdlog: Lib, il2cpp: Lib };
pub const Outputs = struct {
    core: Lib,
    /// Original pinned adapter, retained for independent M1b producers/fixtures.
    adapters: ?Lib,
    /// Host-derived isolation TU plus the pinned admission/browser TUs; never link both adapters into one product.
    host: ?Lib,
    zlib: Lib,
    verification: *std.Build.Step,
};

/// These steps are deliberately separate from the default install step and final mod.
/// Disabled options perform no private source reads, prefix inspection or network access.
pub fn add(ctx: *const Ctx, options: Options, deps: Dependencies) ?Outputs {
    const b = ctx.b;
    const core_step = b.step("profiles-core", "Build the full pinned Profiles core archive, without activation");
    const adapter_step = b.step("profiles-adapters", "Build all three pinned Profiles adapter translation units");
    const test_step = b.step("profiles-tests", "Run game-independent Profiles contracts on disposable fixture state");
    const compile_tests = b.step("profiles-build-tests", "Compile Profiles contracts without running them");
    if (options.host and (!options.enabled or !options.adapters))
        std.debug.panic("-Dprofiles-host=true requires explicit -Dprofiles=true and -Dprofiles-adapters=true", .{});
    if (!options.enabled) {
        const failure = b.addFail("Profiles not selected; use -Dprofiles=true with explicit source/archive/prefix/receipt");
        inline for (.{ core_step, adapter_step, test_step, compile_tests }) |step| step.dependOn(&failure.step);
        return null;
    }
    if (!ctx.isWindows() and !ctx.isMacos())
        std.debug.panic("Profiles supports native Windows and macOS hosts, not the Linux portable fixture lane", .{});
    if (ctx.isMacos() and ctx.target.result.cpu.arch != .aarch64 and ctx.target.result.cpu.arch != .x86_64)
        std.debug.panic("Profiles macOS supports only arm64 and x86_64", .{});
    const root: std.Build.LazyPath = .{ .cwd_relative = required(options.source, "profiles-source") };
    const archive: std.Build.LazyPath = .{ .cwd_relative = required(options.archive, "profiles-archive") };
    const prefix: std.Build.LazyPath = .{ .cwd_relative = required(options.prefix, "profiles-deps-prefix") };
    const receipt: std.Build.LazyPath = .{ .cwd_relative = required(options.receipt, "profiles-deps-receipt") };
    const target = if (ctx.isWindows()) "x86_64-windows-msvc" else if (ctx.isAarch64()) "aarch64-macos.14.6" else "x86_64-macos.14.6";
    if (ctx.isWindows() and ctx.target.result.cpu.arch != .x86_64)
        std.debug.panic("The Profiles Windows prefix is qualified for x86_64 only", .{});
    const verify = b.addSystemCommand(&.{options.python});
    verify.addArgs(&.{"-B"});
    verify.addFileArg(b.path("scripts/prepare-profiles-deps.py"));
    verify.addArgs(&.{ "verify", "--profiles-source", required(options.source, "profiles-source"), "--profiles-archive" });
    verify.addFileArg(archive);
    verify.addArgs(&.{ "--prefix", required(options.prefix, "profiles-deps-prefix"), "--receipt" });
    verify.addFileArg(receipt);
    verify.addArgs(&.{ "--target", target });
    // External source/prefix trees may be modified independently of Git or the
    // build cache. Re-read their complete hash manifests for every requested build.
    verify.has_side_effects = true;

    if (options.host) {
        // All host include consumers must see verified source/prefix bytes,
        // including curl's headers before its own compile step. Keep the existing
        // build-time gate too; no general module/step registry is needed.
        compat.poisonCache(b);
        _ = b.run(&.{
            options.python,      "-B",                                             b.pathFromRoot("scripts/prepare-profiles-deps.py"), "verify",
            "--profiles-source", required(options.source, "profiles-source"),      "--profiles-archive",                               required(options.archive, "profiles-archive"),
            "--prefix",          required(options.prefix, "profiles-deps-prefix"), "--receipt",                                        required(options.receipt, "profiles-deps-receipt"),
            "--target",          target,
        });
    }

    // In a host product, libarchive and curl use exactly the same verified
    // archive, with no second Zig-built zlib or SDK-search-path substitution.
    const zlib: Lib = .{
        .include_dirs = ctx.dirs(&.{prefix.path(b, "include")}),
        .static_files = ctx.dirs(&.{prefix.path(b, if (ctx.isWindows()) "lib/zs.lib" else "lib/libz.a")}),
    };

    const external: Lib = .{
        .include_dirs = ctx.dirs(&.{prefix.path(b, "include")}),
        .lib_dirs = ctx.dirs(&.{prefix.path(b, "lib")}),
        .defines = if (ctx.isWindows()) &.{ .{ "LIBARCHIVE_STATIC", "1" }, .{ "LZMA_API_STATIC", "1" } } else &.{},
        .dependencies = if (options.host) ctx.libs(&.{zlib}) else &.{},
        .static_files = if (options.host) ctx.dirs(&.{
            prefix.path(b, if (ctx.isWindows()) "lib/archive.lib" else "lib/libarchive.a"),
            prefix.path(b, if (ctx.isWindows()) "lib/lzma.lib" else "lib/liblzma.a"),
        }) else &.{},
        .system_libs = if (options.host and ctx.isWindows())
            &.{ "advapi32", "bcrypt", "crypt32", "shlwapi", "user32", "ws2_32", "xmllite", "uuid" }
        else if (options.host)
            &.{}
        else if (ctx.isWindows())
            &.{ "archive", "lzma", "zs", "advapi32", "bcrypt", "crypt32", "shlwapi", "user32", "ws2_32", "xmllite", "uuid" }
        else
            &.{ "archive", "lzma", "z" },
    };
    var core: Lib = .{
        .include_dirs = ctx.dirs(&.{root.path(b, "include")}),
        .dependencies = ctx.libs(&.{ deps.json, external }),
        .system_libs = if (ctx.isWindows()) &.{ "crypt32", "bcrypt", "shell32", "ole32", "advapi32", "winhttp" } else &.{"proc"},
        .frameworks = if (ctx.isMacos()) &.{ "Security", "CoreFoundation" } else &.{},
        .defines = &.{ .{ "STFC_PROFILES_SOURCE_REVISION", "\"" ++ pinned_revision ++ "\"" }, .{ "STFC_PROFILES_SOURCE_STATE", "\"pinned\"" } },
    };
    const module = ctx.createModule(true);
    core.includeInto(module);
    deps.json.includeInto(module);
    external.includeInto(module);
    module.addCSourceFiles(.{
        .root = root,
        // Exact full embeddable core from pinned xmake/library.lua. No c_api,
        // CLI or runtime bootstrap is substituted for or added to this owner.
        .files = &.{ "src/component.cc", "src/catalog.cc", "src/prefs_store.cc", "src/installation.cc" },
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    module.addCSourceFiles(.{
        .root = root,
        .files = if (ctx.isWindows())
            &.{ "src/windows/prefs_store.cc", "src/windows/user_import_source.cc", "src/windows/user_import_transfer.cc" }
        else
            &.{"src/macos/prefs_store.cc"},
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    core.artifact = ctx.staticLib("stfc-profiles-core", module);
    core.artifact.?.step.dependOn(&verify.step);
    core_step.dependOn(&b.addInstallArtifact(core.artifact.?, .{}).step);

    var adapters: ?Lib = null;
    if (options.adapters) {
        var lib: Lib = .{
            .include_dirs = ctx.dirs(&.{root.path(b, "include")}),
            .dependencies = ctx.libs(&.{ core, deps.spud, deps.spdlog }),
        };
        const adapter_module = ctx.createModule(true);
        core.includeInto(adapter_module);
        // Profiles' public prefs/user-import headers expose JSON types. Merely
        // including the core's own path does not include its dependency paths.
        deps.json.includeInto(adapter_module);
        deps.spud.includeInto(adapter_module);
        deps.spdlog.includeInto(adapter_module);
        // Pinned RuntimeApi resolves its own symbols dynamically. No host
        // feature headers, GameAssembly import library or second IL2CPP owner.
        adapter_module.addCSourceFiles(.{
            .root = root,
            .files = &.{ "adapters/community_mod/profile_isolation.cc", "adapters/community_mod/process_admission.cc", "adapters/community_mod/browser_guardian.cc" },
            .flags = ctx.cxxFlags("c++23", &.{}),
        });
        lib.artifact = ctx.staticLib("stfc-profiles-community-mod-adapter", adapter_module);
        lib.artifact.?.step.dependOn(&verify.step);
        adapter_step.dependOn(&b.addInstallArtifact(lib.artifact.?, .{}).step);
        lib.addBuildDependencies(adapter_step);
        adapters = lib;
    } else {
        adapter_step.dependOn(&b.addFail("Adapter compilation requires -Dprofiles-adapters=true").step);
    }

    var host: ?Lib = null;
    if (options.host) {
        var lib: Lib = .{
            .include_dirs = ctx.dirs(&.{root.path(b, "include")}),
            .defines = &.{.{ "STFC_PROFILES_HOST", "1" }},
            .dependencies = ctx.libs(&.{ core, deps.il2cpp, deps.spud, deps.spdlog }),
        };
        const host_module = ctx.createModule(true);
        lib.includeInto(host_module);
        core.includeInto(host_module);
        deps.json.includeInto(host_module);
        deps.il2cpp.includeInto(host_module);
        deps.spud.includeInto(host_module);
        deps.spdlog.includeInto(host_module);
        host_module.addIncludePath(root.path(b, "adapters/community_mod"));
        host_module.addCSourceFiles(.{
            .root = root,
            .files = &.{ "adapters/community_mod/process_admission.cc", "adapters/community_mod/browser_guardian.cc" },
            .flags = ctx.cxxFlags("c++23", &.{}),
        });
        host_module.addCSourceFile(.{ .file = b.path("src/profiles/profile_isolation.cc"), .flags = ctx.cxxFlags("c++23", &.{}) });
        lib.artifact = ctx.staticLib("stfc-profiles-host-adapter", host_module);
        lib.artifact.?.step.dependOn(&verify.step);
        host = lib;
    }

    const fixture_ctx = ctx.withOptimize(ctx.options.test_optimize orelse @import("ctx.zig").OptimizeMode.Debug);
    inline for (.{
        .{ "identity", "tests/identity_test.cc" },
        .{ "consumer", "tests/consumer_smoke.cc" },
        .{ "preferences", "tests/prefs_store_test.cc" },
        .{ "catalog", "tests/catalog_test.cc" },
    }) |fixture| addFixture(fixture_ctx, core, root.path(b, fixture[1]), fixture[0], &verify.step, compile_tests, test_step);
    if (ctx.isWindows())
        // This pinned test includes installation.cc in its own TU, as its
        // original recipe does, to reach private synthetic crash checkpoints.
        addFixture(fixture_ctx, core, root.path(b, "tests/installation_test.cc"), "installation", &verify.step, compile_tests, test_step);
    if (adapters) |lib|
        addFixture(fixture_ctx, lib, b.path("tests/profiles_link_test.cc"), "adapter-link", &verify.step, compile_tests, test_step);
    return .{ .core = core, .adapters = adapters, .host = host, .zlib = zlib, .verification = &verify.step };
}

fn addFixture(ctx: *const Ctx, lib: Lib, source: std.Build.LazyPath, name: []const u8, verification: *std.Build.Step, compile_tests: *std.Build.Step, tests: *std.Build.Step) void {
    const b = ctx.b;
    const module = ctx.createModule(true);
    lib.linkInto(module);
    if (ctx.isWindows()) module.linkSystemLibrary("uuid", .{});
    module.addCSourceFile(.{ .file = source, .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
    const exe = ctx.executable(b.fmt("profiles-{s}-tests", .{name}), module);
    exe.step.dependOn(verification);
    compile_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
    tests.dependOn(&b.addRunArtifact(exe).step);
}

fn required(value: ?[]const u8, name: []const u8) []const u8 {
    const path = value orelse std.debug.panic("Profiles requires explicit -D{s}=<absolute-path>", .{name});
    if (!std.fs.path.isAbsolute(path)) std.debug.panic("Profiles input must be absolute: -D{s}={s}", .{ name, path });
    return path;
}
