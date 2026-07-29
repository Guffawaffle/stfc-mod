#!/usr/bin/env node

import assert from "node:assert/strict";
import test from "node:test";

import {
  buildSchema,
  parseBoolConfigMetadata,
  parseConfigMemberTypes,
  parseDefaultConfig,
  parseExampleConfig,
  scanRuntimeScalarPaths,
} from "./generate_config_schema.mjs";

test("default and example inventories are non-empty", () => {
  assert.ok(parseDefaultConfig().size > 150);
  assert.ok(parseBoolConfigMetadata().has("control.allow_key_fallthrough"));
  assert.ok(parseExampleConfig().length > 250);
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
