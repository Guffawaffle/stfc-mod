//! The Swift macOS launcher: its native rsync bridge, the Swift compile and link, and the app bundle.
//! Swift needs Xcode, so this only builds on a macOS host for a macOS target.
const std = @import("std");
const builtin = @import("builtin");
const compat = @import("compat.zig");
const Ctx = @import("ctx.zig").Ctx;
const macos_min = @import("ctx.zig").macos_min;
const globFiles = @import("ctx.zig").globFiles;
const hostTool = @import("ctx.zig").hostTool;
const deps = @import("deps.zig");

/// Register `launcher`, which installs zig-out/macOSLauncher.app.
pub fn add(ctx: *const Ctx, development_package: bool) void {
    const b = ctx.b;
    const step = b.step("launcher", "Build the Swift macOS launcher app (requires Xcode on macOS)");
    if (builtin.os.tag != .macos or !ctx.isMacos()) {
        step.dependOn(&b.addFail("The Swift launcher requires a macOS host and target with Xcode installed").step);
        return;
    }
    const sdk = ctx.sysroot.macos_sdk.?;
    const swift_target = b.fmt("{s}-apple-macosx{s}", .{ if (ctx.isAarch64()) "arm64" else "x86_64", macos_min });

    // The C++ side Swift imports through module.modulemap. swiftc links the system libc++, so the bridge builds
    // against the SDK's libc++ headers instead of Zig's bundled copy.
    const rsync = deps.rsync(ctx);
    const bridge_module = ctx.createModule(false);
    bridge_module.addIncludePath(.{ .cwd_relative = b.pathJoin(&.{ sdk, "usr/include/c++/v1" }) });
    rsync.includeInto(bridge_module);
    bridge_module.addCSourceFile(.{
        .file = b.path("src/launcher/macos/rsync.cc"),
        .flags = ctx.cxxFlags("c++23", &.{}),
    });
    const bridge = ctx.staticLib("launcher-bridge", bridge_module);

    // Compile all Swift sources as one module into one object. This needs only headers, so it runs while the
    // native archives build; only the link waits for them.
    const src = b.path("src/launcher/macos");
    const swift = b.addSystemCommand(&.{ "xcrun", "swiftc", "-parse-as-library" });
    swift.addArgs(&.{ "-module-name", "macOSLauncher" });
    swift.addArgs(&.{ "-sdk", sdk });
    swift.addArgs(&.{ "-target", swift_target });
    swift.addArgs(&.{ "-Xcc", "-fmodules", "-Xcc" });
    swift.addPrefixedFileArg("-fmodule-map-file=", src.path(b, "module.modulemap"));
    swift.addArgs(&.{ if (ctx.isDebug()) "-Onone" else "-O", "-g" });
    swift.addArgs(&.{ "-emit-object", "-whole-module-optimization" });
    for (globFiles(b, src, "", &.{".swift"}, &.{})) |file| swift.addFileArg(src.path(b, file));
    swift.addArg("-o");
    const object = swift.addOutputFileArg("macOSLauncher.o");

    const extractor = hostTool(b, "extract-macho-archive", "zig/tools/extract_macho_archive.zig", .Debug);
    const link = b.addSystemCommand(&.{ "xcrun", "swiftc", "-sdk", sdk, "-target", swift_target, "-g" });
    link.addFileArg(object);
    link.addFileArg(appleArchive(b, extractor, "liblauncher-bridge.a", bridge));
    link.addFileArg(appleArchive(b, extractor, "librsync.a", rsync.artifact.?));
    link.addArgs(&.{ "-lc++", "-o" });
    const exe = link.addOutputFileArg("macOSLauncher");
    step.dependOn(&b.addInstallFile(exe, "macOSLauncher.app/Contents/MacOS/macOSLauncher").step);

    installBundleResources(ctx, step, development_package);
}

/// Repack a Zig static library with Apple's libtool. Apple's linker rejects the archive index and unaligned
/// members Zig writes, so the archive is rebuilt from its loose objects.
fn appleArchive(
    b: *std.Build,
    extractor: *std.Build.Step.Compile,
    name: []const u8,
    library: *std.Build.Step.Compile,
) std.Build.LazyPath {
    const unpack = b.addRunArtifact(extractor);
    unpack.addArtifactArg(library);
    const objects = unpack.addOutputDirectoryArg("objects");
    const repack = b.addSystemCommand(&.{ "xcrun", "libtool", "-static", "-o" });
    const archive = repack.addOutputFileArg(name);
    repack.setCwd(objects);
    repack.addArgs(&.{ "-filelist", "objects.txt" });
    return archive;
}

/// Info.plist with the mod version, the app icon, and the compiled asset catalog.
fn installBundleResources(ctx: *const Ctx, step: *std.Build.Step, development_package: bool) void {
    const b = ctx.b;
    const Channel = enum { ci, dev, pre, release };
    const channel = b.option(Channel, "launcher_channel", "Visible launcher channel") orelse
        if (development_package) Channel.dev else Channel.ci;
    if ((channel == .dev and !development_package) or (channel == .ci and development_package))
        @panic("launcher CI/DEV channel must match the explicitly selected payload");
    const tag = b.option([]const u8, "launcher_release_tag", "Exact downstream release tag") orelse "";
    const source = b.option([]const u8, "launcher_source", "Launcher source commit") orelse "";
    const run_id = b.option([]const u8, "launcher_run_id", "Producing CI run ID") orelse "";
    const run_number = b.option([]const u8, "launcher_run_number", "Producing CI build number") orelse "0";
    const run_attempt = b.option([]const u8, "launcher_run_attempt", "Producing CI run attempt") orelse "1";

    // Player-visible identity is separate from the mod's verified source/provenance marker.
    // Core builds are CI; the selected development package is DEV. Trusted
    // promotion selects the final tag/channel before signing.
    const metadata = b.addSystemCommand(&.{"python3"});
    metadata.addFileArg(b.path("scripts/mac-build-identity.py"));
    metadata.addArgs(&.{ "generate", "--template" });
    metadata.addFileArg(b.path("src/launcher/macos/Info.plist.template"));
    metadata.addArgs(&.{
        "--base-version", modVersion(b),     "--macos-min",     macos_min,
        "--channel",      @tagName(channel), "--run-number",    run_number,
        "--run-attempt",  run_attempt,       "--fork-revision", forkRevision(b),
    });
    if (tag.len > 0) metadata.addArgs(&.{ "--release-tag", tag });
    if (source.len > 0) metadata.addArgs(&.{ "--source", source });
    if (run_id.len > 0) metadata.addArgs(&.{ "--run-id", run_id });
    metadata.addArg("--output");
    const info = metadata.addOutputFileArg("Info.plist");
    step.dependOn(&b.addInstallFile(info, "macOSLauncher.app/Contents/Info.plist").step);
    const icon = b.addInstallFile(
        b.path(b.fmt("assets/launcher-icons/launcher-{s}.icns", .{@tagName(channel)})),
        "macOSLauncher.app/Contents/Resources/launcher.icns",
    );
    step.dependOn(&icon.step);

    const actool = b.addSystemCommand(&.{ "xcrun", "actool", "--platform", "macosx" });
    actool.addArgs(&.{ "--minimum-deployment-target", macos_min, "--compile" });
    const compiled = actool.addOutputDirectoryArg("resources");
    actool.addDirectoryArg(b.path("src/launcher/macos/Assets.xcassets"));
    step.dependOn(&b.addInstallDirectory(.{
        .source_dir = compiled,
        .install_dir = .prefix,
        .install_subdir = "macOSLauncher.app/Contents/Resources",
    }).step);
}

fn forkRevision(b: *std.Build) []const u8 {
    const header = compat.readFile(b, b.path("src/mod/version.h"), .limited(64 * 1024));
    const key = "VERSION_FORK_REVISION";
    const offset = std.mem.indexOf(u8, header, key) orelse @panic("missing fork revision in src/mod/version.h");
    const rest = std.mem.trimStart(u8, header[offset + key.len ..], " \t");
    const value = rest[0 .. std.mem.indexOfAny(u8, rest, " \t\r\n") orelse rest.len];
    const revision = std.fmt.parseInt(u63, value, 10) catch @panic("invalid fork revision in src/mod/version.h");
    if (revision == 0) @panic("fork revision must be positive");
    return value;
}

/// MAJOR.MINOR.REVISION.PATCH from the `#define`s in src/mod/version.h.
fn modVersion(b: *std.Build) []const u8 {
    const header = compat.readFile(b, b.path("src/mod/version.h"), .limited(64 * 1024));
    var parts: [4][]const u8 = undefined;
    inline for (.{ "VERSION_MAJOR", "VERSION_MINOR", "VERSION_REVISION", "VERSION_PATCH" }, 0..) |key, i| {
        const offset = std.mem.indexOf(u8, header, key) orelse @panic("missing " ++ key ++ " in src/mod/version.h");
        const rest = std.mem.trimStart(u8, header[offset + key.len ..], " \t");
        parts[i] = rest[0 .. std.mem.indexOfAny(u8, rest, " \t\r\n") orelse rest.len];
    }
    return b.fmt("{s}.{s}.{s}.{s}", .{ parts[0], parts[1], parts[2], parts[3] });
}
