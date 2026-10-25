//! Build graph for the mod, its native test fixtures, and the macOS launcher.
//!
//!   zig/ctx.zig         per-target compile rules (Ctx), library handles (Lib), shared options
//!   zig/compat.zig      std.Build differences between Zig 0.16 and 0.17
//!   zig/toolchains.zig  pinned SDKs: fetch-toolchains and sysroot lookup
//!   zig/deps.zig        third-party C and C++ libraries
//!   zig/protobuf.zig    protobuf-lite, Abseil, and generated game messages
//!   zig/mod.zig         IL2CPP and Unity libraries, the mod, platform entry points, macos-universal
//!   zig/launcher.zig    the Swift launcher app
//!   zig/tests.zig       native fixtures and the dependency smoke test
//!   zig/tools/          host programs the build runs
const std = @import("std");
const builtin = @import("builtin");
const compat = @import("zig/compat.zig");
const Ctx = @import("zig/ctx.zig").Ctx;
const Options = @import("zig/ctx.zig").Options;
const globFiles = @import("zig/ctx.zig").globFiles;
const deps = @import("zig/deps.zig");
const launcher = @import("zig/launcher.zig");
const mod = @import("zig/mod.zig");
const protobuf = @import("zig/protobuf.zig");
const tests = @import("zig/tests.zig");
const toolchains = @import("zig/toolchains.zig");
const aai = @import("zig/aai.zig");
const shared_toml = @import("zig/shared_toml.zig");
const profiles = @import("zig/profiles.zig");

comptime {
    const version = builtin.zig_version;
    if (version.major != 0 or version.minor < 16 or version.minor > 17)
        @compileError("This build supports Zig 0.16 and 0.17");
}

pub fn build(b: *std.Build) void {
    // Steps that do not depend on -Dtarget.
    toolchains.addFetchSteps(b);
    addFormatStep(b);

    const target = b.standardTargetOptions(.{
        .default_target = .{ .abi = if (builtin.os.tag == .windows) .msvc else null },
    });
    const optimize = b.standardOptimizeOption(.{});
    const options = Options.parse(b);
    const profiles_options = profiles.Options.parse(b);
    mod.addUniversal(b, optimize, options, profiles_options.host);
    const ctx = Ctx.init(b, target, optimize, options);

    // Libraries the native fixtures share with the shipped mod. None of them need networking or a platform SDK,
    // so Linux can build and run the fixtures.
    const spud = deps.spud(ctx, deps.zydis(ctx), deps.capstone(ctx));
    const il2cpp = mod.il2cpp(ctx, spud);
    const spdlog = deps.spdlog(ctx);
    const toml = deps.tomlplusplus(ctx);
    const toml_editor = shared_toml.editor(ctx, toml);
    const proto_runtime = protobuf.protobufLite(ctx, protobuf.abseil(ctx));
    // A null result means Zig still has to fetch a lazy dependency; it reruns build() once it has.
    const proto = protobuf.generate(ctx, proto_runtime) orelse return;
    tests.add(ctx, .{
        .il2cpp = il2cpp,
        .spdlog = spdlog,
        .spud = spud,
        .proto = proto,
        .proto_runtime = proto_runtime,
        .toml = toml,
        .toml_editor = toml_editor,
    });

    // Standalone producers stay outside the mod closure. Profiles enters that
    // closure only through its explicit host option; defaults inspect no private roots.
    const aai_options = aai.Options.parse(b);
    var unicode: ?@import("zig/ctx.zig").Lib = null;
    if (aai_options.enabled and aai_options.native) {
        const package = b.lazyDependency("simdutf", .{}) orelse return;
        unicode = aai.simdutf(ctx, package.path(""));
    }
    _ = aai.add(ctx, aai_options, .{
        .json = deps.nlohmannJson(ctx),
        .il2cpp = il2cpp,
        .spud = spud,
        .spdlog = spdlog,
        .simdutf = unicode,
    });
    _ = shared_toml.add(ctx, shared_toml.Options.parse(b), .{
        .toml = toml,
        .json = deps.nlohmannJson(ctx),
    });
    const profiles_outputs = profiles.add(ctx, profiles_options, .{
        .json = deps.nlohmannJson(ctx),
        .spud = spud,
        .spdlog = spdlog,
        .il2cpp = il2cpp,
    });
    if (!ctx.isWindows() and !ctx.isMacos()) return;

    // Everything below ships in, or tests, a platform build.
    const zlib = if (profiles_options.host) profiles_outputs.?.zlib else deps.zlib(ctx);
    const curl = deps.curl(ctx, zlib) orelse return;
    const cpr = deps.cpr(ctx, curl);
    const profiles_verification = if (profiles_options.host) profiles_outputs.?.verification else null;
    if (profiles_verification) |verify| {
        if (curl.artifact) |artifact| artifact.step.dependOn(verify);
        if (cpr.artifact) |artifact| artifact.step.dependOn(verify);
    }
    const unity = mod.unity(ctx, il2cpp, spud);
    var mod_libs: std.ArrayList(@import("zig/ctx.zig").Lib) = .empty;
    mod_libs.appendSlice(b.allocator, &.{ il2cpp, unity, spdlog, spud, proto, proto_runtime, cpr, curl, toml, toml_editor, deps.nlohmannJson(ctx) }) catch @panic("OOM");
    // The original optional adapter remains a separate producer/fixture. Only
    // the reviewed host adapter may enter this product's source/link closure.
    if (profiles_options.host) mod_libs.append(b.allocator, profiles_outputs.?.host.?) catch @panic("OOM");
    mod.add(ctx, mod_libs.items, profiles_verification);
    launcher.add(ctx);
    tests.addDependencySmoke(ctx, &.{ proto, proto_runtime, cpr, curl, zlib });

    // Build one dependency in isolation, to check a pin bump or a cross-compile without the mod.
    const net_step = cpr.addInstallStep(b, "dep-net", "Build the platform HTTPS dependencies");
    curl.addBuildDependencies(net_step);
    const proto_step = proto.addInstallStep(b, "dep-protobuf", "Build protobuf-lite and generated game messages");
    proto_runtime.addBuildDependencies(proto_step);
    _ = spud.addInstallStep(b, "dep-spud", "Build SPUD and its pinned dependencies");
    _ = spdlog.addInstallStep(b, "dep-spdlog", "Build only the spdlog dependency");
}

/// `fmt`: clang-format for C++ and zig fmt for the build. Objective-C++ (.mm) is left alone; the C++ rules do not
/// suit it.
fn addFormatStep(b: *std.Build) void {
    const step = b.step("fmt", "Format the project's C++ and Zig sources (needs clang-format on PATH)");
    const clang_format = b.addSystemCommand(&.{ "clang-format", "-i" });
    clang_format.has_side_effects = true;
    inline for (.{ "src", "tests" }) |dir|
        for (globFiles(b, b.path(""), dir, &.{ ".cc", ".h" }, &.{})) |file| clang_format.addFileArg(b.path(file));
    step.dependOn(&clang_format.step);
    step.dependOn(&compat.addFmt(b, &.{ "build.zig", "build.zig.zon", "zig" }).step);
}
