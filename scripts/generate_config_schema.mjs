#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const outputPath = path.join(repoRoot, "docs", "windows-launcher", "config-schema.guffawaffle.v1.json");

function read(relativePath) {
  return fs.readFileSync(path.join(repoRoot, relativePath), "utf8").replaceAll("\r\n", "\n");
}

function snakeCase(value) {
  return value
    .replace(/([a-z0-9])([A-Z])/g, "$1_$2")
    .replace(/([A-Z])([A-Z][a-z])/g, "$1_$2")
    .toLowerCase();
}

function titleCase(key) {
  const acronyms = new Set(["api", "dpi", "id", "jsonl", "os", "ssl", "tls", "ui", "url"]);
  return key
    .split("_")
    .map((part) => acronyms.has(part) ? part.toUpperCase() : part[0].toUpperCase() + part.slice(1))
    .join(" ");
}

function parseLiteral(raw) {
  const value = raw.trim().replace(/;$/, "");
  if (value === "true") return true;
  if (value === "false") return false;
  if (/^-?\d+$/.test(value)) return Number.parseInt(value, 10);
  if (/^-?(?:\d+\.\d*|\d*\.\d+)$/.test(value)) return Number.parseFloat(value);
  if (/^"(?:[^"\\]|\\.)*"$/.test(value)) return JSON.parse(value);
  throw new Error(`Unsupported C++/TOML scalar literal: ${raw}`);
}

function parseDefaultConfig() {
  const lines = read("mods/src/defaultconfig.h").split("\n");
  const namespaceStack = [];
  const values = new Map();
  let comments = [];

  const rootSection = new Map([
    ["Buffs", "buffs"],
    ["SystemConfig", "config"],
    ["Control", "control"],
    ["Graphics", "graphics"],
    ["Advanced", "advanced"],
    ["BattleLogDecoder", "battle_log_decoder"],
    ["Notifications", "notifications"],
    ["Patches", "patches"],
    ["Shortcuts", "shortcuts"],
    ["Sync", "sync"],
    ["Sidecar", "sidecar"],
    ["UI", "ui"],
  ]);

  for (const line of lines) {
    const trimmed = line.trim();
    const namespaceMatch = trimmed.match(/^namespace\s+([A-Za-z0-9_]+)\s*$/);
    if (namespaceMatch) {
      namespaceStack.push(namespaceMatch[1]);
      comments = [];
      continue;
    }

    if (/^}\s*\/\/\s*namespace/.test(trimmed)) {
      namespaceStack.pop();
      comments = [];
      continue;
    }

    if (trimmed.startsWith("///")) {
      comments.push(trimmed.replace(/^\/\/\/<?\s?/, "").trim());
      continue;
    }

    const constantMatch = trimmed.match(
      /^constexpr\s+(?:bool|int|auto|const\s+char\s*\*)\s+([A-Za-z0-9_]+)\s*=\s*(.+?);\s*(?:\/\/\/?<\s*(.*))?$/,
    );
    if (!constantMatch || namespaceStack[0] !== "DefaultConfig" || namespaceStack.length < 2) {
      if (trimmed && !trimmed.startsWith("#") && !trimmed.startsWith("//")) comments = [];
      continue;
    }

    const section = rootSection.get(namespaceStack[1]);
    if (!section) {
      comments = [];
      continue;
    }

    const nested = namespaceStack.slice(2).map(snakeCase);
    const settingPath = [section, ...nested, constantMatch[1]].join(".");
    const description = [...comments, constantMatch[3] ?? ""]
      .filter(Boolean)
      .join(" ")
      .replace(/\s+/g, " ")
      .trim();

    // A conditional build override may define the same key twice. The final
    // declaration is the normal production fallback in defaultconfig.h.
    values.set(settingPath, {
      value: parseLiteral(constantMatch[2]),
      description,
      source: `mods/src/defaultconfig.h:${settingPath}`,
    });
    comments = [];
  }

  return values;
}

function stripTomlComment(raw) {
  let quoted = false;
  let escaped = false;
  for (let index = 0; index < raw.length; index += 1) {
    const character = raw[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (character === "\\" && quoted) {
      escaped = true;
      continue;
    }
    if (character === "\"") quoted = !quoted;
    if (character === "#" && !quoted) return raw.slice(0, index);
  }
  return raw;
}

function parseExampleConfig() {
  const entries = [];
  let section = "";
  let comments = [];

  for (const [lineIndex, line] of read("example_community_patch_settings.toml").split("\n").entries()) {
    const trimmed = line.trim();
    if (!trimmed) {
      comments = [];
      continue;
    }
    if (trimmed.startsWith("#")) {
      const text = trimmed.replace(/^#+\s?/, "").trim();
      if (text && !/^[<\[=*>|\-]+$/.test(text) && !/^[*#\s]+$/.test(text)) comments.push(text);
      continue;
    }

    const sectionMatch = trimmed.match(/^\[([A-Za-z0-9_.-]+)]$/);
    if (sectionMatch) {
      section = sectionMatch[1];
      comments = [];
      continue;
    }

    const settingMatch = stripTomlComment(trimmed).match(/^([A-Za-z0-9_-]+)\s*=\s*(.+?)\s*$/);
    if (!settingMatch) {
      throw new Error(`Unsupported reference TOML syntax at line ${lineIndex + 1}: ${line}`);
    }

    entries.push({
      path: `${section}.${settingMatch[1]}`,
      value: parseLiteral(settingMatch[2]),
      description: comments.join(" ").replace(/\s+/g, " ").trim(),
      line: lineIndex + 1,
    });
    comments = [];
  }

  return entries;
}

function parseBoolConfigMetadata() {
  const values = new Map();
  const source = read("mods/src/config_metadata.h");
  const specPattern =
    /inline\s+constexpr\s+BoolConfigSpec\s+[A-Za-z0-9_]+\s*\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"[^"]+"\s*,\s*(true|false)\s*,\s*"([^"]*)"/g;
  for (const match of source.matchAll(specPattern)) {
    const settingPath = `${match[1]}.${match[2]}`;
    values.set(settingPath, {
      value: match[3] === "true",
      description: match[4],
      source: `mods/src/config_metadata.h:${settingPath}`,
    });
  }
  return values;
}

function parseConfigMemberTypes() {
  const values = new Map();
  const source = read("mods/src/config.h");
  const memberPattern =
    /^\s*(bool|double|float|int|int32_t|int64_t|size_t|uint32_t|uint64_t)\s+([A-Za-z0-9_]+)\s*;/gm;
  for (const match of source.matchAll(memberPattern)) {
    let kind = "integer";
    if (["double", "float"].includes(match[1])) kind = "number";
    if (match[1] === "bool") kind = "boolean";
    const existing = values.get(match[2]);
    if (!existing) {
      values.set(match[2], kind);
    } else if (existing !== kind) {
      values.set(match[2], "ambiguous");
    }
  }
  return values;
}

function captureBalancedCall(source, name, startIndex) {
  const open = source.indexOf("(", startIndex + name.length);
  if (open < 0) return null;
  let depth = 0;
  let quoted = false;
  let escaped = false;
  for (let index = open; index < source.length; index += 1) {
    const character = source[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (character === "\\" && quoted) {
      escaped = true;
      continue;
    }
    if (character === "\"") quoted = !quoted;
    if (quoted) continue;
    if (character === "(") depth += 1;
    if (character === ")") {
      depth -= 1;
      if (depth === 0) return source.slice(open + 1, index);
    }
  }
  throw new Error(`Unbalanced ${name} call`);
}

function splitArguments(call) {
  const args = [];
  let start = 0;
  let depth = 0;
  let quoted = false;
  let escaped = false;
  for (let index = 0; index < call.length; index += 1) {
    const character = call[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (character === "\\" && quoted) {
      escaped = true;
      continue;
    }
    if (character === "\"") quoted = !quoted;
    if (quoted) continue;
    if ("([{<".includes(character)) depth += 1;
    if ([")", "]", "}", ">"].includes(character)) depth -= 1;
    if (character === "," && depth === 0) {
      args.push(call.slice(start, index).trim());
      start = index + 1;
    }
  }
  args.push(call.slice(start).trim());
  return args;
}

function unquoteCpp(value) {
  return /^"(?:[^"\\]|\\.)*"$/.test(value) ? JSON.parse(value) : null;
}

function scanLiteralParserPaths() {
  const source = read("mods/src/config.cc");
  const paths = new Set();
  const name = "get_config_or_default";
  let cursor = source.indexOf("void Config::Load()");
  while ((cursor = source.indexOf(name, cursor)) >= 0) {
    const call = captureBalancedCall(source, name, cursor);
    const args = splitArguments(call);
    if (args.length >= 5 && args[0] === "config" && args[1] === "parsed") {
      const section = unquoteCpp(args[2]);
      const key = unquoteCpp(args[3]);
      if (section && key) paths.add(`${section}.${key}`);
    }
    cursor += name.length;
  }
  return paths;
}

function scanCustomParserPaths(defaults) {
  const paths = new Set();
  const canonicalSources = [
    "mods/src/config_sidecar.cc",
    "mods/src/patches/action_queue_repair_config.h",
  ];
  const canonicalPattern =
    /"(advanced\.(?:diagnostics|queue|kirshara_queue)\.[A-Za-z0-9_.]+|sidecar\.(?:sync|logging)\.[A-Za-z0-9_.]+)"/g;
  for (const relativePath of canonicalSources) {
    for (const match of read(relativePath).matchAll(canonicalPattern)) {
      if (defaults.has(match[1])) paths.add(match[1]);
    }
  }

  const metadata = read("mods/src/config_metadata.h");
  const boolSpecPattern =
    /inline\s+constexpr\s+BoolConfigSpec\s+[A-Za-z0-9_]+\s*\{\s*"([^"]+)"\s*,\s*"([^"]+)"/g;
  for (const match of metadata.matchAll(boolSpecPattern)) {
    const settingPath = `${match[1]}.${match[2]}`;
    if (defaults.has(settingPath)) paths.add(settingPath);
  }
  return paths;
}

function scanRuntimeScalarPaths(defaults = parseDefaultConfig()) {
  return new Set([...scanLiteralParserPaths(), ...scanCustomParserPaths(defaults)]);
}

function parseInputBindings() {
  const registry = read("mods/src/patches/input_binding/action_registry.cc");
  const bridge = read("mods/src/patches/input_binding/input_config_bridge.cc");
  const actions = [];
  const actionPattern =
    /InputActionId::([A-Za-z0-9_]+)\s*,\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*TriggerMode::/g;
  for (const match of registry.matchAll(actionPattern)) {
    actions.push({ id: match[1], key: match[2], defaultValue: match[3] });
  }

  const aliasArrays = new Map();
  const arrayPattern =
    /constexpr\s+BindingConfigAlias\s+(k[A-Za-z0-9_]+Aliases)\[\]\s*=\s*\{([\s\S]*?)\};/g;
  for (const match of bridge.matchAll(arrayPattern)) {
    const aliases = [];
    const aliasPattern = /\{\s*"([^"]+)"\s*,\s*BindingConfigSourceKind::([A-Za-z0-9_]+)/g;
    for (const alias of match[2].matchAll(aliasPattern)) {
      aliases.push({
        path: `shortcuts.${alias[1]}`,
        status: alias[2] === "DeprecatedAlias" ? "deprecated" : "compatibility",
        precedence: "canonical-wins",
      });
    }
    aliasArrays.set(match[1], aliases);
  }

  const actionAliases = new Map();
  const switchPattern =
    /case\s+InputActionId::([A-Za-z0-9_]+)\s*:\s*return\s+(k[A-Za-z0-9_]+Aliases)\s*;/g;
  for (const match of bridge.matchAll(switchPattern)) {
    actionAliases.set(match[1], aliasArrays.get(match[2]) ?? []);
  }

  return actions.map((action) => ({
    path: `input.bindings.${action.key}`,
    title: titleCase(action.key),
    description: `Keyboard or mouse binding for ${titleCase(action.key).toLowerCase()}.`,
    category: "input",
    control: "keybinding",
    valueType: { kind: "keybinding" },
    default: action.defaultValue,
    platforms: ["windows", "macos"],
    apply: "next-session",
    sensitivity: "public",
    stability: "stable",
    sourceSupport: ["guffawaffle"],
    aliases: actionAliases.get(action.id) ?? [],
    provenance: { runtimePath: `input.bindings.${action.key}`, defaultSource: "input-action-registry" },
  }));
}

function parseNotificationSettings() {
  const source = read("mods/src/patches/notification_catalog.h");
  const settings = [];
  const entryPattern =
    /NotificationEventCatalogEntry\{\s*NotificationKind::([A-Za-z0-9_]+)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"[\s\S]*?NotificationSound::([A-Za-z0-9_]+)\s*\}/g;

  for (const match of source.matchAll(entryPattern)) {
    const [, , canonicalKey, runtimeName, legacyCategory, legacyKey, sound] = match;
    const aliases = [
      `notifications.events.${legacyCategory}.${legacyKey}`,
      `notifications.system.${legacyCategory}.${legacyKey}`,
      `notifications.audio.${legacyCategory}.${legacyKey}`,
      `notifications.notifications_${canonicalKey}`,
    ].map((aliasPath) => ({
      path: aliasPath,
      status: "deprecated",
      precedence: "canonical-replaces-whole-policy",
      removal: "3.0.0",
    }));

    settings.push({
      path: `notifications.${canonicalKey}`,
      title: titleCase(canonicalKey),
      description: `Configure system and audio delivery for the ${runtimeName} event.`,
      category: "notifications",
      control: "notification-policy",
      valueType: {
        kind: "union",
        variants: [
          { kind: "boolean", semantics: { false: "disabled", true: "system-only" } },
          {
            kind: "object",
            replacement: "whole-value",
            fields: {
              system: { kind: "boolean", default: false },
              audio: { kind: "boolean", default: false },
              sound: {
                kind: "enum",
                values: ["none", "default", "info", "success", "warning", "alarm", "arrival", "soft", "ping", "repair"],
                default: snakeCase(sound),
              },
            },
          },
        ],
      },
      default: false,
      expandedDefault: { system: false, audio: false, sound: snakeCase(sound) },
      invalidValueBehavior: "warn-and-use-event-default",
      platforms: ["windows", "macos"],
      apply: "next-session",
      sensitivity: "public",
      stability: runtimeName.startsWith("experimental.") ? "experimental" : "stable",
      sourceSupport: ["guffawaffle"],
      aliases,
      provenance: { runtimePath: `notifications.${canonicalKey}`, defaultSource: "notification-event-catalog" },
    });
  }

  if (settings.length === 0) throw new Error("No notification catalog entries were discovered.");
  return settings;
}

function parseSyncTargetSettings(defaults, configMemberTypes) {
  const configHeader = read("mods/src/config.h");
  const optionKeys = [...configHeader.matchAll(
    /SyncConfig::Option\{SyncConfig::Type::[A-Za-z0-9_]+,\s*"[^"]+",\s*"([^"]+)"/g,
  )].map((match) => match[1]);
  const keys = [
    "url",
    "token",
    "proxy",
    "verify_ssl",
    "allow_unsafe_tls_without_certificate_validation",
    ...optionKeys,
  ];

  const uniqueKeys = [...new Set(keys)];
  const settings = uniqueKeys.map((key) => {
    const fallback = defaults.get(`sync.${key}`);
    if (!fallback) throw new Error(`No sync target default was found for ${key}.`);
    const settingPath = `sync.targets.*.${key}`;
    return {
      path: settingPath,
      title: titleCase(key),
      description: fallback.description || `Configure ${titleCase(key).toLowerCase()} for this sync target.`,
      category: "sync",
      control: "scalar",
      valueType: schemaType(settingPath, fallback.value, configMemberTypes),
      default: fallback.value,
      platforms: ["windows", "macos"],
      apply: "next-session",
      sensitivity: sensitivityFor(settingPath),
      stability: "stable",
      sourceSupport: ["guffawaffle"],
      aliases: [],
      provenance: { runtimePath: settingPath, defaultSource: fallback.source },
    };
  });

  settings.push({
    path: "sync.targets.*.mode",
    title: "Mode",
    description: "Outbound sync contract used by this target.",
    category: "sync",
    control: "scalar",
    valueType: { kind: "enum", values: ["legacy", "majel"] },
    default: "legacy",
    platforms: ["windows", "macos"],
    apply: "next-session",
    sensitivity: "public",
    stability: "stable",
    sourceSupport: ["guffawaffle"],
    aliases: [],
    provenance: { runtimePath: "sync.targets.*.mode", defaultSource: "SyncTargetConfig::Mode" },
  });
  return settings;
}

function schemaType(settingPath, value, configMemberTypes = new Map()) {
  const enumValues = {
    "input.scopely_shortcuts": ["off", "native", "fallback"],
    "input.original_frame_policy": ["mod", "fallthrough_unhandled", "fallthrough_all"],
    "advanced.diagnostics.runtime_trace": ["off", "summary", "detailed", "verbose"],
    "sidecar.sync.fleet_runtime_mode": ["normal", "request_only", "snapshot_only", "enqueue_no_transport"],
  };
  if (settingPath.startsWith("ui.mission_hud.")) {
    return { kind: "enum", values: ["auto", "always", "never"] };
  }
  if (enumValues[settingPath]) return { kind: "enum", values: enumValues[settingPath] };
  if (typeof value === "boolean") return { kind: "boolean" };
  if (typeof value === "number") {
    const memberKind = configMemberTypes.get(settingPath.split(".").at(-1));
    if (["integer", "number"].includes(memberKind)) return { kind: memberKind };
    return { kind: Number.isInteger(value) ? "integer" : "number" };
  }
  if (typeof value === "string") return { kind: "string" };
  if (Array.isArray(value)) return { kind: "array" };
  throw new Error(`Unsupported schema value: ${JSON.stringify(value)}`);
}

function sensitivityFor(settingPath) {
  const leaf = settingPath.split(".").at(-1);
  if (leaf === "token") return "secret";
  if (["url", "proxy", "loader_image", "root"].includes(leaf)) return "private";
  return "public";
}

function stabilityFor(settingPath) {
  if (settingPath.startsWith("advanced.diagnostics.")
      || settingPath.startsWith("advanced.kirshara_queue.")
      || settingPath === "control.allow_key_fallthrough") return "internal";
  if (settingPath.startsWith("patches.")) return "advanced";
  if (settingPath === "control.enable_experimental" || settingPath.startsWith("advanced.queue.")) return "experimental";
  return "stable";
}

function applyFor(settingPath) {
  return settingPath.startsWith("patches.") ? "restart-required" : "next-session";
}

function scalarAliases(settingPath) {
  const aliases = {
    "input.original_frame_policy": [
      { path: "control.original_frame_policy", status: "deprecated", precedence: "canonical-wins" },
    ],
    "sidecar.sync.battlelog_enrichment": [
      { path: "battle_log_decoder.enabled", status: "deprecated", precedence: "canonical-wins" },
    ],
    "advanced.diagnostics.ship_identity": [
      { path: "sidecar.probes.ship_identity", status: "deprecated", precedence: "canonical-wins" },
    ],
    "advanced.diagnostics.battle_log_decoder": [
      { path: "sidecar.probes.battle_log_decoder", status: "deprecated", precedence: "canonical-wins" },
    ],
    "advanced.diagnostics.battle_catalog": [
      { path: "sidecar.probes.battle_catalog", status: "deprecated", precedence: "canonical-wins" },
    ],
    "advanced.diagnostics.debug": [
      { path: "sidecar.diagnostics.debug", status: "deprecated", precedence: "canonical-wins" },
    ],
    "advanced.diagnostics.logging": [
      { path: "sidecar.diagnostics.logging", status: "deprecated", precedence: "canonical-wins" },
    ],
  };
  return aliases[settingPath] ?? [];
}

function constraintsFor(settingPath) {
  if (/^advanced\.diagnostics\.files\.(?:navhook_trace|action_queue_probe)_(?:max_kb|files)$/.test(settingPath)) {
    return { minimum: 1 };
  }
  if (settingPath === "advanced.diagnostics.runtime_trace_report_interval_ms") {
    return { minimum: 1000, maximum: 60000 };
  }
  if (settingPath === "sidecar.logging.jsonl_replay_seconds" || settingPath === "sidecar.logging.jsonl_recent_logs") {
    return { minimum: 0 };
  }
  return {};
}

function resolveDefaultPath(settingPath) {
  if (settingPath === "input.scopely_shortcuts") return "control.scopely_shortcuts";
  if (settingPath === "input.original_frame_policy") return "control.original_frame_policy";
  if (settingPath.startsWith("sync.targets.*.")) return `sync.${settingPath.split(".").at(-1)}`;
  return settingPath;
}

function makeScalarSetting(settingPath, fallback, description = "", configMemberTypes = new Map()) {
  const constraints = constraintsFor(settingPath);
  return {
    path: settingPath,
    title: titleCase(settingPath.split(".").at(-1)),
    description: fallback.description || description || `Configure ${titleCase(settingPath.split(".").at(-1)).toLowerCase()}.`,
    category: settingPath.split(".")[0],
    control: "scalar",
    valueType: schemaType(settingPath, fallback.value, configMemberTypes),
    ...(Object.keys(constraints).length > 0 ? { constraints } : {}),
    default: fallback.value,
    platforms: ["windows", "macos"],
    apply: applyFor(settingPath),
    sensitivity: sensitivityFor(settingPath),
    stability: stabilityFor(settingPath),
    sourceSupport: ["guffawaffle"],
    aliases: scalarAliases(settingPath),
    provenance: { runtimePath: settingPath, defaultSource: fallback.source },
  };
}

function buildSchema() {
  const defaults = new Map([...parseDefaultConfig(), ...parseBoolConfigMetadata()]);
  const configMemberTypes = parseConfigMemberTypes();
  const example = parseExampleConfig();
  const parserPaths = scanRuntimeScalarPaths(defaults);
  const settings = [];
  const seen = new Set();

  for (const entry of example) {
    if (entry.path.startsWith("notifications.") || entry.path.startsWith("shortcuts.")) continue;
    if (entry.path === "battle_log_decoder.enabled") continue;

    const canonicalPath = entry.path.replace(/^sync\.targets\.[^.]+\./, "sync.targets.*.");
    if (seen.has(canonicalPath)) continue;
    seen.add(canonicalPath);

    const defaultPath = resolveDefaultPath(canonicalPath);
    const fallback = defaultPath ? defaults.get(defaultPath) : null;
    if (!fallback) {
      throw new Error(`No runtime default was resolved for ${canonicalPath} (looked for ${defaultPath}).`);
    }

    settings.push(makeScalarSetting(canonicalPath, fallback, entry.description, configMemberTypes));
  }

  for (const parserPath of parserPaths) {
    if (seen.has(parserPath)) continue;
    const defaultPath = resolveDefaultPath(parserPath);
    const fallback = defaults.get(defaultPath);
    if (!fallback) throw new Error(`No runtime default was resolved for custom parser path ${parserPath}.`);
    seen.add(parserPath);
    settings.push(makeScalarSetting(parserPath, fallback, "", configMemberTypes));
  }

  settings.push(
    ...parseSyncTargetSettings(defaults, configMemberTypes),
    ...parseInputBindings(),
    ...parseNotificationSettings(),
  );
  settings.sort((left, right) => left.path < right.path ? -1 : left.path > right.path ? 1 : 0);

  const paths = new Set(settings.map((setting) => setting.path));
  const missingParserPaths = [...parserPaths]
    .filter((parserPath) => !paths.has(parserPath))
    .filter((parserPath) => ![
      "battle_log_decoder.emit_feed",
      "battle_log_decoder.emit_segments",
    ].includes(parserPath));
  if (missingParserPaths.length > 0) {
    throw new Error(`Parser paths missing from generated schema: ${missingParserPaths.sort().join(", ")}`);
  }

  return {
    schemaVersion: "1.0.0",
    schemaId: "stfc-community-mod.config-schema",
    source: {
      id: "guffawaffle",
      repository: "Guffawaffle/stfc-mod",
      persistence: {
        runtimeFormat: "toml",
        writeMode: "sparse",
        authority: "mod-runtime-parser",
      },
    },
    contract: {
      aliasPrecedence: "canonical-wins",
      invalidScalarBehavior: "warn-and-use-runtime-default",
      unknownKeyBehavior: "preserve",
      supportedAdapters: ["scalar", "keybinding", "notification-policy"],
    },
    settings,
  };
}

if (path.resolve(process.argv[1] ?? "") === fileURLToPath(import.meta.url)) {
  const check = process.argv.includes("--check");
  const schema = buildSchema();
  const generated = `${JSON.stringify(schema, null, 2)}\n`;
  if (check) {
    if (!fs.existsSync(outputPath)) {
      console.error(`Generated config schema is missing: ${path.relative(repoRoot, outputPath)}`);
      process.exit(1);
    }
    const existing = fs.readFileSync(outputPath, "utf8").replaceAll("\r\n", "\n");
    if (existing !== generated) {
      console.error("Generated config schema is stale. Run: node scripts/generate_config_schema.mjs");
      process.exit(1);
    }
    console.log(`Config schema is current (${schema.settings.length} settings).`);
  } else {
    fs.writeFileSync(outputPath, generated, "utf8");
    console.log(`Wrote ${path.relative(repoRoot, outputPath)} (${schema.settings.length} settings).`);
  }
}

export {
  buildSchema,
  parseDefaultConfig,
  parseBoolConfigMetadata,
  parseConfigMemberTypes,
  parseExampleConfig,
  scanCustomParserPaths,
  scanLiteralParserPaths,
  scanRuntimeScalarPaths,
};
