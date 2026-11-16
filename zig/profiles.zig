//! Optional pinned Profiles core, matching CLI and the mod's shared-hook host adapter.
//! Only enabled Windows builds need XMake: it supplies the pinned archive libraries, never a profile runtime.
const std = @import("std");
const builtin = @import("builtin");
const Ctx = @import("ctx.zig").Ctx;
const Lib = @import("ctx.zig").Lib;

pub const revision = "1cd7a9ccdba5df872b95b9342c0eb8ef8a7cb6eb";

pub const Component = struct {
    source: std.Build.LazyPath,
    runtime: Lib,
};

/// Called only with -Dprofiles. A missing lazy package asks Zig to fetch it and configure again.
pub fn add(ctx: *const Ctx, json: Lib, spud: Lib, spdlog: Lib, il2cpp: Lib, zlib: Lib) ?Component {
    const b = ctx.b;
    if (!ctx.isWindows() and !ctx.isMacos())
        std.debug.panic("Profiles requires a Windows or macOS target", .{});
    if (ctx.isMacos() and !ctx.isAarch64() and ctx.target.result.cpu.arch != .x86_64)
        std.debug.panic("Profiles macOS supports only arm64 and x86_64", .{});
    const source = (b.lazyDependency("stfc_profiles", .{}) orelse return null).path("");
    const archive: Lib = if (ctx.isWindows()) windowsArchive(ctx, source, zlib) orelse return null else .{};
    const core = coreLibrary(ctx, source, json, archive, false);

    const adapter_module = ctx.createModule(true);
    core.includeInto(adapter_module);
    json.includeInto(adapter_module);
    spud.includeInto(adapter_module);
    spdlog.includeInto(adapter_module);
    il2cpp.includeInto(adapter_module);
    adapter_module.addIncludePath(source.path(b, "adapters/community_mod"));
    adapter_module.addCSourceFiles(.{
        .root = source,
        .files = &.{
            "adapters/community_mod/process_admission.cc",
            "adapters/community_mod/browser_guardian.cc",
        },
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    adapter_module.addCSourceFile(.{ .file = b.path("src/profiles/profile_isolation.cc"), .flags = ctx.cxxFlags("c++23", &.{}) });
    const adapter: Lib = .{
        .artifact = ctx.staticLib("stfc-profiles-host-adapter", adapter_module),
        .dependencies = ctx.libs(&.{ core, spud, spdlog, il2cpp }),
        .include_dirs = core.include_dirs,
        .defines = &.{ .{ "STFC_PROFILES_HOST", "1" }, .{ "STFC_PROFILES_ENABLED", "1" } },
    };

    const cli_module = ctx.createModule(true);
    core.linkInto(cli_module);
    cli_module.addCSourceFile(.{ .file = source.path(b, "cli/main.cc"), .flags = ctx.cxxFlags("c++23", &.{}) });
    const cli = ctx.executable("stfc-profiles", cli_module);
    if (ctx.isWindows()) {
        cli_module.linkSystemLibrary("uuid", .{});
        cli_module.linkSystemLibrary("comctl32", .{});
        // Resource type 24 embeds the source-owned Common Controls/asInvoker manifest.
        const manifest = b.addWriteFiles();
        _ = manifest.addCopyFile(source.path(b, "cli/windows.manifest"), "windows.manifest");
        const rc = manifest.add("profiles-cli.rc", "1 24 \"windows.manifest\"\n");
        cli_module.addWin32ResourceFile(.{ .file = rc, .include_paths = &.{manifest.getDirectory()} });
    }
    b.installArtifact(cli);
    const step = b.step("profiles", "Build the optional Profiles core, host adapter and matching CLI");
    step.dependOn(&b.addInstallArtifact(cli, .{}).step);
    adapter.addBuildDependencies(step);
    if (ctx.isMacos()) {
        // dladdr locates the guardian beside the loaded dylib, including an unassembled install prefix.
        const adjacent = b.addInstallArtifact(cli, .{ .dest_dir = .{ .override = .lib } });
        b.getInstallStep().dependOn(&adjacent.step);
        step.dependOn(&adjacent.step);
    }
    const receipt = b.addWriteFiles().add("profiles-source.txt", "Guffawaffle/stfc-profiles\n" ++ revision ++ "\n");
    b.getInstallStep().dependOn(&b.addInstallFile(receipt, "share/stfc-profiles/source.txt").step);
    b.getInstallStep().dependOn(&b.addInstallFile(source.path(b, "LICENSE"), "share/stfc-profiles/LICENSE").step);
    addFixtures(ctx, source, core, fixtureAdapter(ctx, source, core, json, spud, spdlog));
    return .{ .source = source, .runtime = adapter };
}

// The unmodified dependency adapter remains a standalone fixture producer. It
// resolves runtime symbols dynamically, so its link test never loads GameAssembly.
// It is never linked beside the mod's host adapter in the shipped product.
fn fixtureAdapter(ctx: *const Ctx, source: std.Build.LazyPath, core: Lib, json: Lib, spud: Lib, spdlog: Lib) Lib {
    const module = ctx.createModule(true);
    inline for (.{ core, json, spud, spdlog }) |lib| lib.includeInto(module);
    module.addCSourceFiles(.{
        .root = source,
        .files = &.{ "adapters/community_mod/profile_isolation.cc", "adapters/community_mod/process_admission.cc", "adapters/community_mod/browser_guardian.cc" },
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    return .{
        .artifact = ctx.staticLib("stfc-profiles-community-mod-adapter", module),
        .dependencies = ctx.libs(&.{ core, spud, spdlog }),
        .include_dirs = core.include_dirs,
    };
}

/// Retain the game-free fixture targets against the product's pinned source/core.
fn addFixtures(ctx: *const Ctx, source: std.Build.LazyPath, core: Lib, adapter: Lib) void {
    const b = ctx.b;
    const core_step = b.step("profiles-core", "Build the pinned Profiles core archive");
    core_step.dependOn(&b.addInstallArtifact(core.artifact.?, .{}).step);
    const adapter_step = b.step("profiles-adapters", "Build the pinned community-mod adapters without activation");
    adapter_step.dependOn(&b.addInstallArtifact(adapter.artifact.?, .{}).step);
    adapter.addBuildDependencies(adapter_step);
    const tests = b.step("profiles-tests", "Run game-independent Profiles contracts on disposable fixture state");
    const compile_tests = b.step("profiles-build-tests", "Compile Profiles contracts without running them");
    const fixture_ctx = ctx.withOptimize(ctx.options.test_optimize orelse @import("ctx.zig").OptimizeMode.Debug);
    // The pinned catalog contract also checks its allocator-safe native C ABI.
    // Keep this DLL and its fixture executable in a disposable generated directory.
    const catalog_native: ?*std.Build.Step.Compile = if (ctx.isWindows()) nativeFixture(fixture_ctx, source, core) else null;
    inline for (.{
        .{ "identity", "tests/identity_test.cc" },
        .{ "consumer", "tests/consumer_smoke.cc" },
        .{ "preferences", "tests/prefs_store_test.cc" },
        .{ "catalog", "tests/catalog_test.cc" },
    }) |fixture| addFixture(fixture_ctx, core, source.path(b, fixture[1]), fixture[0], compile_tests, tests, if (std.mem.eql(u8, fixture[0], "catalog")) catalog_native else null);
    if (ctx.isWindows())
        addFixture(fixture_ctx, core, source.path(b, "tests/installation_test.cc"), "installation", compile_tests, tests, null);
    addFixture(fixture_ctx, adapter, b.path("tests/profiles_link_test.cc"), "adapter-link", compile_tests, tests, null);
}

fn nativeFixture(ctx: *const Ctx, source: std.Build.LazyPath, core: Lib) *std.Build.Step.Compile {
    const module = ctx.createModule(true);
    core.linkInto(module);
    module.addCMacro("STFC_PROFILES_EXPORTS", "1");
    module.addCSourceFile(.{ .file = source.path(ctx.b, "src/c_api.cc"), .flags = ctx.cxxFlags("c++23", &.{}) });
    const native = ctx.b.addLibrary(.{ .name = "stfc-profiles-native", .linkage = .dynamic, .root_module = module });
    ctx.applyCompile(native);
    return native;
}

fn addFixture(ctx: *const Ctx, lib: Lib, source: std.Build.LazyPath, name: []const u8, compile_tests: *std.Build.Step, tests: *std.Build.Step, native: ?*std.Build.Step.Compile) void {
    const b = ctx.b;
    const module = ctx.createModule(true);
    lib.linkInto(module);
    if (ctx.isWindows()) module.linkSystemLibrary("uuid", .{});
    module.addCSourceFile(.{ .file = source, .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
    const exe = ctx.executable(b.fmt("profiles-{s}-tests", .{name}), module);
    compile_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
    if (native) |library| {
        const stage = b.addWriteFiles();
        const executable = stage.addCopyFile(exe.getEmittedBin(), "profiles-catalog-tests.exe");
        _ = stage.addCopyFile(library.getEmittedBin(), "stfc-profiles-native.dll");
        const run = std.Build.Step.Run.create(b, "run Profiles catalog/native ABI fixture");
        run.addFileArg(executable);
        run.step.dependOn(&stage.step);
        compile_tests.dependOn(&library.step);
        tests.dependOn(&run.step);
    } else {
        tests.dependOn(&b.addRunArtifact(exe).step);
    }
}

/// A separate core archive for the Swift launcher uses the SDK's libc++ headers, just like its rsync bridge.
/// It contains no game adapter or bootstrap; c_api.cc supplies the allocator-safe C boundary.
pub fn launcherLibrary(ctx: *const Ctx, source: std.Build.LazyPath, json: Lib) Lib {
    return coreLibrary(ctx, source, json, .{}, true);
}

fn coreLibrary(ctx: *const Ctx, source: std.Build.LazyPath, json: Lib, archive: Lib, launcher: bool) Lib {
    const b = ctx.b;
    var lib: Lib = .{
        .include_dirs = ctx.dirs(&.{source.path(b, "include")}),
        .dependencies = ctx.libs(&.{ json, archive }),
        .system_libs = if (ctx.isWindows())
            &.{ "crypt32", "bcrypt", "shell32", "ole32", "advapi32", "winhttp", "uuid" }
        else
            &.{"proc"},
        .frameworks = if (ctx.isMacos()) &.{ "Security", "CoreFoundation" } else &.{},
        .defines = &.{ .{ "STFC_PROFILES_SOURCE_REVISION", "\"" ++ revision ++ "\"" }, .{ "STFC_PROFILES_SOURCE_STATE", "\"pinned\"" } },
    };
    const module = ctx.createModule(!launcher);
    if (launcher) module.addIncludePath(.{ .cwd_relative = b.pathJoin(&.{ ctx.sysroot.macos_sdk.?, "usr/include/c++/v1" }) });
    lib.includeInto(module);
    json.includeInto(module);
    archive.includeInto(module);
    module.addCSourceFiles(.{
        .root = source,
        .files = &.{ "src/component.cc", "src/catalog.cc", "src/prefs_store.cc", "src/installation.cc" },
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    module.addCSourceFiles(.{
        .root = source,
        .files = if (ctx.isWindows())
            &.{ "src/windows/prefs_store.cc", "src/windows/user_import_source.cc", "src/windows/user_import_transfer.cc" }
        else
            &.{ "src/macos/prefs_store.cc", "src/macos/launch.cc", "src/macos/browser_policy.cc" },
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    if (launcher) module.addCSourceFile(.{ .file = source.path(b, "src/c_api.cc"), .flags = ctx.cxxFlags("c++23", &.{}) });
    lib.artifact = ctx.staticLib(if (launcher) "launcher-profiles" else "stfc-profiles-core", module);
    return lib;
}

fn windowsArchive(ctx: *const Ctx, source: std.Build.LazyPath, zlib: Lib) ?Lib {
    const b = ctx.b;
    if (builtin.os.tag != .windows)
        std.debug.panic("-Dprofiles Windows builds require a Windows host with XMake 3.0.8 and Visual Studio", .{});
    if (ctx.target.result.cpu.arch != .x86_64)
        std.debug.panic("-Dprofiles Windows builds currently support x86_64-windows-msvc", .{});
    const recipes = (b.lazyDependency("profiles_xmake_recipes", .{}) orelse return null).path("");
    const archive_notices = (b.lazyDependency("profiles_libarchive_notices", .{}) orelse return null).path("");
    const xz_notices = (b.lazyDependency("profiles_xz_notices", .{}) orelse return null).path("");
    const script = b.addWriteFiles().add("profiles-archive.lua", archive_producer);
    const produce = b.addSystemCommand(&.{ "xmake", "lua" });
    produce.addFileArg(script);
    produce.addDirectoryArg(source);
    produce.addDirectoryArg(recipes);
    produce.addDirectoryArg(archive_notices);
    produce.addDirectoryArg(xz_notices);
    const stage = produce.addOutputDirectoryArg("profiles-archive");
    // XMake validates its own dependency cache; its host compiler/tool environment is external to Zig's cache.
    produce.has_side_effects = true;
    b.getInstallStep().dependOn(&b.addInstallDirectory(.{
        .source_dir = stage.path(b, "notices"),
        .install_dir = .prefix,
        .install_subdir = "share/stfc-profiles/notices/windows",
    }).step);
    b.getInstallStep().dependOn(&b.addInstallFile(
        b.dependency("zlib", .{}).path("LICENSE"),
        "share/stfc-profiles/notices/windows/zlib/LICENSE",
    ).step);
    return .{
        .include_dirs = ctx.dirs(&.{stage.path(b, "include")}),
        .static_files = ctx.dirs(&.{ stage.path(b, "lib/archive.lib"), stage.path(b, "lib/lzma.lib") }),
        .dependencies = ctx.libs(&.{zlib}),
        .defines = &.{ .{ "LIBARCHIVE_STATIC", "1" }, .{ "LZMA_API_STATIC", "1" } },
        .system_libs = &.{ "advapi32", "bcrypt", "ws2_32", "shlwapi", "user32", "crypt32" },
    };
}

// Only the small Windows archive producer uses XMake. Its project lives in the build cache, never in a
// fetched source tree. Recipe sources are immutable Zig packages; normal mod builds do not fetch either one.
const archive_producer =
    \\function main(source, recipes, archive_notices, xz_notices, stage)
    \\    -- Zig 0.17 supplies relative LazyPaths; XMake changes cwd for the generated project.
    \\    source, recipes = path.absolute(source), path.absolute(recipes)
    \\    archive_notices, xz_notices = path.absolute(archive_notices), path.absolute(xz_notices)
    \\    stage = path.absolute(stage)
    \\    os.mkdir(stage)
    \\    local notices = path.join(stage, "notices")
    \\    os.mkdir(path.join(notices, "libarchive"))
    \\    os.cp(path.join(archive_notices, "COPYING"), path.join(notices, "libarchive", "COPYING"))
    \\    local bsd_notices = ""
    \\    for _, name in ipairs({"archive_read_support_filter_compress.c", "archive_write_add_filter_compress.c"}) do
    \\        local header = assert(io.readfile(path.join(archive_notices, "libarchive", name)):match("^(.-)\n#include"))
    \\        assert(header:find("Regents", 1, true), "missing libarchive component copyright")
    \\        bsd_notices = bsd_notices .. name .. "\n" .. header .. "\n\n"
    \\    end
    \\    io.writefile(path.join(notices, "libarchive", "BSD-3-Clause-notices.txt"), bsd_notices)
    \\    os.mkdir(path.join(notices, "xz"))
    \\    for _, name in ipairs({"COPYING", "COPYING.0BSD", "COPYING.GPLv2", "COPYING.GPLv3", "COPYING.LGPLv2.1", "AUTHORS"}) do
    \\        os.cp(path.join(xz_notices, name), path.join(notices, "xz", name))
    \\    end
    \\    local project = [=[
    \\set_project("profiles-archive-dependencies")
    \\set_license("GPL-3.0")
    \\set_runtimes("MT")
    \\set_policy("package.precompiled", false)
    \\add_rules("mode.release")
    \\add_repositories("profiles-archive " .. path.join(@SOURCE@, "xmake-packages"))
    \\add_repositories("profiles-pinned " .. @RECIPES@)
    \\add_requires("libarchive 3.8.9", {system = false, configs = {shared = false, xz = true, zlib = true,
    \\    openssl3 = false, bzip2 = false, lz4 = false, lzo = false, zstd = false}})
    \\add_requires("xz 5.8.3", {system = false, configs = {shared = false}})
    \\target("profiles-archive-dependencies")
    \\    set_kind("phony")
    \\    add_packages("libarchive", "xz")
    \\    on_build(function(target)
    \\        local output = os.projectdir()
    \\        os.mkdir(path.join(output, "include"))
    \\        os.mkdir(path.join(output, "lib"))
    \\        for _, entry in ipairs({{"libarchive", "archive.lib"}, {"xz", "lzma.lib"}}) do
    \\            local pkg = target:pkg(entry[1])
    \\            assert(pkg, "missing archive package " .. entry[1])
    \\            local includes = table.join(table.wrap(pkg:get("includedirs")), table.wrap(pkg:get("sysincludedirs")))
    \\            for _, dir in ipairs(includes) do
    \\                os.cp(path.join(dir, "*"), path.join(output, "include"))
    \\            end
    \\            local copied = false
    \\            for _, dir in ipairs(table.wrap(pkg:get("linkdirs"))) do
    \\                local file = path.join(dir, entry[2])
    \\                if os.isfile(file) then os.cp(file, path.join(output, "lib", entry[2])); copied = true; break end
    \\            end
    \\            assert(copied, "missing static archive " .. entry[2])
    \\        end
    \\        assert(os.isfile(path.join(output, "include", "archive.h")), "missing libarchive public headers")
    \\    end)
    \\target_end()
    \\]=]
    \\    project = project:gsub("@SOURCE@", function() return string.format("%q", source) end)
    \\    project = project:gsub("@RECIPES@", function() return string.format("%q", recipes) end)
    \\    io.writefile(path.join(stage, "xmake.lua"), project)
    \\    os.execv(os.programfile(), {"f", "-c", "-P", stage, "-o", path.join(stage, "build"),
    \\        "-p", "windows", "-a", "x64", "-m", "release", "-y"})
    \\    os.execv(os.programfile(), {"build", "-P", stage, "-y", "profiles-archive-dependencies"})
    \\end
;
