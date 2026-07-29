#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

import { configPresentationOverrides } from "./config_presentation_overrides.mjs";

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
    /InputActionId::([A-Za-z0-9_]+)\s*,\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*TriggerMode::([A-Za-z0-9_]+)\s*,\s*InputPhase::([A-Za-z0-9_]+)\s*,\s*InputLayer::([A-Za-z0-9_]+)\s*,\s*ConflictGroup::([A-Za-z0-9_]+)\s*,\s*(\d+)\s*\}\s*,\s*ActionCategory::([A-Za-z0-9_]+)/g;
  for (const match of registry.matchAll(actionPattern)) {
    actions.push({
      id: match[1],
      key: match[2],
      defaultValue: match[3],
      triggerMode: match[4],
      inputPhase: match[5],
      inputLayer: match[6],
      conflictGroup: match[7],
      priority: Number.parseInt(match[8], 10),
      actionCategory: match[9],
    });
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
    valueType: {
      kind: "keybinding",
      multiple: true,
      unbound: "NONE",
      triggerMode: action.triggerMode,
      inputPhase: action.inputPhase,
      inputLayer: action.inputLayer,
      conflictGroup: action.conflictGroup,
      actionCategory: action.actionCategory,
    },
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
  if (typeof value === "string") {
    const stringFormats = {
      "config.assets_url_override": "uri",
      "config.settings_url": "uri",
      "ui.disabled_banner_types": "comma-separated-list",
    };
    const format = stringFormats[settingPath];
    return format ? { kind: "string", format } : { kind: "string" };
  }
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

const applyTimingLabels = new Map([
  ["live", "Immediate"],
  ["next-session", "Next launch"],
  ["restart-required", "Restart required"],
]);

const presentationOverrideFields = new Set([
  "path",
  "label",
  "help",
  "group",
  "searchTerms",
  "enumOptions",
  "unit",
  "editorWidth",
]);

const editorWidths = new Set(["compact", "standard", "wide"]);

function normalizePresentationCopy(value) {
  return value
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, " ")
    .trim();
}

function cleanPresentationHelp(description, label) {
  const cleaned = description
    .replace(/\s+Defaults?:\s*.*$/i, "")
    .replace(/\s+/g, " ")
    .trim();
  if (!cleaned) return null;

  const normalizedLabel = normalizePresentationCopy(label);
  const normalizedHelp = normalizePresentationCopy(cleaned);
  const withoutGenericVerb = normalizePresentationCopy(
    cleaned.replace(/^(configure|enable|disable|toggle|set|choose)\s+/i, ""),
  );
  return normalizedHelp === normalizedLabel || withoutGenericVerb === normalizedLabel
    ? null
    : cleaned;
}

function sentenceCasePresentationLabel(value) {
  const knownWords = new Map([
    ["Battlelog", "Battle log"],
    ["Battlelogs", "Battle logs"],
    ["Realtime", "Real-time"],
    ["Jsonl", "JSONL"],
    ["Ttl", "TTL"],
    ["Url", "URL"],
    ["Tls", "TLS"],
    ["Ssl", "SSL"],
    ["Stfc", "STFC"],
    ["Dpi", "DPI"],
    ["Ui", "UI"],
    ["Scopely", "Scopely"],
  ]);
  const words = value.split(/\s+/).flatMap((word) => {
    const punctuation = word.match(/^(.*?)([:,]?)$/);
    const body = punctuation?.[1] ?? word;
    const suffix = punctuation?.[2] ?? "";
    const replacement = knownWords.get(body) ?? body;
    const parts = replacement.split(" ");
    parts[parts.length - 1] += suffix;
    return parts;
  });

  return words.map((word, index) => {
    const punctuation = word.match(/^(.*?)([:,]?)$/);
    const body = punctuation?.[1] ?? word;
    const suffix = punctuation?.[2] ?? "";
    if (index === 0 || /^[A-Z0-9]{2,}$/.test(body) || knownWords.has(body)) {
      return `${body}${suffix}`;
    }

    return `${body.toLowerCase()}${suffix}`;
  }).join(" ");
}

function normalizeJoinedPresentationTokens(value) {
  return value
    .replace(/chatside/g, "chat_side")
    .replace(/chat(alliance|global|private)/g, "chat_$1")
    .replace(/awayteam/g, "away_team")
    .replace(/station(interior|exterior)/g, "station_$1")
    .replace(/qtrials/g, "q_trials")
    .replace(/([a-z])(\d+)/g, "$1_$2");
}

function keybindingPresentationLabel(setting) {
  const key = setting.path.split(".").at(-1);
  const hotkeysAction = key.match(/^hotkeys_(enable|disable)$/);
  if (hotkeysAction) return `${titleCase(hotkeysAction[1])} hotkeys`;
  const loggingAction = key.match(/^log_(debug|error|info|off|trace|warn)$/);
  if (loggingAction) return `Set logging to ${titleCase(loggingAction[1])}`;
  const queueAction = key.match(/^fleet_queue_(add|clear|toggle)$/);
  if (queueAction) {
    const verb = titleCase(queueAction[1]);
    return queueAction[1] === "add" ? "Add fleet to queue" : `${verb} fleet queue`;
  }
  const fleetAction = key.match(/^fleet_(recall|repair|service)$/);
  if (fleetAction) return `${titleCase(fleetAction[1])} fleet`;
  const fleetOrdinal = key.match(/^fleet_(primary|secondary)$/);
  if (fleetOrdinal) return `${titleCase(fleetOrdinal[1])} fleet action`;
  if (key === "fleet_recall_cancel") return "Cancel fleet recall";
  if (key === "fleet_view_info") return "View fleet info";
  if (key === "select_current") return "Select current ship";

  return titleCase(normalizeJoinedPresentationTokens(key));
}

function keybindingPresentationHelp(label) {
  return `Keyboard or mouse shortcut for ${label[0].toLowerCase()}${label.slice(1)}.`;
}

function notificationPresentationHelp(setting) {
  const key = setting.path.split(".").at(-1);
  const exactConsequences = new Map([
    ["fleet_arrived_at_destination", "your fleet arrives at its destination"],
    ["fleet_arrived_in_system", "your fleet arrives in a system"],
    ["fleet_started_mining", "your fleet starts mining"],
    ["fleet_node_depleted", "your fleet's mining node is depleted"],
    ["fleet_docked", "your fleet docks"],
    ["fleet_repair_complete", "fleet repairs finish"],
    ["incoming_attack_player", "another player attacks one of your assets"],
    ["incoming_attack_hostile", "a hostile attacks one of your assets"],
    ["victory", "you win a battle"],
    ["defeat", "you lose a battle"],
    ["achievement", "you earn an achievement"],
    ["armada_created", "an armada is created"],
    ["armada_canceled", "an armada is canceled"],
    ["armada_incoming_attack", "an armada attack is incoming"],
    ["armada_player_blocked", "a player is blocked from an armada"],
    ["armada_player_unblocked", "a player is unblocked from an armada"],
    ["fleet_battle", "your fleet enters battle"],
    ["station_battle", "your station enters battle"],
    ["faction_level_up", "your faction level increases"],
    ["faction_level_down", "your faction level decreases"],
    ["faction_warning", "a faction warning is issued"],
    ["chained_event_scored", "you score in a chained event"],
    ["dynamic_crisis_update", "dynamic crisis status changes"],
    ["galactic_anomaly_system_entered", "your fleet enters a galactic anomaly system"],
    ["joined_takeover", "a takeover you joined becomes active"],
    ["competitor_joined_takeover", "a competitor joins your takeover"],
    ["abandoned_territory", "your alliance abandons territory"],
    ["outpost_started_or_ended", "an outpost starts or ends"],
    ["strike_hit", "a strike hits its target"],
    ["surge_warmup_ended", "surge warmup ends"],
    ["surge_hostile_group_defeated", "a surge hostile group is defeated"],
    ["arena_time_left", "arena time is running low"],
    ["surge_time_left", "surge time is running low"],
  ]);
  const exact = exactConsequences.get(key);
  if (exact) return `Alerts you when ${exact}.`;

  const partialVictory = key.match(/^(?:(.+)_)?partial_victory$/);
  if (partialVictory) {
    const subject = partialVictory[1]
      ? `${normalizeJoinedPresentationTokens(partialVictory[1]).replaceAll("_", " ")} battle`
      : "a battle";
    return `Alerts you when ${subject} ends in a partial victory.`;
  }

  const outcome = key.match(/^(.+)_(victory|defeat)$/);
  if (outcome) {
    const subject = normalizeJoinedPresentationTokens(outcome[1]).replaceAll("_", " ");
    const result = outcome[2] === "victory" ? "win" : "lose";
    const article = /^[aeiou]/.test(subject) ? "an" : "a";
    return `Alerts you when you ${result} ${subject === "station" ? "a station battle" : `${article} ${subject}`}.`;
  }

  const armadaBattle = key.match(/^(.*armada)_battle_(won|lost)$/);
  if (armadaBattle) {
    const subject = normalizeJoinedPresentationTokens(armadaBattle[1]).replaceAll("_", " ");
    return `Alerts you when your ${subject} battle is ${armadaBattle[2]}.`;
  }

  const lifecycle = key.match(/^(.+)_(created|canceled|activated|expired|purchased|applied|updated|discovered)$/);
  if (lifecycle) {
    const subject = normalizeJoinedPresentationTokens(lifecycle[1]).replaceAll("_", " ");
    return lifecycle[2] === "expired"
      ? `Alerts you when ${subject} expires.`
      : `Alerts you when ${subject} is ${lifecycle[2]}.`;
  }

  const completion = key.match(/^(.+)_(complete|completed|failed)$/);
  if (completion) {
    const subject = normalizeJoinedPresentationTokens(completion[1]).replaceAll("_", " ");
    const auxiliary = subject.endsWith("events") ? "are" : "is";
    if (completion[2] === "failed") return `Alerts you when ${subject} fails.`;
    const state = completion[2] === "complete" ? "complete" : "completed";
    return `Alerts you when ${subject} ${auxiliary} ${state}.`;
  }

  const capacity = key.match(/^(.+)_(full|progress)$/);
  if (capacity) {
    const subject = normalizeJoinedPresentationTokens(capacity[1]).replaceAll("_", " ");
    return capacity[2] === "full"
      ? `Alerts you when ${subject} is full.`
      : `Alerts you when ${subject} progress changes.`;
  }

  return null;
}

function notificationPresentationGroup(setting) {
  const canonicalKey = setting.path.split(".").at(-1);
  const legacyAlias = setting.aliases.find((alias) => alias.path.startsWith("notifications.events."));
  const legacyCategory = legacyAlias?.path.split(".")[2];
  if (legacyCategory === "battle") return "Battle and incoming attacks";
  if (legacyCategory === "fleet") {
    return canonicalKey.includes("repair") || canonicalKey.includes("docked")
      ? "Repairs and docking"
      : "Fleet movement and mining";
  }
  if (legacyCategory === "armada") return "Armada";
  if (legacyCategory === "event") return "Events and tournaments";

  if (canonicalKey.includes("armada")) return "Armada";
  if (/(takeover|territory|outpost)/.test(canonicalKey)) return "Territory and takeover";
  if (/(treasury|warchest|queue.*(?:lease|purchased))/.test(canonicalKey)) return "Economy and treasury";
  if (/(fleet|mining|node_depleted)/.test(canonicalKey)) return "Fleet movement and mining";
  if (/(repair|docked)/.test(canonicalKey)) return "Repairs and docking";
  if (/(event|tournament|challenge|crisis|surge|arena)/.test(canonicalKey)) return "Events and tournaments";
  return "Experimental and generic toasts";
}

function presentationGroup(setting) {
  if (setting.control === "keybinding") {
    return titleCase(snakeCase(setting.valueType.actionCategory));
  }
  if (setting.control === "notification-policy") {
    return notificationPresentationGroup(setting);
  }
  return titleCase(setting.category);
}

function defaultEditorWidth(setting) {
  if (setting.control === "keybinding" || setting.valueType.kind === "string") return "wide";
  if (["boolean", "integer", "number"].includes(setting.valueType.kind)) return "compact";
  return "standard";
}

function uniqueSearchTerms(values) {
  const seen = new Set();
  const terms = [];
  for (const raw of values.flat()) {
    if (typeof raw !== "string") throw new Error("Presentation search terms must be strings.");
    const value = raw.trim();
    if (!value) throw new Error("Presentation search terms must not be blank.");
    const key = value.toLowerCase();
    if (!seen.has(key)) {
      seen.add(key);
      terms.push(value);
    }
  }
  return terms;
}

function validatePresentationOverrides(settings, overrides = configPresentationOverrides) {
  const settingsByPath = new Map(settings.map((setting) => [setting.path, setting]));
  const seenPaths = new Set();
  for (const override of overrides) {
    if (!override || typeof override !== "object" || Array.isArray(override)) {
      throw new Error("Presentation overrides must be objects.");
    }
    for (const field of Object.keys(override)) {
      if (!presentationOverrideFields.has(field)) {
        throw new Error(`Presentation override for ${override.path ?? "<unknown>"} uses unsupported field '${field}'.`);
      }
    }
    if (typeof override.path !== "string" || !override.path.trim()) {
      throw new Error("Presentation overrides must declare a non-empty canonical path.");
    }
    if (seenPaths.has(override.path)) {
      throw new Error(`Duplicate presentation override path: ${override.path}`);
    }
    seenPaths.add(override.path);
    const setting = settingsByPath.get(override.path);
    if (!setting) throw new Error(`Stale presentation override path: ${override.path}`);

    for (const field of ["label", "group", "unit"]) {
      if (field in override && (typeof override[field] !== "string" || !override[field].trim())) {
        throw new Error(`Presentation override ${override.path}.${field} must be a non-empty string.`);
      }
    }
    if ("help" in override
        && override.help !== null
        && (typeof override.help !== "string" || !override.help.trim())) {
      throw new Error(`Presentation override ${override.path}.help must be a non-empty string or null.`);
    }
    if ("editorWidth" in override && !editorWidths.has(override.editorWidth)) {
      throw new Error(`Presentation override ${override.path} uses unsupported editor width '${override.editorWidth}'.`);
    }
    if ("enumOptions" in override) {
      if (setting.valueType.kind !== "enum" || !Array.isArray(override.enumOptions)) {
        throw new Error(`Presentation override ${override.path}.enumOptions requires an enum setting and an array.`);
      }
      const declaredValues = setting.valueType.values;
      const seenValues = new Set();
      for (const option of override.enumOptions) {
        if (!option || typeof option !== "object" || Array.isArray(option)) {
          throw new Error(`Presentation override ${override.path}.enumOptions entries must be objects.`);
        }
        for (const field of Object.keys(option)) {
          if (!["value", "label", "help"].includes(field)) {
            throw new Error(`Presentation override ${override.path}.enumOptions uses unsupported field '${field}'.`);
          }
        }
        for (const field of ["value", "label"]) {
          if (typeof option[field] !== "string" || !option[field].trim()) {
            throw new Error(`Presentation override ${override.path}.enumOptions.${field} must be non-empty.`);
          }
        }
        if ("help" in option && (typeof option.help !== "string" || !option.help.trim())) {
          throw new Error(`Presentation override ${override.path}.enumOptions.help must be non-empty when present.`);
        }
        if (!declaredValues.includes(option.value) || seenValues.has(option.value)) {
          throw new Error(`Presentation override ${override.path}.enumOptions has invalid or duplicate value '${option.value}'.`);
        }
        seenValues.add(option.value);
      }
      if (seenValues.size !== declaredValues.length) {
        throw new Error(`Presentation override ${override.path}.enumOptions must cover every declared value.`);
      }
    }
    if ("searchTerms" in override) {
      if (!Array.isArray(override.searchTerms)) {
        throw new Error(`Presentation override ${override.path}.searchTerms must be an array.`);
      }
      uniqueSearchTerms(override.searchTerms);
    }
    if ("unit" in override
        && (setting.control !== "scalar" || !["integer", "number"].includes(setting.valueType.kind))) {
      throw new Error(`Presentation override ${override.path} declares a unit for a non-numeric scalar setting.`);
    }
  }
  return new Map(overrides.map((override) => [override.path, override]));
}

function addPresentationMetadata(settings, overrides = configPresentationOverrides) {
  const overridesByPath = validatePresentationOverrides(settings, overrides);
  for (const setting of settings) {
    const override = overridesByPath.get(setting.path) ?? {};
    const rawDerivedLabel = setting.control === "keybinding"
      ? keybindingPresentationLabel(setting)
      : setting.title;
    const derivedLabel = sentenceCasePresentationLabel(rawDerivedLabel);
    const label = (override.label ?? derivedLabel).trim();
    const derivedHelp = setting.control === "notification-policy"
      ? notificationPresentationHelp(setting)
      : setting.control === "keybinding"
        ? keybindingPresentationHelp(derivedLabel)
        : cleanPresentationHelp(setting.description, label);
    const helpSource = Object.hasOwn(override, "help") ? override.help : derivedHelp;
    const help = helpSource?.trim() || null;
    const group = (override.group ?? presentationGroup(setting)).trim();
    const applyTiming = applyTimingLabels.get(setting.apply);
    if (!applyTiming) throw new Error(`Setting ${setting.path} uses unsupported apply behavior '${setting.apply}'.`);
    const editorWidth = override.editorWidth ?? defaultEditorWidth(setting);
    if (!editorWidths.has(editorWidth)) {
      throw new Error(`Setting ${setting.path} uses unsupported editor width '${editorWidth}'.`);
    }

    const searchTerms = uniqueSearchTerms([
      setting.path,
      setting.title,
      setting.description,
      setting.category,
      group,
      setting.aliases.map((alias) => alias.path),
      override.searchTerms ?? [],
    ]);
    const accessibleHelp = [help, `Applies: ${applyTiming}.`].filter(Boolean).join(" ");
    setting.presentation = {
      label,
      ...(help ? { help } : {}),
      group,
      searchTerms,
      ...(override.enumOptions
        ? {
            enumOptions: override.enumOptions.map((option) => ({
              value: option.value,
              label: option.label.trim(),
              ...(option.help ? { help: option.help.trim() } : {}),
            })),
          }
        : {}),
      ...(override.unit ? { unit: override.unit.trim() } : {}),
      editorWidth,
      applyTiming,
      accessibleName: label,
      accessibleHelp,
    };
  }
  return settings;
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
  addPresentationMetadata(settings);

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
  addPresentationMetadata,
  parseDefaultConfig,
  parseBoolConfigMetadata,
  parseConfigMemberTypes,
  parseExampleConfig,
  scanCustomParserPaths,
  scanLiteralParserPaths,
  scanRuntimeScalarPaths,
  validatePresentationOverrides,
};
