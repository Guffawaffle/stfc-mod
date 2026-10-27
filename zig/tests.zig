//! Native test fixtures. They link the same libraries as the shipped mod, and run without the game.
const std = @import("std");
const builtin = @import("builtin");
const compat = @import("compat.zig");
const Ctx = @import("ctx.zig").Ctx;
const Lib = @import("ctx.zig").Lib;
const deps = @import("deps.zig");

/// The libraries the fixtures link. They are the production artifacts, built at -Doptimize.
pub const Libs = struct {
    il2cpp: Lib,
    spdlog: Lib,
    spud: Lib,
    proto: Lib,
    proto_runtime: Lib,
    toml: Lib,
    toml_editor: Lib,
    cpr: ?Lib = null,
    curl: ?Lib = null,
};

pub const Steps = struct {
    build_tests: *std.Build.Step,
    test_sync: *std.Build.Step,
    test_sync_upload: *std.Build.Step,
};

/// Register the fixture steps. Fixture code compiles at -Dtest-optimize, so a Debug fixture keeps its assertions
/// while sharing release libraries with the shipped build.
pub fn add(build_ctx: *const Ctx, libs: Libs) Steps {
    const ctx = build_ctx.withOptimize(build_ctx.options.test_optimize orelse build_ctx.optimize);
    const b = ctx.b;
    const build_tests = b.step("build-tests", "Compile the tests without running them");
    const test_il2cpp = b.step("test-il2cpp", "Build and run the IL2CPP boundary tests (native targets only)");
    const test_mod = b.step("test-mod", "Build and run the settings and shortcut tests (native targets only)");
    const test_settings_models = b.step("test-settings-models", "Run the preserved downstream play settings and owned callback fixtures");
    const test_sync = b.step("test-sync", "Run sync record and delivery regression tests without the game");
    const test_sync_upload = b.step("test-sync-upload", "Run the production uploader against a synthetic loopback listener");
    if (!ctx.isWindows() and !ctx.isMacos())
        test_sync_upload.dependOn(&b.addFail("The production CPR uploader fixture requires the Windows/macOS product dependency closure; pure sync fixtures remain portable").step);
    const test_launcher = b.step("test-launcher", "Run launcher delta and archive tests (Linux/macOS targets)");
    const test_spud = b.step("test-spud", "Run the x64 trampoline relocation test (native x86_64 targets only)");
    const flags = ctx.cxxFlags("c++23", &.{});
    const mod_flags = ctx.cxxFlags("c++23", &.{ "-Wno-deprecated-literal-operator", "-Wno-invalid-offsetof", "-Wno-deprecated-declarations" });

    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path("tests/warp_click_intent_test.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        addFixture(ctx, build_tests, test_mod, "warp-click-intent-tests", module);
    }

    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path("tests/audio_coalescing_test.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        addFixture(ctx, build_tests, test_mod, "audio-coalescing-tests", module);
    }
    {
        const module = settingsModule(ctx, libs);
        module.addCMacro("NOTIFICATION_AUDIO_TEST", "1");
        module.addCSourceFiles(.{
            .files = &.{ "tests/audio_dispatch_test.cc", "src/mod/features/notification_audio.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        addFixture(ctx, build_tests, test_mod, "audio-dispatch-tests", module);
    }
    if (ctx.isWindows() or ctx.isMacos()) {
        const module = settingsModule(ctx, libs);
        module.addCSourceFiles(.{
            .files = &.{ "tests/notification_audio_files_test.cc", "src/mod/features/notification_audio.cc", "src/mod/features/notification_audio_files.cc", "src/mod/notification_audio_windows.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        if (ctx.isWindows()) inline for (.{ "winmm", "mfuuid", "shlwapi", "ole32" }) |lib| module.linkSystemLibrary(lib, .{});
        if (ctx.isMacos()) {
            module.addCSourceFile(.{ .file = b.path("src/mod/features/notification_audio_mac.mm"), .flags = flags });
            module.linkFramework("Cocoa", .{});
        }
        addFixture(ctx, build_tests, test_mod, "notification-audio-files-tests", module);
    }

    inline for (.{ "keyboard_layout_diagnostics", "officer_preset_handoff", "discovery_tow_policy", "native_text_entry" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/{s}_test.cc", .{fixture})), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        addFixture(ctx, build_tests, test_mod, b.fmt("{s}-tests", .{fixture}), module);
    }

    inline for (.{ "image_file_setting", "image_path_validator", "file_picker_task" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/{s}_test.cc", .{fixture})), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable(b.fmt("{s}-tests", .{fixture}), module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        if (comptime std.mem.eql(u8, fixture, "image_file_setting"))
            run.addDirectoryArg(b.path(".zig-cache/image-file-setting-fixture"));
        test_settings_models.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }

    inline for (.{ "keyboard_layout", "keyboard_chord" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addIncludePath(b.path("src/libs/unity/include"));
        module.addIncludePath(b.path("src/libs/il2cpp/include"));
        module.addIncludePath(b.path("third_party/libil2cpp"));
        libs.il2cpp.includeInto(module);
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/{s}_tests.cc", .{fixture})), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        if (ctx.isWindows()) module.linkSystemLibrary("user32", .{});
        const exe = ctx.executable(b.fmt("{s}-tests", .{fixture}), module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        test_mod.dependOn(&b.addRunArtifact(exe).step);
    }
    {
        const module = settingsModule(ctx, libs);
        module.addCSourceFiles(.{
            .files = &.{ "tests/keyboard_shortcut_test.cc", "tests/keyboard_layout_no_host.cc", "src/mod/shortcuts.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        addFixture(ctx, build_tests, test_mod, "keyboard-shortcut-tests", module);
    }
    {
        const module = settingsModule(ctx, libs);
        module.addCSourceFiles(.{
            .files = &.{ "tests/shortcut_capture_guard_test.cc", "tests/keyboard_layout_no_host.cc", "src/mod/shortcuts.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        addFixture(ctx, build_tests, test_mod, "shortcut-capture-guard-tests", module);
    }

    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path("tests/fleet_opc_sample_test.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable("fleet-opc-sample-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        test_mod.dependOn(&b.addRunArtifact(exe).step);
    }

    inline for (.{ "boolean_settings", "choice_setting", "slider_setting", "settings_maintenance", "settings_search", "settings_metadata", "page_catalog", "native_boolean_callback", "settings_row_quarantine", "settings_row_insertion", "settings_page_publication", "shortcut_editor" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addIncludePath(b.path("third_party/libil2cpp"));
        if (comptime std.mem.eql(u8, fixture, "settings_maintenance"))
            module.addCMacro("STFC_SETTINGS_TEXT_ENTRY_FIXTURE", "1");
        if (comptime std.mem.eql(u8, fixture, "shortcut_editor")) {
            module.addIncludePath(b.path("src/libs/unity/include"));
            libs.il2cpp.includeInto(module);
            libs.toml.includeInto(module);
        }
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/{s}_test.cc", .{fixture})), .flags = if (comptime std.mem.eql(u8, fixture, "shortcut_editor")) ctx.cxxFlags("c++23", &.{ "-Wno-invalid-offsetof", "-UNDEBUG" }) else ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable(b.fmt("{s}-tests", .{fixture}), module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_settings_models.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }

    const test_toml_editor = b.step("test-toml-editor", "Run the pinned preparation engine and preserving editor fixtures");
    const test_runtime_config = b.step("test-runtime-config", "Run pure runtime-setting writer and host lifecycle fixtures");
    const test_runtime_reload = b.step("test-runtime-reload", "Run pure pending-setting reload overlay fixtures");
    const test_mod_state = b.step("test-mod-state", "Run isolated mod-state transaction and writer fixtures");
    const test_basic_gameplay = b.step("test-basic-gameplay", "Run actual gameplay mutation helpers through the preserving file worker");
    const test_gameplay_presentation = b.step("test-gameplay-presentation", "Run pure gameplay presentation and lifecycle policies");
    const test_officer_preset_order = b.step("test-officer-preset-order", "Run canonical and presentation officer preset order policies");
    const test_hud_visibility = b.step("test-hud-visibility", "Run HUD baseline restoration and native refresh sequence policies");
    const test_toast_parameters = b.step("test-toast-parameters", "Run toast parameter source selection and formatting fixtures");
    const test_haven_visuals = b.step("test-haven-visuals", "Run Haven shared discovery, ownership and restoration fixtures");
    const test_planned_system_warp = b.step("test-planned-system-warp", "Run retained ordinary system warp qualification and action routing fixtures");
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path("tests/haven_visuals_test.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable("haven-visuals-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_haven_visuals.dependOn(&run.step);
        test_gameplay_presentation.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }
    inline for (.{ "officer_preset_order", "hud_visibility", "toast_parameters", "planned_system_warp", "galaxy_policy", "galaxy_selection_pins", "galaxy_selection_policy" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addIncludePath(b.path("third_party/libil2cpp"));
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/{s}_test.cc", .{fixture})), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable(b.fmt("{s}-tests", .{fixture}), module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_gameplay_presentation.dependOn(&run.step);
        test_mod.dependOn(&run.step);
        if (comptime std.mem.eql(u8, fixture, "officer_preset_order")) test_officer_preset_order.dependOn(&run.step);
        if (comptime std.mem.eql(u8, fixture, "hud_visibility")) test_hud_visibility.dependOn(&run.step);
        if (comptime std.mem.eql(u8, fixture, "toast_parameters")) test_toast_parameters.dependOn(&run.step);
        if (comptime std.mem.eql(u8, fixture, "planned_system_warp")) test_planned_system_warp.dependOn(&run.step);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        deps.nlohmannJson(ctx).linkInto(module);
        module.addCSourceFile(.{ .file = b.path("tests/galaxy_housing_data_test.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        addFixture(ctx, build_tests, test_mod, "galaxy-housing-data-tests", module);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        libs.toml_editor.linkInto(module);
        module.addCSourceFiles(.{
            .files = &.{ "tests/basic_gameplay_settings_test.cc", "src/mod/runtime_config_writer.cc", "src/mod/toml_editor.cc", "src/mod/config_save.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        const exe = ctx.executable("basic-gameplay-settings-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_basic_gameplay.dependOn(&run.step);
        test_runtime_config.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }
    const test_ship_pins = b.step("test-ship-pins", "Run stable ship identity/order, pointer policy and receipt fixtures");
    const test_checked_ship_order = b.step("test-checked-ship-order", "Run checked native-order reconciliation policy fixtures");
    inline for (.{ "pinned_ship_order", "pin_receipt_flow", "pin_pointer_policy", "checked_ship_order" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        if (comptime std.mem.eql(u8, fixture, "pinned_ship_order") or std.mem.eql(u8, fixture, "pin_receipt_flow"))
            deps.nlohmannJson(ctx).linkInto(module);
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/{s}_test.cc", .{fixture})), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable(b.fmt("{s}-tests", .{fixture}), module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_ship_pins.dependOn(&run.step);
        test_mod.dependOn(&run.step);
        if (comptime std.mem.eql(u8, fixture, "checked_ship_order")) test_checked_ship_order.dependOn(&run.step);
    }
    {
        // The unchanged full fixture combines engine and C ABI checks. Its C ABI
        // implementation/export macro is test-only, never a product dependency.
        const module = ctx.createModule(true);
        libs.toml_editor.linkInto(module);
        deps.nlohmannJson(ctx).linkInto(module);
        module.addIncludePath(b.path("third_party/stfc_toml/test_support/include"));
        module.addCMacro("STFC_TOML_BUILD", "1");
        module.addCSourceFiles(.{
            .files = &.{ "third_party/stfc_toml/test_support/editor_test.cc", "third_party/stfc_toml/test_support/native.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        const exe = ctx.executable("shared-toml-core-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_toml_editor.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        libs.toml_editor.linkInto(module);
        module.addCSourceFiles(.{
            .files = &.{ "tests/toml_editor_test.cc", "src/mod/toml_editor.cc", "src/mod/config_save.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        const exe = ctx.executable("toml-editor-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_toml_editor.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        libs.toml_editor.linkInto(module);
        // This fixture includes the writer TU under compile-time save/thread
        // seams. Do not link another writer or editor implementation into it.
        module.addCSourceFile(.{ .file = b.path("tests/runtime_config_writer_test.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable("runtime-config-writer-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_runtime_config.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addIncludePath(b.path("tests"));
        libs.toml_editor.linkInto(module);
        // Includes the production adapter with fake hook/worker boundaries.
        // No native game binding, real settings path or worker owner is linked.
        module.addCSourceFile(.{ .file = b.path("tests/runtime_config_test.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable("runtime-config-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_runtime_config.dependOn(&run.step);
        test_mod.dependOn(&run.step);
        if (ctx.isWindows()) {
            // The strict harness spawns only this disposable fixture executable;
            // its four force-close cases must self-exit within bounded deadlines.
            const force_close = b.addSystemCommand(&.{ "python", "-B" });
            force_close.addFileArg(b.path("scripts/test-runtime-force-close.py"));
            force_close.addArtifactArg(exe);
            force_close.has_side_effects = true;
            test_runtime_config.dependOn(&force_close.step);
            test_mod.dependOn(&force_close.step);
        }
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        libs.toml.linkInto(module);
        module.addCSourceFile(.{ .file = b.path("tests/runtime_reload_test.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable("runtime-reload-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_runtime_reload.dependOn(&run.step);
        test_runtime_config.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }
    inline for (.{ "transaction", "writer" }) |kind| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        libs.spdlog.linkInto(module);
        deps.nlohmannJson(ctx).linkInto(module);
        // Transaction includes its real owner TU with a private fixture path;
        // writer is header-only. Neither target links a second state owner.
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/mod_state_{s}_test.cc", .{kind})), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable(b.fmt("mod-state-{s}-tests", .{kind}), module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_mod_state.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }

    // Checked startup document replacement, with test-only failure controls in
    // the failure TU and exclusively owned temporary filesystem state.
    const test_config_save = b.step("test-config-save", "Run isolated checked configuration-output fixtures");
    inline for (.{ false, true }) |failure| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        libs.toml.linkInto(module);
        const save_flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"});
        module.addCSourceFile(.{
            .file = b.path(if (failure) "tests/config_save_failure_test.cc" else "tests/config_save_test.cc"),
            .flags = save_flags,
        });
        // The failure fixture includes the production TU under compile-time
        // write/close substitutions; do not compile a duplicate owner there.
        if (!failure) module.addCSourceFile(.{ .file = b.path("src/mod/config_save.cc"), .flags = save_flags });
        const exe = ctx.executable(if (failure) "config-save-failure-tests" else "config-save-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_config_save.dependOn(&run.step);
        test_mod.dependOn(&run.step);
    }

    inline for (.{ "profile_bootstrap", "profile_hook_targets" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addIncludePath(b.path("src/profiles"));
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/{s}.cc", .{fixture})), .flags = flags });
        addFixture(ctx, build_tests, test_mod, b.fmt("{s}-tests", .{fixture}), module);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        libs.spdlog.linkInto(module);
        module.addCMacro("STFC_PROFILES_HOST", "1");
        module.addCSourceFiles(.{ .files = &.{ "tests/profile_paths.cc", "src/mod/file.cc" }, .flags = flags });
        const exe = ctx.executable("profile-paths-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        inline for (.{ "ordinary", "custom-relative", "custom-absolute", "custom-directory", "custom-directory-absolute", "named", "unprepared" }) |mode| {
            const run = b.addRunArtifact(exe);
            run.addArg(mode);
            test_mod.dependOn(&run.step);
        }
        if (ctx.isMacos()) {
            const run = b.addRunArtifact(exe);
            run.addArg("mac-legacy-migration");
            test_mod.dependOn(&run.step);
        }
    }

    // HookSet policy over SPUD's public generic installable-hook seam. Unsafe modes each need a fresh process:
    // the production native-state latch is monotonic and intentionally has no test reset API.
    {
        const module = ctx.createModule(true);
        libs.il2cpp.linkInto(module);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path("tests/hook_installation.cc"), .flags = flags });
        const exe = ctx.executable("hook-installation-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        test_il2cpp.dependOn(&b.addRunArtifact(exe).step);
        inline for (.{ "rollback-failed", "install-throws", "mutation-then-error", "protection-failed", "detach-throws", "suspension-unavailable", "unknown-error" }) |mode| {
            const run = b.addRunArtifact(exe);
            run.addArg(mode);
            test_il2cpp.dependOn(&run.step);
        }
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path("tests/frame_callbacks.cc"), .flags = flags });
        addFixture(ctx, build_tests, test_mod, "frame-callbacks-tests", module);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        libs.il2cpp.includeInto(module);
        module.addCSourceFile(.{ .file = b.path("tests/installation.cc"), .flags = flags });
        addFixture(ctx, build_tests, test_mod, "installation-tests", module);
    }

    // The action queue decisions in src/mod, header-only.
    inline for (.{ "fleet_alert_arrival", "fleet_alert_opc", "incoming_attack_dedup" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/{s}.cc", .{fixture})), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        addFixture(ctx, build_tests, test_mod, b.fmt("{s}-tests", .{fixture}), module);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path("tests/action_queue.cc"), .flags = flags });
        addFixture(ctx, build_tests, test_mod, "action-queue-tests", module);
    }

    // IL2CPP bindings and GC handles against a fake runtime API defined in each test file.
    {
        const module = ctx.createModule(true);
        libs.il2cpp.linkInto(module);
        // The fake API must precede consumers so the linker does not pull in production API storage.
        module.addCSourceFile(.{ .file = b.path("tests/il2cpp_bindings.cc"), .flags = flags });
        module.addIncludePath(b.path("src/mod"));
        module.addIncludePath(b.path("src/libs/unity/include"));
        module.addCSourceFile(.{ .file = b.path("src/libs/unity/src/input.cc"), .flags = flags });
        const exe = ctx.executable("il2cpp-bindings-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        test_il2cpp.dependOn(&b.addRunArtifact(exe).step);
        // Metadata resolution is process-cached. Missing required array-class
        // metadata must be observed before any successful lookup in this process.
        inline for (.{ "array-class-missing", "string-class-missing" }) |mode| {
            const missing_class = b.addRunArtifact(exe);
            missing_class.addArg(mode);
            test_il2cpp.dependOn(&missing_class.step);
        }
        // Avatar field/target qualification is also process-cached. Exercise
        // absent native retention support and each malformed schema separately.
        inline for (.{
            "chat-avatar-native",                 "chat-avatar-reject-profile-type",
            "chat-avatar-reject-avatar-type",     "chat-avatar-reject-admin-type",
            "chat-avatar-reject-keep-type",       "chat-avatar-reject-static",
            "chat-avatar-reject-byref",           "chat-avatar-reject-header-offset",
            "chat-avatar-reject-negative-offset", "chat-avatar-reject-overflow-offset",
            "chat-avatar-reject-end-offset",      "chat-avatar-reject-undersized-owner",
            "chat-avatar-method-static",          "chat-avatar-method-generic",
            "chat-avatar-method-inflated",        "chat-avatar-method-return",
            "chat-avatar-method-parameters",      "chat-avatar-method-inherited",
            "chat-avatar-method-null-code",       "chat-avatar-method-missing",
        }) |mode| {
            const avatar_case = b.addRunArtifact(exe);
            avatar_case.addArg(mode);
            test_il2cpp.dependOn(&avatar_case.step);
        }
        // The movement binding also caches its qualified signature. Each
        // incompatible layout must be presented before its first resolution.
        inline for (.{
            "warp-reject-static",              "warp-reject-virtual",             "warp-reject-missing",
            "warp-reject-byref-void",          "warp-reject-payload",             "warp-reject-nullable-class",
            "warp-reject-vector-class",        "warp-reject-u4-result",           "warp-reject-u8-course",
            "warp-reject-u8-move",             "warp-reject-u8-validator",        "warp-reject-u4-depth",
            "warp-reject-null-move-code",      "warp-reject-null-invoker",        "warp-reject-null-getter-code",
            "warp-reject-null-course-code",    "warp-reject-null-validator-code", "warp-reject-null-vector-invoker",
            "warp-reject-null-shared-invoker", "warp-reject-null-reference-code",
        }) |mode| {
            const invalid_warp = b.addRunArtifact(exe);
            invalid_warp.addArg(mode);
            test_il2cpp.dependOn(&invalid_warp.step);
        }
    }
    {
        const module = ctx.createModule(true);
        libs.il2cpp.linkInto(module);
        module.addCSourceFile(.{ .file = b.path("tests/il2cpp_gc.cc"), .flags = flags });
        addFixture(ctx, build_tests, test_il2cpp, "il2cpp-gc-tests", module);
    }
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/libs/unity/include"));
        libs.il2cpp.linkInto(module);
        libs.spud.linkInto(module);
        module.addCSourceFile(.{ .file = b.path("tests/unity_tracker.cc"), .flags = flags });
        addFixture(ctx, build_tests, test_il2cpp, "unity-tracker-tests", module);
    }

    // Settings, shortcuts and reload. The fixture paths are defined on mod_logic.cc alone, so the shared settings
    // objects compile with identical flags and come from Clang's cache for the second fixture.
    {
        const module = settingsModule(ctx, libs);
        module.addCSourceFile(.{
            .file = b.path("tests/mod_logic.cc"),
            .flags = ctx.cxxFlags("c++23", &.{
                "-Wno-deprecated-literal-operator",
                b.fmt("-DSTFC_SHORTCUT_DEFAULTS={s}", .{quotedPath(b, "tests/shortcut_defaults.toml")}),
                b.fmt("-DSTFC_EXAMPLE_SETTINGS={s}", .{
                    quotedPath(b, "docs/configuration/example_community_patch_settings_en.toml"),
                }),
            }),
        });
        module.addCSourceFiles(.{
            .files = &.{
                "src/mod/shortcuts.cc",
                "src/mod/config.cc",
                "src/mod/config_save.cc",
                "src/mod/file.cc",
                "tests/runtime_config_no_host.cc",
                "tests/keyboard_layout_no_host.cc",
                "src/mod/arguments.cc",
                "src/mod/features/notification_audio.cc",
                "src/mod/features/notification_audio_files.cc",
                "src/mod/notification_audio_windows.cc",
            },
            .flags = mod_flags,
        });
        if (ctx.isWindows()) {
            // file.cc reads the command line through CommandLineToArgvW.
            module.linkSystemLibrary("shell32", .{});
            inline for (.{ "winmm", "mfuuid", "shlwapi", "ole32" }) |lib| module.linkSystemLibrary(lib, .{});
        }
        if (ctx.isMacos()) {
            module.addCSourceFiles(.{
                .files = &.{ "src/mod/file_macos.mm", "src/mod/features/notification_audio_mac.mm" },
                .flags = flags,
            });
            module.linkFramework("Cocoa", .{});
        }
        addFixture(ctx, build_tests, test_mod, "mod-logic-tests", module);
    }
    {
        const module = settingsModule(ctx, libs);
        module.addCSourceFiles(.{
            .files = &.{
                "tests/config_reload.cc",
                "src/mod/shortcuts.cc",
                "src/mod/config.cc",
                "src/mod/config_save.cc",
                "src/mod/features/notification_audio.cc",
                "src/mod/features/notification_audio_files.cc",
                "src/mod/notification_audio_windows.cc",
                "tests/runtime_config_no_host.cc",
                "tests/keyboard_layout_no_host.cc",
            },
            .flags = ctx.cxxFlags("c++23", &.{ "-Wno-deprecated-literal-operator", "-Wno-invalid-offsetof", "-Wno-deprecated-declarations", "-UNDEBUG" }),
        });
        if (ctx.isWindows()) inline for (.{ "winmm", "mfuuid", "shlwapi", "ole32" }) |lib| module.linkSystemLibrary(lib, .{});
        if (ctx.isMacos()) {
            module.addCSourceFile(.{ .file = b.path("src/mod/features/notification_audio_mac.mm"), .flags = flags });
            module.linkFramework("Cocoa", .{});
        }
        const exe = ctx.executable("config-reload-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        test_mod.dependOn(&b.addRunArtifact(exe).step);
        const startup_failure = b.addRunArtifact(exe);
        startup_failure.addArg("startup-output-failure");
        test_mod.dependOn(&startup_failure.step);
        test_config_save.dependOn(&startup_failure.step);
        inline for (.{ "startup-invalid-primary", "startup-missing-primary" }) |mode| {
            const primary = b.addRunArtifact(exe);
            primary.addArg(mode);
            test_mod.dependOn(&primary.step);
            test_config_save.dependOn(&primary.step);
        }
    }

    {
        const module = settingsModule(ctx, libs);
        // The catalog's lazy picker admission uses the shared frame declarations.
        // This remains header-only: synthetic owners supply those functions.
        libs.spud.includeInto(module);
        module.addCSourceFiles(.{
            .files = &.{
                "tests/config_settings_test.cc",
                "src/mod/settings/config_settings.cc",
                "src/mod/settings/audio_alerts.cc",
                "src/mod/settings/mod_pages.cc",
                "src/mod/settings/shortcut_settings.cc",
                "src/mod/settings/maintenance_pages.cc",
                "src/mod/shortcuts.cc",
                "src/mod/config.cc",
                "src/mod/config_save.cc",
                "tests/keyboard_layout_no_host.cc",
                "src/mod/features/notification_audio.cc",
                "src/mod/features/notification_audio_files.cc",
                "src/mod/notification_audio_windows.cc",
            },
            .flags = ctx.cxxFlags("c++23", &.{ "-Wno-deprecated-literal-operator", "-Wno-invalid-offsetof", "-Wno-deprecated-declarations", "-UNDEBUG" }),
        });
        if (ctx.isWindows()) inline for (.{ "winmm", "mfuuid", "shlwapi", "ole32" }) |lib| module.linkSystemLibrary(lib, .{});
        if (ctx.isMacos()) {
            module.addCSourceFile(.{ .file = b.path("src/mod/features/notification_audio_mac.mm"), .flags = flags });
            module.linkFramework("Cocoa", .{});
        }
        const exe = ctx.executable("config-settings-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_mod.dependOn(&run.step);
        test_settings_models.dependOn(&run.step);
    }

    {
        const module = settingsModule(ctx, libs);
        libs.spud.includeInto(module);
        module.addCSourceFiles(.{
            .files = &.{ "tests/audio_settings_test.cc", "src/mod/settings/audio_alerts.cc" },
            .flags = ctx.cxxFlags("c++23", &.{ "-Wno-invalid-offsetof", "-Wno-deprecated-literal-operator", "-UNDEBUG" }),
        });
        const exe = ctx.executable("audio-settings-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_mod.dependOn(&run.step);
        test_settings_models.dependOn(&run.step);
    }

    {
        const module = settingsModule(ctx, libs);
        module.addCMacro("STFC_SETTINGS_TEXT_ENTRY_FIXTURE", "1");
        module.addCSourceFiles(.{
            .files = &.{ "tests/maintenance_pages_test.cc", "src/mod/settings/maintenance_pages.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        const exe = ctx.executable("maintenance-pages-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_mod.dependOn(&run.step);
        test_settings_models.dependOn(&run.step);
    }

    const test_sync_input = b.step("test-sync-input", "Run pure reservation queue and processor-health fixtures");
    const test_sync_diagnostic = b.step("test-sync-diagnostic", "Run pure diagnostic envelope and admission fixtures");
    {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        deps.nlohmannJson(ctx).linkInto(module);
        module.addCSourceFile(.{ .file = b.path("tests/sync_diagnostic.cc"), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable("sync-diagnostic-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_sync_diagnostic.dependOn(&run.step);
        test_sync.dependOn(&run.step);
    }
    inline for (.{ "queue", "health" }) |fixture| {
        const module = ctx.createModule(true);
        module.addIncludePath(b.path("src/mod"));
        module.addCSourceFile(.{ .file = b.path(b.fmt("tests/sync_input_{s}.cc", .{fixture})), .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}) });
        const exe = ctx.executable(b.fmt("sync-input-{s}-tests", .{fixture}), module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        test_sync_input.dependOn(&run.step);
        test_sync.dependOn(&run.step);
    }

    {
        const module = syncModule(ctx, libs);
        module.addCSourceFiles(.{
            .files = &.{ "tests/sync_diagnostic_state.cc", "src/mod/sync/diagnostics.cc", "src/mod/sync/state.cc" },
            .flags = ctx.cxxFlags("c++23", &.{"-UNDEBUG"}),
        });
        const exe = ctx.executable("sync-diagnostic-state-tests", module);
        build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
        const run = b.addRunArtifact(exe);
        b.step("test-sync-diagnostic-state", "Run the actual diagnostic publisher and session/target state fixtures").dependOn(&run.step);
        test_sync.dependOn(&run.step);
    }

    // Sync records and delivery state over the generated protobuf messages.
    {
        const module = syncModule(ctx, libs);
        module.addCSourceFiles(.{
            .files = &.{ "tests/sync_logic.cc", "src/mod/sync/records.cc", "src/mod/sync/state.cc" },
            .flags = mod_flags,
        });
        addFixture(ctx, build_tests, test_sync, "sync-logic-tests", module);
    }

    // Launcher deltas in C++, plus the Swift archive code when the host can run swiftc.
    if (ctx.isLinux() or ctx.isMacos()) {
        const module = ctx.createModule(true);
        deps.rsync(ctx).linkInto(module);
        module.addCSourceFiles(.{
            .files = &.{ "tests/launcher_rsync.cc", "src/launcher/macos/rsync.cc" },
            .flags = flags,
        });
        addFixture(ctx, build_tests, test_launcher, "launcher-rsync-tests", module);
        if (builtin.os.tag == .macos and ctx.isMacos()) {
            const compile = b.addSystemCommand(&.{ "xcrun", "swiftc", "-parse-as-library", "-o" });
            const exe = compile.addOutputFileArg("launcher-archive-tests");
            compile.addFileArg(b.path("src/launcher/macos/UpdaterFileOperations.swift"));
            compile.addFileArg(b.path("tests/launcher_archive.swift"));
            const run = std.Build.Step.Run.create(b, "launcher-archive-tests");
            run.addFileArg(exe);
            run.addDirectoryArg(b.path("tests/fixtures/launcher"));
            test_launcher.dependOn(&run.step);
        }
    } else {
        test_launcher.dependOn(&b.addFail("The launcher rsync tests require a Linux or macOS target").step);
    }

    // A prologue where two RIP-relative instructions expand ahead of a relative call, detoured and run. ARM64
    // relocates differently and has nothing to check here.
    if (ctx.target.result.cpu.arch == .x86_64) {
        const module = ctx.createModule(true);
        libs.spud.linkInto(module);
        module.addCSourceFile(.{ .file = b.path("tests/spud_relocation.cc"), .flags = flags });
        addFixture(ctx, build_tests, test_spud, "spud-relocation-tests", module);
    } else {
        test_spud.dependOn(&b.addFail("The SPUD relocation test requires an x86_64 target").step);
    }
    return .{ .build_tests = build_tests, .test_sync = test_sync, .test_sync_upload = test_sync_upload };
}

/// Attach the platform transport fixture only after the actual product CPR
/// library exists. Linux never constructs a second platform/network stack.
pub fn addSyncUpload(build_ctx: *const Ctx, libs: Libs, steps: Steps) void {
    const ctx = build_ctx.withOptimize(build_ctx.options.test_optimize orelse build_ctx.optimize);
    const b = ctx.b;
    const module = syncModule(ctx, libs);
    (libs.cpr orelse @panic("The uploader fixture must reuse the existing product CPR library")).linkInto(module);
    (libs.curl orelse @panic("The uploader fixture must reuse the existing product CURL library")).linkInto(module);
    module.addCSourceFiles(.{
        .files = &.{ "tests/sync_upload.cc", "src/mod/sync/upload.cc", "src/mod/sync/state.cc", "src/mod/sync/diagnostics.cc" },
        .flags = ctx.cxxFlags("c++23", &.{ "-Wno-invalid-offsetof", "-UNDEBUG" }),
    });
    const exe = ctx.executable("sync-upload-tests", module);
    steps.build_tests.dependOn(&b.addInstallArtifact(exe, .{}).step);
    const run = b.addSystemCommand(&.{ if (ctx.isWindows()) "python" else "python3", "-B" });
    run.addFileArg(b.path("scripts/test-sync-loopback.py"));
    run.addArtifactArg(exe);
    run.has_side_effects = true;
    steps.test_sync_upload.dependOn(&run.step);
    steps.test_sync.dependOn(&run.step);
}

fn syncModule(ctx: *const Ctx, libs: Libs) *std.Build.Module {
    const b = ctx.b;
    const module = ctx.createModule(true);
    module.addIncludePath(b.path("src/mod"));
    module.addIncludePath(b.path("src/libs/unity/include"));
    libs.il2cpp.includeInto(module);
    inline for (.{ libs.proto, libs.proto_runtime, libs.spdlog, libs.toml, deps.nlohmannJson(ctx) }) |lib|
        lib.linkInto(module);
    return module;
}

/// `test-deps` and `build-dependency-test`: exercise protobuf, zlib and the HTTPS backend as the platform links
/// them. Arguments after `--` pass an optional test URL and CA file.
pub fn addDependencySmoke(ctx: *const Ctx, libs: []const Lib) void {
    const b = ctx.b;
    const module = ctx.createModule(true);
    for (libs) |lib| lib.linkInto(module);
    module.addCSourceFile(.{ .file = b.path("tests/dependency_smoke.cc"), .flags = ctx.cxxFlags("c++23", &.{}) });
    const exe = ctx.executable("dependency-smoke", module);
    const run = b.addRunArtifact(exe);
    compat.addPassthruArgs(run);
    b.step("test-deps", "Run protobuf, zlib and HTTPS backend checks; optionally pass a test URL and CA file")
        .dependOn(&run.step);
    const build_step = b.step("build-dependency-test", "Cross-compile the dependency smoke executable");
    build_step.dependOn(&b.addInstallArtifact(exe, .{}).step);
}

/// Mod headers, logging and TOML for the settings fixtures. il2cpp contributes headers only; settings code never
/// calls into the runtime.
fn settingsModule(ctx: *const Ctx, libs: Libs) *std.Build.Module {
    const b = ctx.b;
    const module = ctx.createModule(true);
    module.addIncludePath(b.path("src/mod"));
    module.addIncludePath(b.path("src/libs/unity/include"));
    libs.spdlog.linkInto(module);
    libs.toml.linkInto(module);
    libs.il2cpp.includeInto(module);
    return module;
}

/// Install `module` as an executable under `build-tests`, and run it under `run_step`.
fn addFixture(
    ctx: *const Ctx,
    build_step: *std.Build.Step,
    run_step: *std.Build.Step,
    name: []const u8,
    module: *std.Build.Module,
) void {
    const b = ctx.b;
    const exe = ctx.executable(name, module);
    build_step.dependOn(&b.addInstallArtifact(exe, .{}).step);
    run_step.dependOn(&b.addRunArtifact(exe).step);
}

/// `path` from the repository root as a C string literal.
fn quotedPath(b: *std.Build, path: []const u8) []const u8 {
    return std.json.Stringify.valueAlloc(b.allocator, compat.pathFromRoot(b, path), .{}) catch @panic("OOM");
}
