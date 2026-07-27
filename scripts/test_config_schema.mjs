#!/usr/bin/env node

import assert from "node:assert/strict";
import test from "node:test";

import {
  buildSchema,
  parseDefaultConfig,
  parseExampleConfig,
  scanLiteralParserPaths,
} from "./generate_config_schema.mjs";

test("default and example inventories are non-empty", () => {
  assert.ok(parseDefaultConfig().size > 150);
  assert.ok(parseExampleConfig().length > 250);
});

test("every literal generic parser path is represented", () => {
  const schemaPaths = new Set(buildSchema().settings.map((setting) => setting.path));
  for (const parserPath of scanLiteralParserPaths()) {
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
    for (const alias of setting.aliases) assert.ok(alias.precedence);
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
