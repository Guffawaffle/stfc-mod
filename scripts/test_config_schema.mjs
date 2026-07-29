#!/usr/bin/env node

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  addPresentationMetadata,
  buildSchema,
  parseBoolConfigMetadata,
  parseConfigMemberTypes,
  parseDefaultConfig,
  parseExampleConfig,
  scanRuntimeScalarPaths,
  validatePresentationOverrides,
} from "./generate_config_schema.mjs";

test("default and example inventories are non-empty", () => {
  assert.ok(parseDefaultConfig().size > 150);
  assert.ok(parseBoolConfigMetadata().has("control.allow_key_fallthrough"));
  assert.ok(parseExampleConfig().length > 250);
});

test("runtime manifest positively identifies the principal taxonomy contract", () => {
  const manifest = JSON.parse(readFileSync(
    new URL("../docs/windows-launcher/runtime-manifest.guffawaffle.v1.json", import.meta.url),
    "utf8",
  ));
  const versionHeader = readFileSync(
    new URL("../mods/src/version.h", import.meta.url),
    "utf8",
  );
  const versionParts = ["MAJOR", "MINOR", "REVISION", "PATCH"].map((part) => {
    const match = versionHeader.match(new RegExp(`#define VERSION_${part}\\s+(\\d+)`));
    assert.ok(match, `missing VERSION_${part}`);
    return match[1];
  });

  assert.equal(manifest.manifestSchema, 1);
  assert.equal(manifest.distributionId, "guffawaffle.stfc-community-mod");
  assert.equal(manifest.runtimeVersion, versionParts.join("."));
  assert.equal(typeof manifest.sourceRevision, "string");
  assert.ok(manifest.sourceRevision.length > 0);
  assert.deepEqual(manifest.capabilities, ["settings.principal-taxonomy.v1"]);
  assert.equal(manifest.settingsCatalog.schemaVersion, 1);
  assert.match(manifest.settingsCatalog.revision, /^guffawaffle-taxonomy-/);
  const schema = buildSchema();
  assert.equal(schema.source.id, "guffawaffle");
  assert.equal(
    manifest.settingsCatalog.schemaVersion,
    Number.parseInt(schema.schemaVersion.split(".")[0], 10),
  );
});

test("numeric schema kinds retain concrete Config member types", () => {
  const memberTypes = parseConfigMemberTypes();
  assert.equal(memberTypes.get("select_timer"), "integer");
  assert.equal(memberTypes.get("default_system_zoom"), "number");
  assert.equal(memberTypes.get("keyboard_zoom_speed"), "number");

  const schema = buildSchema();
  for (const settingPath of [
    "graphics.default_system_zoom",
    "graphics.fr_scale",
    "graphics.keyboard_zoom_speed",
    "graphics.loader_logo_scale",
    "graphics.system_zoom_preset_1",
    "graphics.system_zoom_preset_2",
    "graphics.system_zoom_preset_3",
    "graphics.system_zoom_preset_4",
    "graphics.system_zoom_preset_5",
    "graphics.zoom",
  ]) {
    const setting = schema.settings.find((candidate) => candidate.path === settingPath);
    assert.equal(setting.valueType.kind, "number", `${settingPath} must retain its runtime float type`);
  }
});

test("public string settings declare purpose-specific editor formats", () => {
  const schema = buildSchema();
  const formats = new Map([
    ["config.assets_url_override", "uri"],
    ["config.settings_url", "uri"],
    ["ui.disabled_banner_types", "comma-separated-list"],
  ]);

  for (const [settingPath, format] of formats) {
    const setting = schema.settings.find((candidate) => candidate.path === settingPath);
    assert.equal(setting.valueType.kind, "string");
    assert.equal(setting.valueType.format, format);
  }
});

test("every discovered runtime scalar parser path is represented", () => {
  const defaults = parseDefaultConfig();
  const schemaPaths = new Set(buildSchema().settings.map((setting) => setting.path));
  for (const parserPath of scanRuntimeScalarPaths(defaults)) {
    assert.ok(schemaPaths.has(parserPath), `missing parser path: ${parserPath}`);
  }
});

test("schema has three unified control adapters", () => {
  const schema = buildSchema();
  const controls = new Set(schema.settings.map((setting) => setting.control));
  assert.deepEqual([...controls].sort(), ["keybinding", "notification-policy", "scalar"]);
});

test("every setting has unique launcher and provenance metadata", () => {
  const schema = buildSchema();
  const paths = new Set();
  const aliasOwners = new Map();
  for (const setting of schema.settings) {
    assert.ok(!paths.has(setting.path), `duplicate setting path: ${setting.path}`);
    paths.add(setting.path);
    assert.ok(setting.title);
    assert.ok(setting.description);
    assert.ok(setting.category);
    assert.ok(setting.valueType.kind);
    assert.ok(setting.platforms.length > 0);
    assert.ok(setting.apply);
    assert.ok(setting.sensitivity);
    assert.ok(setting.stability);
    assert.ok(setting.provenance.runtimePath);
    assert.ok(setting.provenance.defaultSource);
    for (const alias of setting.aliases) {
      assert.ok(alias.precedence);
      assert.ok(!aliasOwners.has(alias.path),
        `alias ${alias.path} is claimed by both ${aliasOwners.get(alias.path)} and ${setting.path}`);
      aliasOwners.set(alias.path, setting.path);
    }
  }
});

test("every setting has generated and validated player presentation", () => {
  const schema = buildSchema();
  const applyLabels = new Map([
    ["live", "Immediate"],
    ["next-session", "Next launch"],
    ["restart-required", "Restart required"],
  ]);

  for (const setting of schema.settings) {
    const presentation = setting.presentation;
    assert.ok(presentation.label?.trim(), `missing presentation label: ${setting.path}`);
    assert.ok(presentation.group?.trim(), `missing presentation group: ${setting.path}`);
    assert.ok(presentation.accessibleName?.trim(), `missing accessible name: ${setting.path}`);
    assert.ok(presentation.accessibleHelp?.trim(), `missing accessible help: ${setting.path}`);
    assert.ok(["compact", "standard", "wide"].includes(presentation.editorWidth));
    assert.equal(presentation.applyTiming, applyLabels.get(setting.apply));
    assert.ok(presentation.searchTerms.includes(setting.path));
    for (const alias of setting.aliases) {
      assert.ok(
        presentation.searchTerms.includes(alias.path),
        `presentation search omits alias ${alias.path} for ${setting.path}`,
      );
    }
    if (presentation.unit) {
      assert.equal(setting.control, "scalar");
      assert.ok(["integer", "number"].includes(setting.valueType.kind));
    }
    if (presentation.enumOptions) {
      assert.equal(setting.valueType.kind, "enum");
      assert.deepEqual(
        new Set(presentation.enumOptions.map((option) => option.value)),
        new Set(setting.valueType.values),
      );
      assert.ok(presentation.enumOptions.every((option) => option.label?.trim()));
    }
  }
});

test("presentation overrides improve representative player copy without changing runtime metadata", () => {
  const schema = buildSchema();
  const backdrop = schema.settings.find((setting) => setting.path === "graphics.fr_scale");
  assert.equal(backdrop.title, "Fr Scale");
  assert.equal(backdrop.apply, "next-session");
  assert.equal(backdrop.presentation.label, "System backdrop scale");
  assert.equal(backdrop.presentation.group, "Camera");
  assert.equal(backdrop.presentation.unit, "×");

  const queueAdd = schema.settings.find((setting) => setting.path === "input.bindings.fleet_queue_add");
  assert.equal(queueAdd.title, "Fleet Queue Add");
  assert.equal(queueAdd.valueType.kind, "keybinding");
  assert.equal(queueAdd.presentation.label, "Add fleet to queue");
  assert.equal(queueAdd.presentation.group, "Fleet");
});

test("keybinding presentation normalizes joined tokens numbers and fleet actions", () => {
  const schema = buildSchema();
  const labels = new Map([
    ["input.bindings.select_ship1", "Select ship 1"],
    ["input.bindings.set_zoom_preset1", "Set zoom preset 1"],
    ["input.bindings.select_chatalliance", "Select chat alliance"],
    ["input.bindings.show_awayteam", "Show away team"],
    ["input.bindings.show_stationinterior", "Show station interior"],
    ["input.bindings.fleet_queue_clear", "Clear fleet queue"],
    ["input.bindings.fleet_recall_cancel", "Cancel fleet recall"],
    ["input.bindings.fleet_repair", "Repair fleet"],
    ["input.bindings.fleet_view_info", "View fleet info"],
    ["input.bindings.hotkeys_disable", "Disable hotkeys"],
    ["input.bindings.log_debug", "Set logging to debug"],
  ]);

  for (const [settingPath, expectedLabel] of labels) {
    const setting = schema.settings.find((candidate) => candidate.path === settingPath);
    assert.equal(setting.presentation.label, expectedLabel, settingPath);
    assert.match(setting.presentation.help, /^Keyboard or mouse shortcut for /);
  }
});

test("data sync presentation hides implementation enums and supplies units", () => {
  const schema = buildSchema();
  const runtimeMode = schema.settings.find(
    (setting) => setting.path === "sidecar.sync.fleet_runtime_mode",
  );
  assert.equal(runtimeMode.presentation.label, "Fleet runtime behavior");
  assert.ok(!runtimeMode.presentation.help.includes("request_only"));
  assert.deepEqual(
    runtimeMode.presentation.enumOptions.map(({ value, label }) => ({ value, label })),
    [
      { value: "normal", label: "Normal" },
      { value: "request_only", label: "Requests only" },
      { value: "snapshot_only", label: "Build snapshots only" },
      { value: "enqueue_no_transport", label: "Queue without delivery" },
    ],
  );
  assert.ok(runtimeMode.presentation.enumOptions.every((option) => option.help?.trim()));

  const retainedLogs = schema.settings.find(
    (setting) => setting.path === "sidecar.logging.jsonl_recent_logs",
  );
  const replayWindow = schema.settings.find(
    (setting) => setting.path === "sidecar.logging.jsonl_replay_seconds",
  );
  assert.equal(retainedLogs.presentation.label, "Retained battle-log groups");
  assert.equal(retainedLogs.presentation.unit, "groups");
  assert.equal(replayWindow.presentation.label, "Replay window");
  assert.equal(replayWindow.presentation.unit, "seconds");

  const generic = schema.settings.find(
    (setting) => setting.path === "sidecar.sync.proxy",
  );
  assert.equal(generic.presentation.help, undefined);
});

test("notification presentation uses consequence help or intentionally omits it", () => {
  const schema = buildSchema();
  const expectedHelp = new Map([
    ["notifications.fleet_started_mining", "Alerts you when your fleet starts mining."],
    [
      "notifications.incoming_attack_player",
      "Alerts you when another player attacks one of your assets.",
    ],
    ["notifications.armada_created", "Alerts you when an armada is created."],
    ["notifications.armada_battle_lost", "Alerts you when your armada battle is lost."],
    ["notifications.assault_victory", "Alerts you when you win an assault."],
    ["notifications.challenge_failed", "Alerts you when challenge fails."],
    ["notifications.dynamic_crisis_failed", "Alerts you when dynamic crisis fails."],
    ["notifications.dynamic_crisis_update", "Alerts you when dynamic crisis status changes."],
    [
      "notifications.cross_alliance_armada_partial_victory",
      "Alerts you when cross alliance armada battle ends in a partial victory.",
    ],
    ["notifications.faction_weekly_events_complete", "Alerts you when faction weekly events are complete."],
  ]);

  for (const [settingPath, help] of expectedHelp) {
    const setting = schema.settings.find((candidate) => candidate.path === settingPath);
    assert.equal(setting.presentation.help, help, settingPath);
  }

  const generic = schema.settings.find((setting) => setting.path === "notifications.standard");
  assert.equal(generic.presentation.help, undefined);
  assert.equal(generic.presentation.accessibleHelp, "Applies: Next launch.");
  assert.ok(schema.settings
    .filter((setting) => setting.control === "notification-policy")
    .every((setting) => !setting.presentation.help?.startsWith("Configure system and audio")));
});

test("presentation grouping derives bounded hotkey and notification domains", () => {
  const schema = buildSchema();
  const fleetPrimary = schema.settings.find((setting) => setting.path === "input.bindings.fleet_primary");
  const battle = schema.settings.find((setting) => setting.path === "notifications.incoming_attack_player");
  const repair = schema.settings.find((setting) => setting.path === "notifications.fleet_repair_complete");
  assert.equal(fleetPrimary.presentation.group, "Fleet");
  assert.equal(battle.presentation.group, "Battle and incoming attacks");
  assert.equal(repair.presentation.group, "Repairs and docking");
});

test("presentation override validation rejects stale duplicate forbidden and incompatible entries", () => {
  const settings = [{
    path: "graphics.example",
    title: "Example",
    description: "Example.",
    category: "graphics",
    control: "scalar",
    valueType: { kind: "boolean" },
    apply: "next-session",
    aliases: [],
  }];

  assert.throws(
    () => validatePresentationOverrides(settings, [{ path: "graphics.missing", label: "Missing" }]),
    /Stale presentation override path/,
  );
  assert.throws(
    () => validatePresentationOverrides(settings, [
      { path: "graphics.example", label: "First" },
      { path: "graphics.example", label: "Second" },
    ]),
    /Duplicate presentation override path/,
  );
  assert.throws(
    () => validatePresentationOverrides(settings, [{ path: "graphics.example", default: true }]),
    /unsupported field 'default'/,
  );
  assert.throws(
    () => validatePresentationOverrides(settings, [{ path: "graphics.example", unit: "%" }]),
    /unit for a non-numeric scalar/,
  );
  assert.throws(
    () => validatePresentationOverrides(settings, [{ path: "graphics.example", searchTerms: [""] }]),
    /must not be blank/,
  );
  assert.throws(
    () => addPresentationMetadata([{ ...settings[0], apply: "surprise" }], []),
    /unsupported apply behavior/,
  );

  const enumSetting = {
    ...settings[0],
    path: "graphics.mode",
    valueType: { kind: "enum", values: ["one", "two"] },
  };
  assert.throws(
    () => validatePresentationOverrides(
      [enumSetting],
      [{
        path: "graphics.mode",
        enumOptions: [{ value: "one", label: "One" }],
      }],
    ),
    /must cover every declared value/,
  );
  assert.throws(
    () => validatePresentationOverrides(
      settings,
      [{
        path: "graphics.example",
        enumOptions: [],
      }],
    ),
    /requires an enum setting/,
  );
});

test("notification policies replace deprecated values as a whole", () => {
  const schema = buildSchema();
  const incoming = schema.settings.find((setting) => setting.path === "notifications.incoming_attack_player");
  assert.equal(incoming.control, "notification-policy");
  assert.equal(incoming.default, false);
  assert.equal(incoming.invalidValueBehavior, "warn-and-use-event-default");
  assert.ok(incoming.aliases.length >= 4);
  assert.ok(incoming.aliases.every((alias) => alias.precedence === "canonical-replaces-whole-policy"));
});

test("keybindings derive defaults and aliases from runtime registries", () => {
  const schema = buildSchema();
  const disable = schema.settings.find((setting) => setting.path === "input.bindings.hotkeys_disable");
  assert.equal(disable.default, "CTRL-ALT-MINUS");
  assert.ok(disable.aliases.some((alias) => alias.path === "shortcuts.set_hotkeys_disble"
    && alias.status === "deprecated"));

  const fleetPrimary = schema.settings.find((setting) => setting.path === "input.bindings.fleet_primary");
  assert.deepEqual(fleetPrimary.valueType, {
    kind: "keybinding",
    multiple: true,
    unbound: "NONE",
    triggerMode: "Down",
    inputPhase: "Frame",
    inputLayer: "Fleet",
    conflictGroup: "FleetAction",
    actionCategory: "Fleet",
  });

  const queueAdd = schema.settings.find((setting) => setting.path === "input.bindings.fleet_queue_add");
  assert.equal(queueAdd.valueType.conflictGroup, "None");

  const zoomIn = schema.settings.find((setting) => setting.path === "input.bindings.zoom_in");
  assert.equal(zoomIn.valueType.triggerMode, "Pressed");
  assert.equal(zoomIn.valueType.conflictGroup, "Zoom");
});

test("sensitivity and restart metadata cover risky settings", () => {
  const schema = buildSchema();
  const token = schema.settings.find((setting) => setting.path === "sync.token");
  const targetToken = schema.settings.find((setting) => setting.path === "sync.targets.*.token");
  const patch = schema.settings.find((setting) => setting.path === "patches.zoomhooks");
  assert.equal(token.sensitivity, "secret");
  assert.equal(targetToken.sensitivity, "secret");
  assert.equal(patch.apply, "restart-required");
});

test("deprecated sidecar observability paths are aliases, not duplicate settings", () => {
  const schema = buildSchema();
  const paths = new Set(schema.settings.map((setting) => setting.path));
  assert.ok(!paths.has("battle_log_decoder.enabled"));
  const enrichment = schema.settings.find((setting) => setting.path === "sidecar.sync.battlelog_enrichment");
  assert.ok(enrichment.aliases.some((alias) => alias.path === "battle_log_decoder.enabled"));
  const shipIdentity = schema.settings.find((setting) => setting.path === "advanced.diagnostics.ship_identity");
  assert.ok(shipIdentity.aliases.some((alias) => alias.path === "sidecar.probes.ship_identity"));
});

test("hidden runtime settings remain machine-readable but internal", () => {
  const schema = buildSchema();
  for (const settingPath of [
    "advanced.diagnostics.runtime_trace",
    "advanced.diagnostics.runtime_trace_report_interval_ms",
    "advanced.queue.queue_repair_enabled",
    "advanced.kirshara_queue.course_target_completion",
    "control.allow_key_fallthrough",
  ]) {
    const setting = schema.settings.find((candidate) => candidate.path === settingPath);
    assert.ok(setting, `missing hidden runtime setting: ${settingPath}`);
    assert.ok(["internal", "experimental"].includes(setting.stability));
  }
});

test("every reference setting is canonical or represented by an alias", () => {
  const schema = buildSchema();
  const canonicalPaths = new Set(schema.settings.map((setting) => setting.path));
  const aliasPaths = new Set(schema.settings.flatMap((setting) => setting.aliases.map((alias) => alias.path)));
  for (const entry of parseExampleConfig()) {
    const normalized = entry.path.replace(/^sync\.targets\.[^.]+\./, "sync.targets.*.");
    assert.ok(canonicalPaths.has(normalized) || aliasPaths.has(normalized),
      `reference setting is neither canonical nor an alias: ${entry.path}`);
  }
});
