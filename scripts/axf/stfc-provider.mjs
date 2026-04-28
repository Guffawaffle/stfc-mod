#!/usr/bin/env node

import { spawnSync } from "node:child_process";
import { createHash, randomUUID } from "node:crypto";
import {
  existsSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  renameSync,
  rmSync,
  statSync,
  writeFileSync
} from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";

const PROVIDER_VERSION = "0.2.0";
const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const DEFAULT_GAME_ROOT = "/mnt/c/Games/Star Trek Fleet Command/default/game";
const GAME_ROOT = process.env.STFC_GAME_ROOT || DEFAULT_GAME_ROOT;
const OLD_AX_ROOT = process.env.STFC_AX_REF_ROOT || "/mnt/d/dev/stfc-mod/.ax";
const WINDOWS_BUILD_ROOT = process.env.STFC_WINDOWS_BUILD_ROOT || "/mnt/d/dev/stfc-mod-interop";
const WINDOWS_PWSH_CANDIDATES = [
  process.env.STFC_WINDOWS_PWSH,
  "/mnt/c/Program Files/PowerShell/7/pwsh.exe",
  "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
].filter(Boolean);

const COMMANDS = {
  help: "Show available STFC AXF provider commands",
  list: "Alias of help",
  status: "One-shot health check: git, build, deploy, game, log",
  doctor: "Check Linux tooling, reference data, paths, build outputs, and config files",
  "windows-interop": "Check the Windows PowerShell bridge used for build/deploy/cycle",
  "game-running": "Best-effort WSL process check for prime.exe",
  config: "Show settings TOML or runtime vars",
  build: "Build the mod through Windows PowerShell/xmake interop",
  deploy: "Build, copy, and verify the deployed DLL through Windows interop",
  cycle: "Stop/deploy/start the game through Windows interop, then verify postflight",
  "deploy-status": "Compare built and deployed DLL hashes",
  "pure-tests": "Configure, build, and run the Linux pure decoder test suite",
  "decode-tool": "Configure and build the Linux battle-log decode helper",
  "battle-log": "Decode captured battle probe JSONL journals",
  "debug-send": "Send one live JSON debug request through the file transport",
  "live-state": "Probe named live runtime state surfaces",
  "recent-events": "Read or clear live debug recent-events recap",
  log: "Unified native log entrypoint with slice/session/errors/boot modes",
  "log-slice": "Tail/filter community_patch.log",
  "log-errors": "Show error and warning lines from the log",
  "log-boot": "Show the last boot sequence with hook install results",
  "log-session": "Show post-boot log lines from the current session",
  preflight: "Aggregate status, boot, and recent log errors",
  postflight: "Aggregate deploy status, boot, and recent log errors",
  "dump-search": "Search Il2Cpp dump.cs text",
  "dump-index": "Rebuild the SQLite dump index using the reference Python parser",
  "dump-stats": "Summary stats for the dump index",
  "dump-class": "Look up a class by name from the dump index",
  "dump-hierarchy": "Walk a class inheritance chain from the dump index",
  "dump-method": "Find methods by name, optionally scoped to a class",
  "dump-enum": "Look up an enum and optionally list its values",
  "dump-offsets": "Show field offsets for a class",
  "dump-fts": "FTS5 ranked search across classes, methods, and fields",
  investigate: "Class investigation: dump info, hierarchy, offsets, and header preview"
};

const IGNORED_LOG_ISSUES = [
  /Unable to find method 'GameObject::SetActive'/,
  /Unable to find method 'UnityEngine\.Screen->get_resolutions'/,
  /Unable to find method 'GameServerModelRegsitry->HandleBinaryObjects'/,
  /Unable to find method 'SlotDataContainer->ParseSlot(?:Updated|Removed)Json'/
];

const started = performance.now();
const { command, options } = parseCli(process.argv.slice(2));

try {
  const result = await dispatch(command, options);
  writeEnvelope(command, result);
} catch (error) {
  writeEnvelope(command, fail(error?.message ?? String(error), { exception: String(error?.stack ?? error) }));
}

async function dispatch(commandName, options) {
  switch (commandName) {
    case "help":
    case "list":
      return ok({ providerVersion: PROVIDER_VERSION, repoRoot: REPO_ROOT, commands: COMMANDS });
    case "status":
      return commandStatus(options);
    case "doctor":
      return commandDoctor();
    case "windows-interop":
      return commandWindowsInterop();
    case "game-running":
      return ok(gameProcessInfo());
    case "config":
      return commandConfig(options);
    case "build":
      return commandWindowsAx("build", options, { tail: "Tail" });
    case "deploy":
      return commandWindowsAx("deploy", options, { tail: "Tail" });
    case "cycle":
      return commandWindowsAx("cycle", options, {
        tail: "Tail",
        "stop-timeout-sec": "StopTimeoutSec",
        "start-timeout-sec": "StartTimeoutSec",
        "boot-timeout-sec": "BootTimeoutSec",
        "poll-ms": "PollMs",
        "error-last": "ErrorLast"
      });
    case "deploy-status":
      return commandDeployStatus();
    case "pure-tests":
      return commandValidation("pure-tests", options);
    case "decode-tool":
      return commandValidation("decode-tool", options);
    case "battle-log":
      return commandBattleLog(options);
    case "debug-send":
      return commandDebugSend(options);
    case "live-state":
      return commandLiveState(options);
    case "recent-events":
      return commandRecentEvents(options);
    case "log":
      return commandLog(options);
    case "log-slice":
      return commandLogSlice(options);
    case "log-errors":
      return commandLogErrors(options);
    case "log-boot":
      return commandLogBoot(options);
    case "log-session":
      return commandLogSession(options);
    case "preflight":
      return commandPreflight(options);
    case "postflight":
      return commandPostflight(options);
    case "dump-search":
      return commandDumpSearch(options);
    case "dump-index":
      return commandDumpIndex(options);
    case "dump-stats":
      return commandQueryDump("stats", [], options);
    case "dump-class":
      return commandQueryDump("class", queryArgsForName(options), options);
    case "dump-hierarchy":
      return commandQueryDump("hierarchy", queryArgsForClass(options), options);
    case "dump-method":
      return commandQueryDump("method", queryArgsForName(options, { method: true }), options);
    case "dump-enum":
      return commandQueryDump("enum", queryArgsForName(options, { values: true }), options);
    case "dump-offsets":
      return commandQueryDump("offsets", queryArgsForClass(options, { field: true }), options);
    case "dump-fts":
      return commandQueryDump("search", queryArgsForQuery(options), options);
    case "investigate":
      return commandInvestigate(options);
    default:
      return fail(`Unknown stfc-mod command: ${commandName}`, {
        command: commandName,
        availableCommands: Object.keys(COMMANDS)
      });
  }
}

function commandStatus(options) {
  const paths = getPaths();
  const dirty = gitDirtyFiles();
  const tracking = gitTracking();
  const buildExists = exists(paths.buildDll);
  const deployedExists = exists(paths.deployedDll);
  const buildHash = buildExists ? sha256(paths.buildDll) : null;
  const deployedHash = deployedExists ? sha256(paths.deployedDll) : null;
  const hashMatch = Boolean(buildHash && deployedHash && buildHash === deployedHash);
  const logInfo = logSummary(paths.logFile);

  return ok({
    ok: hashMatch && dirty.length === 0,
    repoRoot: REPO_ROOT,
    git: {
      branch: gitOutput(["rev-parse", "--abbrev-ref", "HEAD"]),
      ahead: tracking.ahead,
      behind: tracking.behind,
      dirty: dirty.length > 0,
      dirtyCount: dirty.length,
      dirtyFiles: bool(options.summary) ? null : dirty
    },
    build: {
      mode: paths.buildMode,
      dll: paths.buildDll,
      dllExists: buildExists,
      dllModified: mtime(paths.buildDll)
    },
    deploy: {
      deployedDll: paths.deployedDll,
      deployedExists,
      hashMatch
    },
    game: gameProcessInfo(),
    log: logInfo
  });
}

function commandDoctor() {
  const paths = getPaths();
  const interop = windowsInteropInfo({ probe: true });
  const checks = [];
  addToolCheck(checks, "node", "node", ["--version"], true);
  addToolCheck(checks, "python3", pythonCommand(), ["--version"], true);
  addToolCheck(checks, "cmake", "cmake", ["--version"], true);
  addToolCheck(checks, "c++", "c++", ["--version"], true);
  addToolCheck(checks, "xmake", "xmake", ["--version"], false);
  addToolCheck(checks, "axf", "axf", ["help"], false);
  addPathCheck(checks, "repo-root", REPO_ROOT, true);
  addPathCheck(checks, "linux-validation-script", join(REPO_ROOT, "scripts", "validate-linux.sh"), true);
  addPathCheck(checks, "cmake-project", join(REPO_ROOT, "CMakeLists.txt"), true);
  addPathCheck(checks, "reference-ax-root", OLD_AX_ROOT, false);
  addPathCheck(checks, "dump-db", join(OLD_AX_ROOT, "cache", "stfc.db"), false);
  addPathCheck(checks, "dump-cs", paths.dumpCs, false);
  addPathCheck(checks, "prime-headers", paths.primeRoot, false);
  addPathCheck(checks, "build-dll", paths.buildDll, false);
  addPathCheck(checks, "deployed-dll", paths.deployedDll, false);
  addPathCheck(checks, "settings", paths.settingsToml, false);
  addPathCheck(checks, "runtime-vars", paths.runtimeVars, false);
  addPathCheck(checks, "log-file", paths.logFile, false);
  for (const check of interop.checks) {
    checks.push(check);
  }

  const failedRequired = checks.filter((check) => check.required && !check.ok);
  return ok({
    ok: failedRequired.length === 0,
    providerVersion: PROVIDER_VERSION,
    repoRoot: REPO_ROOT,
    gameRoot: GAME_ROOT,
    oldAxReferenceRoot: OLD_AX_ROOT,
    windowsInterop: interop.summary,
    checks
  });
}

function commandWindowsInterop() {
  const interop = windowsInteropInfo({ probe: true });
  return interop.ok ? ok(interop) : fail("Windows interop is not available.", interop);
}

function commandConfig(options) {
  const paths = getPaths();
  const source = bool(options["runtime-vars"]) ? "runtime-vars" : "settings";
  const targetPath = source === "runtime-vars" ? paths.runtimeVars : paths.settingsToml;
  if (!exists(targetPath)) {
    return fail("Config file not found.", { source, path: targetPath });
  }

  let content = readText(targetPath);
  if (bool(options.compact)) {
    content = content
      .split(/\r?\n/)
      .filter((line) => !line.trim().startsWith("#") && line.trim() !== "")
      .join("\n");
  }

  return ok({
    source,
    path: targetPath,
    lastWriteTimeUtc: mtime(targetPath),
    content
  });
}

function commandDeployStatus() {
  const paths = getPaths();
  const buildExists = exists(paths.buildDll);
  const deployedExists = exists(paths.deployedDll);
  const buildHash = buildExists ? sha256(paths.buildDll) : null;
  const deployedHash = deployedExists ? sha256(paths.deployedDll) : null;
  const hashMatch = Boolean(buildHash && deployedHash && buildHash === deployedHash);
  const data = {
    repoRoot: REPO_ROOT,
    buildDll: paths.buildDll,
    deployedDll: paths.deployedDll,
    buildExists,
    deployedExists,
    buildHash,
    deployedHash,
    hashMatch,
    buildWriteTimeUtc: mtime(paths.buildDll),
    deployedWriteTimeUtc: mtime(paths.deployedDll)
  };
  return hashMatch ? ok(data) : fail("Built and deployed DLL hashes do not match.", data);
}

function commandWindowsAx(axCommand, options, argMap = {}) {
  const interop = windowsInteropInfo({ probe: false });
  if (!interop.ok) {
    return fail("Windows interop is not available.", interop);
  }
  const shouldSync = axCommand !== "game-running";
  const sync = shouldSync ? syncWindowsBuildRoot() : ok({ skipped: true, reason: "command does not use repo build files" });
  if (!sync.ok) {
    return fail("Failed to sync WSL repo to the Windows build mirror.", {
      interop: interop.summary,
      sync: sync.data
    });
  }
  const xmakePrep = shouldSync ? prepareWindowsXmake(interop) : ok({ skipped: true, reason: "command does not build" });
  if (!xmakePrep.ok) {
    return fail("Failed to prepare Windows xmake configuration.", {
      interop: interop.summary,
      sync: sync.data,
      xmakePrep: xmakePrep.data
    });
  }

  const axArgs = [axCommand, ...windowsAxArgs(options, argMap)];
  const psCommand = [
    "$ErrorActionPreference = 'Stop'",
    `$env:AX_REPO_ROOT = '${escapePowerShellSingleQuoted(interop.paths.repoRootWin)}'`,
    `& '${escapePowerShellSingleQuoted(interop.paths.axScriptWin)}' ${axArgs.map(powerShellSingleQuoted).join(" ")}`,
    "exit $LASTEXITCODE"
  ].join("; ");
  const args = [
    "-NoLogo",
    "-NoProfile",
    "-NonInteractive",
    "-Command",
    psCommand
  ];
  const result = runProcess(interop.paths.pwsh, args, {
    cwd: REPO_ROOT,
    maxBuffer: 128 * 1024 * 1024
  });
  const parsed = parseJsonMaybe(result.stdout);
  const data = {
    interop: interop.summary,
    sync: sync.data,
    xmakePrep: xmakePrep.data,
    windowsCommand: axCommand,
    exitCode: result.exitCode,
    signal: result.signal,
    error: result.error,
    stdoutTail: tailLines(result.stdout, intOpt(options.tail, 40)),
    stderrTail: tailLines(result.stderr, intOpt(options.tail, 40)),
    result: parsed
  };
  const scriptOk = result.exitCode === 0 && !(parsed && typeof parsed === "object" && parsed.ok === false);
  return scriptOk ? ok(parsed) : fail(`Windows .ax ${axCommand} failed.`, data);
}

function commandValidation(validationCommand, options) {
  const script = join(REPO_ROOT, "scripts", "validate-linux.sh");
  if (!exists(script)) {
    return fail("Linux validation wrapper is missing.", { script });
  }
  const tail = intOpt(options.tail, 30);
  const result = runProcess(script, [validationCommand], { cwd: REPO_ROOT });
  const data = processData(result, tail);
  return result.exitCode === 0
    ? ok(data)
    : fail(`${validationCommand} failed with exit code ${result.exitCode}.`, data);
}

function commandBattleLog(options) {
  const paths = getPaths();
  const probePath = stringOpt(options["probe-path"]) || join(GAME_ROOT, "community_patch_battle_probe.jsonl");
  if (!exists(probePath)) {
    return fail("Probe file not found.", { phase: "probe-file", probePath });
  }

  if (!bool(options["no-build"])) {
    const built = commandValidation("decode-tool", options);
    if (!built.ok) {
      return fail("Failed to build battle-log decode tool.", { phase: "build", build: built.data }, built.hints);
    }
  }

  const tool = join(REPO_ROOT, "build", "linux-validation", "battle-log-decode");
  if (!exists(tool)) {
    return fail("battle-log-decode not found.", {
      phase: "tool-exe",
      tool,
      hint: "Run: axf run stfc-mod decode-tool --json"
    });
  }

  const args = ["--probe", probePath];
  const compare = splitList(options.compare);
  if (compare.length > 0) {
    if (compare.length !== 2) {
      return fail("--compare requires exactly two journal ids.", { compare });
    }
    args.push("--compare", compare[0], compare[1]);
  } else if (stringOpt(options["journal-id"])) {
    args.push("--journal-id", stringOpt(options["journal-id"]));
  } else {
    args.push("--latest");
  }

  for (const battleType of splitList(options["battle-type"])) {
    args.push("--battle-type", battleType);
  }
  if (bool(options.segments)) args.push("--segments");
  if (bool(options.feed)) args.push("--sidecar-feed");
  if (bool(options["summary-only"])) args.push("--summary-only");
  const segmentLimit = intOpt(options["segment-limit"], 0);
  if (segmentLimit > 0) args.push("--segment-limit", String(segmentLimit));
  if (bool(options.pretty)) args.push("--pretty");

  const run = runProcess(tool, args, { cwd: REPO_ROOT, maxBuffer: 128 * 1024 * 1024 });
  const parsed = parseJsonMaybe(run.stdout);
  const data = {
    phase: "run",
    tool,
    probePath,
    exitCode: run.exitCode,
    stderrTail: tailLines(run.stderr, intOpt(options.tail, 30)),
    result: parsed
  };
  const resultOk = run.exitCode === 0 && !(parsed && typeof parsed === "object" && parsed.ok === false);
  return resultOk ? ok(parsed) : fail("battle-log decode failed.", data);
}

async function commandDebugSend(options) {
  const cmd = stringOpt(options.cmd);
  const requestJson = stringOpt(options["request-json"]);
  if (!cmd && !requestJson) {
    return fail("Specify --cmd <name> or --request-json <json>.");
  }
  let request;
  if (requestJson) {
    try {
      request = JSON.parse(requestJson);
    } catch (error) {
      return fail(`Failed to parse --request-json: ${error.message}`);
    }
  } else {
    request = { cmd };
  }
  const response = await sendDebugRequest(request, options);
  return response.ok === false
    ? fail(response.error ?? response.phase ?? "debug request failed", response)
    : ok(response);
}

async function commandLiveState(options) {
  const view = (stringOpt(options.view) || "fleetbar").trim().toLowerCase();
  const commandMap = {
    ping: "ping",
    tracker: "tracker-list",
    "tracker-list": "tracker-list",
    top: "top-canvas",
    "top-canvas": "top-canvas",
    fleetbar: "fleetbar-state",
    "fleetbar-state": "fleetbar-state",
    "fleet-slots": "fleet-slots-state",
    "fleet-slots-state": "fleet-slots-state",
    slots: "fleet-slots-state",
    mine: "mine-viewer-state",
    "mine-viewer": "mine-viewer-state",
    "mine-viewer-state": "mine-viewer-state",
    target: "target-viewer-state",
    "target-viewer": "target-viewer-state",
    "target-viewer-state": "target-viewer-state"
  };
  const debugCommand = commandMap[view];
  if (!debugCommand) {
    return fail("unknown view", {
      view,
      supportedViews: ["ping", "tracker", "top-canvas", "fleetbar", "fleet-slots", "mine-viewer", "target-viewer"]
    });
  }

  const response = await sendDebugRequest({ cmd: debugCommand }, options);
  if (response.ok === false) {
    return fail(response.error ?? response.phase ?? "live-state request failed", response);
  }

  if (debugCommand !== "tracker-list") {
    return ok({ view: debugCommand, result: response.result });
  }

  const result = response.result ?? {};
  let classes = Array.isArray(result.classes) ? result.classes : [];
  classes = classes.filter((entry) => liveStateClassMatches(entry, options));
  const matchedClassCount = classes.length;
  const top = Math.max(0, intOpt(options.top, 20));
  if (top > 0) classes = classes.slice(0, top);
  const namesOnly = bool(options["names-only"]);
  const summary = bool(options.summary);
  const outputClasses = summary ? null : namesOnly ? classes.map((entry) => entry.fullName ?? entry.className) : classes;

  return ok({
    view: "tracker-list",
    trackedClassCount: Number(result.trackedClassCount ?? 0),
    trackedObjectCount: Number(result.trackedObjectCount ?? 0),
    matchedClassCount,
    returnedClassCount: classes.length,
    filters: {
      match: stringOpt(options.match) || null,
      namespace: stringOpt(options.namespace) || null,
      className: stringOpt(options["class-name"]) || null,
      pointer: stringOpt(options.pointer) || null,
      minCount: intOpt(options["min-count"], -1) < 0 ? null : intOpt(options["min-count"], -1),
      maxCount: intOpt(options["max-count"], -1) < 0 ? null : intOpt(options["max-count"], -1),
      exact: bool(options.exact),
      top,
      namesOnly
    },
    classes: outputClasses
  });
}

async function commandRecentEvents(options) {
  const clear = bool(options.clear);
  const request = { cmd: clear ? "clear-recent-events" : "recent-events" };
  if (!clear) {
    const afterSeq = intOpt(options["after-seq"], -1);
    if (afterSeq >= 0) request.afterSeq = afterSeq;
    const kinds = splitList(options.kind);
    if (kinds.length === 1) request.kind = kinds[0];
    if (kinds.length > 1) request.kinds = kinds;
    if (stringOpt(options.match)) {
      request.match = stringOpt(options.match);
      if (bool(options.exact)) request.exact = true;
    }
    const last = intOpt(options.last, 50);
    if (last > 0) request.last = last;
    if (bool(options.summary) && !request.match) request.summary = true;
  }

  const response = await sendDebugRequest(request, options);
  return response.ok === false
    ? fail(response.error ?? response.phase ?? "recent-events request failed", response)
    : ok(response);
}

function commandLog(options) {
  const selected = ["session", "errors", "boot"].filter((key) => bool(options[key]));
  if (selected.length > 1) {
    return fail("cannot combine --session, --errors, and --boot");
  }
  if (bool(options.summary) && !bool(options.boot)) {
    return fail("cannot use --summary without --boot");
  }
  if (bool(options.boot)) return commandLogBoot(options);
  if (bool(options.errors)) return commandLogErrors({ ...options, last: options.tail ?? options.last });
  if (bool(options.session)) return commandLogSession({ ...options, last: options.tail ?? options.last });
  return commandLogSlice(options);
}

function commandLogSlice(options) {
  const paths = getPaths();
  const log = readLogLines(paths.logFile);
  if (!log.ok) return log;
  const last = intOpt(options.tail ?? options.last, 80);
  const matcher = buildMatcher(options.pattern, bool(options["case-sensitive"]));
  const matches = [];
  for (let i = 0; i < log.lines.length; i += 1) {
    const line = log.lines[i];
    if (!matcher || matcher(line)) {
      matches.push({ lineNumber: i + 1, text: formatLogLine(line, bool(options.raw)) });
    }
  }
  const sliced = matches.slice(-Math.max(1, last));
  if (bool(options["text-only"])) {
    return ok(sliced.map((line) => line.text).join("\n"));
  }
  return ok({ path: paths.logFile, totalLines: log.lines.length, returnedCount: sliced.length, lines: sliced });
}

function commandLogErrors(options) {
  const paths = getPaths();
  const log = readLogLines(paths.logFile);
  if (!log.ok) return log;
  const scanAll = bool(options["scan-all"]);
  const last = intOpt(options.last, 200);
  const start = scanAll ? 0 : Math.max(0, log.lines.length - Math.max(1, last));
  const formatted = [];
  for (let i = start; i < log.lines.length; i += 1) {
    const text = log.lines[i];
    const match = text.match(/\[(error|warn)[^\]]*\]/i);
    if (!match) continue;
    formatted.push({
      lineNumber: i + 1,
      level: match[1].toLowerCase(),
      ignored: isIgnoredLogIssue(text),
      text: formatLogLine(text, bool(options.raw), true)
    });
  }
  const actionable = formatted.filter((line) => !line.ignored);
  if (bool(options["text-only"])) {
    return ok(actionable.map((line) => line.text).join("\n"));
  }
  const errorLines = actionable.filter((line) => line.level === "error");
  const warningLines = actionable.filter((line) => line.level === "warn");
  return ok({
    path: paths.logFile,
    scannedLast: scanAll ? log.lines.length : last,
    totalCount: actionable.length,
    errorCount: errorLines.length,
    warningCount: warningLines.length,
    ignoredCount: formatted.length - actionable.length,
    ignoredLines: formatted.filter((line) => line.ignored),
    lines: actionable
  });
}

function commandLogBoot(options) {
  const paths = getPaths();
  const log = readLogLines(paths.logFile);
  if (!log.ok) return log;
  let bootStart = -1;
  for (let i = log.lines.length - 1; i >= 0; i -= 1) {
    if (/Initializing STFC Community Patch/.test(log.lines[i])) {
      bootStart = i;
      break;
    }
  }
  if (bootStart < 0) {
    return fail("No boot sequence found in log.", { path: paths.logFile });
  }
  let bootEnd = log.lines.length - 1;
  for (let i = bootStart; i < log.lines.length; i += 1) {
    if (/Installed .+ version/.test(log.lines[i])) {
      for (let j = i + 1; j < Math.min(i + 5, log.lines.length); j += 1) {
        if (/=-=-=/.test(log.lines[j])) {
          bootEnd = j;
          break;
        }
      }
      break;
    }
  }
  const bootLines = log.lines.slice(bootStart, bootEnd + 1);
  const hooks = [];
  const bootErrors = [];
  let version = null;
  const patchRegex = /([+x])\s+(Patch|Skipp)ing\s+(\d+)\s+of\s+(\d+)\s+\(([^)]+)\)/;
  for (let i = 0; i < bootLines.length; i += 1) {
    const line = bootLines[i];
    const hook = line.match(patchRegex);
    if (hook) {
      hooks.push({
        index: Number(hook[3]),
        total: Number(hook[4]),
        name: hook[5],
        action: hook[1] === "+" ? "installed" : "skipped"
      });
    }
    if (/\[(error|warn)/i.test(line)) {
      bootErrors.push({ lineNumber: bootStart + i + 1, text: formatLogLine(line, bool(options.raw), true) });
    }
    const versionMatch = line.match(/Installed\s+(release|beta)\s+version\s+(.+)/);
    if (versionMatch) version = versionMatch[2].trim();
  }
  if (bool(options["text-only"])) {
    return ok(bootLines.map((line) => formatLogLine(line, bool(options.raw))).join("\n"));
  }
  const summary = bool(options.summary);
  return ok({
    path: paths.logFile,
    version,
    hookCount: hooks.length,
    installed: hooks.filter((hook) => hook.action === "installed").length,
    skipped: hooks.filter((hook) => hook.action === "skipped").length,
    hooks: summary ? null : hooks,
    bootErrors,
    bootLines: summary
      ? null
      : bootLines.map((line, index) => ({ lineNumber: bootStart + index + 1, text: formatLogLine(line, bool(options.raw)) }))
  });
}

function commandLogSession(options) {
  const paths = getPaths();
  const log = readLogLines(paths.logFile);
  if (!log.ok) return log;
  let sessionStart = log.lines.length;
  for (let i = log.lines.length - 1; i >= 0; i -= 1) {
    if (/Installed .+ version/.test(log.lines[i])) {
      sessionStart = i + 1;
      for (let j = i + 1; j < Math.min(i + 5, log.lines.length); j += 1) {
        if (/=-=-=/.test(log.lines[j])) {
          sessionStart = j + 1;
          break;
        }
      }
      break;
    }
  }
  let lines = sessionStart < log.lines.length ? log.lines.slice(sessionStart).filter((line) => line !== "") : [];
  const matcher = buildMatcher(options.pattern, false);
  if (matcher) lines = lines.filter((line) => matcher(line));
  lines = lines.slice(-Math.max(1, intOpt(options.last, 50)));
  const formatted = lines.map((line) => formatLogLine(line, bool(options.raw)));
  if (bool(options["text-only"])) return ok(formatted.join("\n"));
  return ok({ totalLines: formatted.length, lines: formatted });
}

function commandPreflight(options) {
  const status = commandStatus({ summary: options.summary }).data;
  const boot = commandLogBoot({ summary: true });
  const errors = commandLogErrors({ last: 10 });
  return ok({
    ok: Boolean(status.ok),
    status,
    boot: boot.ok ? compactBoot(boot.data) : boot,
    recentErrors: errors.ok ? errors.data : errors
  });
}

function commandPostflight(options) {
  const deploy = commandDeployStatus();
  const boot = commandLogBoot({ summary: bool(options.summary) });
  const errors = commandLogErrors({ last: 10 });
  const errorCount = errors.ok ? errors.data.errorCount : null;
  return ok({
    ok: Boolean(deploy.ok && boot.ok),
    deploy: deploy.ok
      ? { ok: true, hashMatch: deploy.data.hashMatch }
      : { ok: false, error: deploy.error, data: deploy.data },
    boot: boot.ok ? compactBoot(boot.data) : boot,
    logHealthy: errorCount === 0,
    warningCount: errors.ok ? errors.data.warningCount : null,
    errorCount,
    recentErrors: errors.ok ? errors.data : errors
  });
}

function commandDumpSearch(options) {
  const paths = getPaths();
  const pattern = stringOpt(options.pattern);
  if (!pattern) return fail("--pattern is required.");
  if (!exists(paths.dumpCs)) {
    return fail("dump.cs not found. Run Il2CppDumper first.", { path: paths.dumpCs });
  }
  const context = Math.max(0, intOpt(options.context, 3));
  const maxResults = Math.max(1, intOpt(options["max-results"], 30));
  const caseSensitive = bool(options["case-sensitive"]);
  const matcher = buildMatcher(pattern, caseSensitive);
  const lines = readText(paths.dumpCs).split(/\r?\n/);
  const matches = [];
  for (let i = 0; i < lines.length && matches.length < maxResults; i += 1) {
    if (matcher(lines[i])) {
      matches.push({
        lineNumber: i + 1,
        match: lines[i].trim(),
        pre: lines.slice(Math.max(0, i - context), i).map((line) => line.trim()),
        post: lines.slice(i + 1, Math.min(lines.length, i + context + 1)).map((line) => line.trim())
      });
    }
  }
  return ok({
    path: paths.dumpCs,
    pattern,
    matchCount: matches.length,
    capped: matches.length >= maxResults,
    matches
  });
}

function commandDumpIndex(options) {
  const script = join(OLD_AX_ROOT, "build_dump_index.py");
  if (!exists(script)) {
    return fail("Reference build_dump_index.py not found.", { script });
  }
  const dumpPath = stringOpt(options["dump-path"]) || getPaths().dumpCs;
  const dbPath = stringOpt(options["db-path"]) || join(OLD_AX_ROOT, "cache", "stfc.db");
  const result = runProcess(pythonCommand(), [script, "--dump", dumpPath, "--db", dbPath], {
    cwd: REPO_ROOT,
    maxBuffer: 128 * 1024 * 1024
  });
  const data = processData(result, intOpt(options.tail, 40));
  data.dumpPath = dumpPath;
  data.dbPath = dbPath;
  return result.exitCode === 0 ? ok(data) : fail("dump-index failed.", data);
}

function commandQueryDump(queryCommand, queryArgs, options) {
  if (queryArgs.ok === false) return queryArgs;
  const script = join(OLD_AX_ROOT, "query_dump.py");
  if (!exists(script)) {
    return fail("Reference query_dump.py not found.", { script });
  }
  const args = [script, queryCommand, ...(Array.isArray(queryArgs) ? queryArgs : [])];
  if (queryCommand === "stats" && options["max-namespaces"]) {
    args.push("--max-namespaces", String(intOpt(options["max-namespaces"], 15)));
  }
  const result = runProcess(pythonCommand(), args, { cwd: REPO_ROOT, maxBuffer: 64 * 1024 * 1024 });
  const parsed = parseJsonMaybe(result.stdout);
  const data = {
    queryCommand,
    referenceScript: script,
    referenceDb: join(OLD_AX_ROOT, "cache", "stfc.db"),
    exitCode: result.exitCode,
    stderrTail: tailLines(result.stderr, intOpt(options.tail, 30)),
    result: parsed
  };
  return result.exitCode === 0 ? ok(parsed) : fail("dump query failed.", data);
}

function commandInvestigate(options) {
  const className = stringOpt(options.class);
  if (!className) return fail("--class is required.");
  const maxResults = intOpt(options["max-results"], 5);
  const exact = bool(options.exact);
  const classData = commandQueryDump("class", [className, "--max", String(maxResults), "--members", ...(exact ? ["--exact"] : [])], {});
  const hierarchyData = commandQueryDump("hierarchy", [className, "--max", String(maxResults), ...(exact ? ["--exact"] : [])], {});
  const offsetData = commandQueryDump("offsets", [className, "--max", "200", ...(exact ? ["--exact"] : [])], {});
  const headerMatches = primeHeaderMatches(className, maxResults);
  const headerPreview = headerMatches.length > 0
    ? filePreview(headerMatches[0].path, intOpt(options["header-preview-lines"], 160))
    : null;
  const classes = classData.ok ? classData.data.classes ?? [] : [];
  const primaryClass = classes[0] ?? null;
  let offsets = [];
  if (offsetData.ok && Array.isArray(offsetData.data.classes)) {
    const matching = offsetData.data.classes.find((entry) => primaryClass && entry.name === primaryClass.name) ?? offsetData.data.classes[0];
    offsets = matching?.fields ?? [];
  }
  const data = {
    query: className,
    classCount: classData.ok ? classData.data.count : 0,
    classes,
    hierarchy: hierarchyData.ok ? hierarchyData.data.classes?.[0]?.hierarchy ?? null : null,
    offsets,
    headerMatches,
    headerPreview,
    queryErrors: [classData, hierarchyData, offsetData].filter((entry) => !entry.ok).map((entry) => entry.error)
  };
  return data.classCount > 0 || headerMatches.length > 0 ? ok(data) : fail("No class or header matches found.", data);
}

function queryArgsForName(options, settings = {}) {
  const name = stringOpt(options.name);
  if (!name) return fail("--name is required.");
  const args = [name, "--max", String(intOpt(options["max-results"], settings.method ? 30 : 20))];
  if (bool(options.exact)) args.push("--exact");
  if (bool(options.fts)) args.push("--fts");
  if (bool(options.members)) args.push("--members");
  if (settings.values && bool(options.values)) args.push("--values");
  if (settings.method && stringOpt(options.class)) args.push("--class", stringOpt(options.class));
  return args;
}

function queryArgsForClass(options, settings = {}) {
  const className = stringOpt(options.class);
  if (!className) return fail("--class is required.");
  const args = [className, "--max", String(intOpt(options["max-results"], settings.field ? 50 : 10))];
  if (bool(options.exact)) args.push("--exact");
  if (settings.field && stringOpt(options.field)) args.push("--field", stringOpt(options.field));
  return args;
}

function queryArgsForQuery(options) {
  const query = stringOpt(options.query);
  if (!query) return fail("--query is required.");
  return [query, "--max", String(intOpt(options["max-results"], 10))];
}

async function sendDebugRequest(request, options) {
  const paths = getPaths();
  const timeoutSec = intOpt(options["timeout-sec"], 10);
  const pollMs = intOpt(options["poll-ms"], 200);
  const requestObject = { ...request, id: request.id || randomUUID() };
  const requestText = JSON.stringify(requestObject);
  const tempPath = `${paths.debugCmdFile}.tmp`;

  rmQuiet(paths.debugOutFile);
  rmQuiet(paths.debugCmdFile);
  rmQuiet(tempPath);
  mkdirSync(dirname(paths.debugCmdFile), { recursive: true });
  writeFileSync(tempPath, requestText, { encoding: "utf8" });
  renameSync(tempPath, paths.debugCmdFile);

  const deadline = Date.now() + timeoutSec * 1000;
  while (Date.now() < deadline) {
    if (exists(paths.debugOutFile)) {
      try {
        const response = JSON.parse(stringifyWideJsonIntegers(readText(paths.debugOutFile)));
        rmQuiet(paths.debugOutFile);
        if (response.id && String(response.id) !== String(requestObject.id)) {
          return {
            ok: false,
            phase: "response-mismatch",
            requestId: requestObject.id,
            responseId: response.id,
            request: requestObject,
            responseRaw: response
          };
        }
        return response;
      } catch {
        await sleep(pollMs);
      }
    } else {
      await sleep(pollMs);
    }
  }

  return {
    ok: false,
    phase: "wait-for-response",
    requestId: requestObject.id,
    timeoutSec,
    request: requestObject,
    cmdFile: paths.debugCmdFile,
    outFile: paths.debugOutFile,
    hint: "Timed out waiting for the game to answer. Ensure prime.exe is running and [debug].live_query = true."
  };
}

function getPaths() {
  const build = findBuildDll();
  return {
    repoRoot: REPO_ROOT,
    oldAxRoot: OLD_AX_ROOT,
    gameRoot: GAME_ROOT,
    buildBase: join(REPO_ROOT, "build", "windows", "x64"),
    buildDll: build.path,
    buildMode: build.mode,
    deployedDll: join(GAME_ROOT, "version.dll"),
    gameExe: join(GAME_ROOT, "prime.exe"),
    logFile: join(GAME_ROOT, "community_patch.log"),
    settingsToml: join(GAME_ROOT, "community_patch_settings.toml"),
    runtimeVars: join(GAME_ROOT, "community_patch_runtime.vars"),
    debugCmdFile: join(GAME_ROOT, "community_patch_debug.cmd"),
    debugOutFile: join(GAME_ROOT, "community_patch_debug.out"),
    battleProbe: join(GAME_ROOT, "community_patch_battle_probe.jsonl"),
    dumpCs: join(REPO_ROOT, "tools", "il2cpp-dump", "dump.cs"),
    primeRoot: join(REPO_ROOT, "mods", "src", "prime")
  };
}

function findBuildDll() {
  if (process.env.STFC_BUILD_DLL) {
    return { path: process.env.STFC_BUILD_DLL, mode: "env" };
  }
  const buildBase = join(REPO_ROOT, "build", "windows", "x64");
  let best = null;
  for (const mode of ["releasedbg", "release", "debug"]) {
    const candidate = join(buildBase, mode, "stfc-community-patch.dll");
    if (!exists(candidate)) continue;
    const item = statSync(candidate);
    if (!best || item.mtimeMs > best.mtimeMs) {
      best = { path: candidate, mode, mtimeMs: item.mtimeMs };
    }
  }
  return best ?? { path: join(buildBase, "releasedbg", "stfc-community-patch.dll"), mode: null };
}

function runProcess(commandPath, args = [], options = {}) {
  const result = spawnSync(commandPath, args, {
    cwd: options.cwd ?? REPO_ROOT,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
    env: options.env ?? process.env,
    maxBuffer: options.maxBuffer ?? 32 * 1024 * 1024
  });
  return {
    filePath: commandPath,
    argumentList: args,
    workingDirectory: options.cwd ?? REPO_ROOT,
    exitCode: result.status,
    signal: result.signal,
    stdout: result.stdout ?? "",
    stderr: result.stderr ?? "",
    error: result.error ? result.error.message : null
  };
}

function processData(result, tail = 30) {
  return {
    filePath: result.filePath,
    argumentList: result.argumentList,
    workingDirectory: result.workingDirectory,
    exitCode: result.exitCode,
    signal: result.signal,
    error: result.error,
    stdoutTail: tailLines(result.stdout, tail),
    stderrTail: tailLines(result.stderr, tail)
  };
}

function commandExists(name) {
  return spawnSync("sh", ["-lc", `command -v ${shellQuote(name)}`], {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"]
  }).status === 0;
}

function pythonCommand() {
  return process.env.STFC_PYTHON || process.env.AX_PYTHON || "python3";
}

function addToolCheck(checks, name, commandPath, args, required) {
  const result = runProcess(commandPath, args, { cwd: REPO_ROOT });
  const available = result.exitCode === 0;
  checks.push({
    name,
    ok: available,
    required,
    status: available ? "ok" : required ? "error" : "missing",
    detail: available ? firstLine(result.stdout || result.stderr) : result.error || firstLine(result.stderr) || `${commandPath} unavailable`
  });
}

function addPathCheck(checks, name, targetPath, required) {
  const okPath = exists(targetPath);
  const detail = okPath && statSync(targetPath).isFile()
    ? `${targetPath} (${Math.round(statSync(targetPath).size / 1024 / 102.4) / 10} MB)`
    : targetPath;
  checks.push({ name, ok: okPath, required, status: okPath ? "ok" : required ? "error" : "missing", detail });
}

function windowsInteropInfo({ probe = false } = {}) {
  const pwsh = findWindowsPowershell();
  const sourceRepoRootWin = toWindowsPath(REPO_ROOT);
  const repoRootWin = toWindowsPath(WINDOWS_BUILD_ROOT);
  const axScript = join(OLD_AX_ROOT, "ax.ps1");
  const axScriptWin = toWindowsPath(axScript);
  const checks = [];

  checks.push({
    name: "windows-pwsh",
    ok: Boolean(pwsh),
    required: false,
    status: pwsh ? "ok" : "missing",
    detail: pwsh ?? "No Windows PowerShell executable found under /mnt/c"
  });
  checks.push({
    name: "windows-source-repo-root",
    ok: Boolean(sourceRepoRootWin),
    required: false,
    status: sourceRepoRootWin ? "ok" : "missing",
    detail: sourceRepoRootWin ?? `Unable to convert ${REPO_ROOT} with wslpath`
  });
  checks.push({
    name: "windows-build-root",
    ok: Boolean(repoRootWin),
    required: false,
    status: repoRootWin ? (exists(WINDOWS_BUILD_ROOT) ? "ok" : "not-created") : "missing",
    detail: repoRootWin ?? `Unable to convert ${WINDOWS_BUILD_ROOT} with wslpath`
  });
  checks.push({
    name: "windows-ax-script",
    ok: Boolean(axScriptWin && exists(axScript)),
    required: false,
    status: axScriptWin && exists(axScript) ? "ok" : "missing",
    detail: axScriptWin ?? axScript
  });

  const summary = {
    available: Boolean(pwsh && repoRootWin && axScriptWin && exists(axScript)),
    pwsh,
    sourceRepoRootWin,
    repoRootWin,
    buildMirrorRoot: WINDOWS_BUILD_ROOT,
    axScriptWin,
    sourceAxRoot: OLD_AX_ROOT
  };

  if (probe && summary.available) {
    const xmake = runWindowsPowershellInline(
      pwsh,
      "(Get-Command xmake -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source)",
      { AX_REPO_ROOT: repoRootWin }
    );
    const repoVisible = runWindowsPowershellInline(
      pwsh,
      `[pscustomobject]@{ repo = (Test-Path '${escapePowerShellSingleQuoted(repoRootWin)}'); xmakeLua = (Test-Path '${escapePowerShellSingleQuoted(repoRootWin)}\\xmake.lua') } | ConvertTo-Json -Compress`,
      { AX_REPO_ROOT: repoRootWin }
    );
    const game = runWindowsPowershellInline(
      pwsh,
      "Get-Process -Name 'prime*' -ErrorAction SilentlyContinue | Select-Object -First 1 Id,ProcessName | ConvertTo-Json -Compress",
      { AX_REPO_ROOT: repoRootWin }
    );
    const repoParsed = parseJsonMaybe(repoVisible.stdout);
    const gameParsed = parseJsonMaybe(game.stdout);
    summary.xmake = xmake.exitCode === 0 ? firstLine(xmake.stdout) || null : null;
    summary.repoVisible = repoParsed?.repo === true;
    summary.xmakeLuaVisible = repoParsed?.xmakeLua === true;
    summary.gameRunning = Boolean(gameParsed && typeof gameParsed === "object" && gameParsed.Id);
    checks.push({
      name: "windows-xmake",
      ok: Boolean(summary.xmake),
      required: false,
      status: summary.xmake ? "ok" : "missing",
      detail: summary.xmake ?? firstLine(xmake.stderr) ?? "xmake not visible to Windows PowerShell"
    });
    checks.push({
      name: "windows-repo-visible",
      ok: summary.repoVisible && summary.xmakeLuaVisible,
      required: false,
      status: summary.repoVisible && summary.xmakeLuaVisible ? "ok" : "missing",
      detail: JSON.stringify(repoParsed)
    });
    checks.push({
      name: "windows-game-process",
      ok: true,
      required: false,
      status: summary.gameRunning ? "running" : "not-running",
      detail: gameParsed && typeof gameParsed === "object" ? JSON.stringify(gameParsed) : "prime.exe not running"
    });
  }

  return {
    ok: summary.available,
    summary,
    paths: { pwsh, repoRootWin, axScriptWin },
    checks
  };
}

function syncWindowsBuildRoot() {
  mkdirSync(WINDOWS_BUILD_ROOT, { recursive: true });
  const excludes = [
    ".git/",
    ".xmake/",
    "build/",
    "node_modules/",
    ".cache/",
    ".DS_Store"
  ];
  const args = [
    "-a",
    "--delete",
    ...excludes.flatMap((entry) => ["--exclude", entry]),
    `${REPO_ROOT}/`,
    `${WINDOWS_BUILD_ROOT}/`
  ];
  const result = runProcess("rsync", args, {
    cwd: REPO_ROOT,
    maxBuffer: 64 * 1024 * 1024
  });
  const data = {
    source: REPO_ROOT,
    destination: WINDOWS_BUILD_ROOT,
    destinationWin: toWindowsPath(WINDOWS_BUILD_ROOT),
    exitCode: result.exitCode,
    stdoutTail: tailLines(result.stdout, 40),
    stderrTail: tailLines(result.stderr, 40)
  };
  return result.exitCode === 0 ? ok(data) : fail("rsync failed.", data);
}

function prepareWindowsXmake(interop) {
  const mode = process.env.STFC_XMAKE_MODE || "release";
  const commandText = [
    "$ErrorActionPreference = 'Stop'",
    `Set-Location '${escapePowerShellSingleQuoted(interop.paths.repoRootWin)}'`,
    `& xmake f -y -m ${powerShellSingleQuoted(mode)}`,
    "exit $LASTEXITCODE"
  ].join("; ");
  const result = runProcess(
    interop.paths.pwsh,
    ["-NoLogo", "-NoProfile", "-NonInteractive", "-Command", commandText],
    {
      cwd: REPO_ROOT,
      maxBuffer: 128 * 1024 * 1024
    }
  );
  const data = {
    mode,
    repoRootWin: interop.paths.repoRootWin,
    exitCode: result.exitCode,
    stdoutTail: tailLines(result.stdout, 80),
    stderrTail: tailLines(result.stderr, 80)
  };
  return result.exitCode === 0 ? ok(data) : fail("xmake configure failed.", data);
}

function findWindowsPowershell() {
  return WINDOWS_PWSH_CANDIDATES.find((candidate) => exists(candidate)) ?? null;
}

function toWindowsPath(filePath) {
  const result = runProcess("wslpath", ["-w", filePath]);
  return result.exitCode === 0 ? result.stdout.trim() : null;
}

function runWindowsPowershellInline(pwsh, commandText, extraEnv = {}) {
  return runProcess(
    pwsh,
    ["-NoLogo", "-NoProfile", "-NonInteractive", "-Command", commandText],
    {
      cwd: REPO_ROOT,
      env: { ...process.env, ...extraEnv },
      maxBuffer: 16 * 1024 * 1024
    }
  );
}

function windowsAxArgs(options, argMap) {
  const args = [];
  for (const [optionName, psName] of Object.entries(argMap)) {
    const value = options[optionName];
    if (value === undefined || value === null || value === false) continue;
    args.push(`-${psName}`);
    if (value !== true) {
      args.push(String(value));
    }
  }
  return args;
}

function gitOutput(args) {
  const result = runProcess("git", ["-C", REPO_ROOT, ...args]);
  return result.exitCode === 0 ? result.stdout.trim() : null;
}

function gitDirtyFiles() {
  const porcelain = gitOutput(["status", "--porcelain"]);
  if (!porcelain) return [];
  return porcelain.split(/\r?\n/).filter(Boolean).map((line) => line.slice(3));
}

function gitTracking() {
  const tracking = gitOutput(["rev-list", "--left-right", "--count", "HEAD...@{upstream}"]);
  const match = tracking?.match(/(\d+)\s+(\d+)/);
  return match ? { ahead: Number(match[1]), behind: Number(match[2]) } : { ahead: 0, behind: 0 };
}

function gameProcessInfo() {
  const interop = windowsInteropInfo({ probe: false });
  if (interop.ok) {
    const result = commandWindowsAx("game-running", {}, {});
    if (result.ok && result.data && typeof result.data === "object") {
      return {
        ...result.data,
        visibility: "windows-powershell"
      };
    }
  }

  const pgrep = commandExists("pgrep")
    ? runProcess("pgrep", ["-af", "prime(\\.exe)?"])
    : { exitCode: 1, stdout: "", stderr: "pgrep unavailable" };
  const lines = pgrep.stdout.split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
  if (lines.length === 0) {
    return {
      ok: true,
      running: false,
      visibility: "wsl-process-table",
      message: "Game is not visible from WSL."
    };
  }
  const [pidText, ...commandParts] = lines[0].split(/\s+/);
  return {
    ok: true,
    running: true,
    pid: Number(pidText),
    processName: commandParts[0] ?? null,
    commandLine: commandParts.join(" "),
    visibility: "wsl-process-table"
  };
}

function logSummary(logPath) {
  if (!exists(logPath)) {
    return { exists: false, path: logPath, lastModified: null, errorCount: 0, warnCount: 0 };
  }
  const lines = readText(logPath).split(/\r?\n/);
  let sessionStart = -1;
  for (let i = lines.length - 1; i >= 0; i -= 1) {
    if (/Installed .+ version/.test(lines[i])) {
      sessionStart = i;
      break;
    }
  }
  const sessionLines = (sessionStart >= 0 ? lines.slice(sessionStart + 1) : lines).filter((line) => !isIgnoredLogIssue(line));
  return {
    exists: true,
    path: logPath,
    lastModified: mtime(logPath),
    errorCount: sessionLines.filter((line) => /\[error\]/i.test(line)).length,
    warnCount: sessionLines.filter((line) => /\[warn/i.test(line)).length
  };
}

function readLogLines(logPath) {
  if (!exists(logPath)) {
    return fail("Log file not found.", { path: logPath });
  }
  return { ok: true, lines: readText(logPath).split(/\r?\n/) };
}

function formatLogLine(line, raw = false, keepLevel = false) {
  if (raw) return line;
  const cleaned = line.replace(/\x1b\[[0-9;?]*[ -/]*[@-~]/g, "");
  const match = cleaned.match(/^\[([^\]]+)\]\s*\[([^\]]+)\]\s*\[([^\]]+)\]\s?(.*)$/);
  if (!match) return cleaned;
  return keepLevel ? `[${match[1]}] [${match[3]}] ${match[4]}` : `[${match[1]}] ${match[4]}`;
}

function isIgnoredLogIssue(line) {
  return IGNORED_LOG_ISSUES.some((pattern) => pattern.test(line));
}

function compactBoot(boot) {
  return {
    ok: true,
    version: boot.version,
    hookCount: boot.hookCount,
    installed: boot.installed,
    skipped: boot.skipped,
    bootErrors: boot.bootErrors
  };
}

function primeHeaderMatches(className, maxResults) {
  const paths = getPaths();
  if (!exists(paths.primeRoot)) return [];
  const classLeaf = className.split(/[\\/]/).pop().replace(/\.h$/i, "");
  const headers = readdirSync(paths.primeRoot, { withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.endsWith(".h"))
    .map((entry) => join(paths.primeRoot, entry.name));
  const exact = [];
  const contains = [];
  for (const header of headers) {
    const base = header.split(/[\\/]/).pop().replace(/\.h$/i, "");
    const entry = {
      path: header,
      relative: relative(REPO_ROOT, header),
      matchKind: base.toLowerCase() === classLeaf.toLowerCase() ? "exact" : "contains",
      className: base,
      lineCount: readText(header).split(/\r?\n/).length
    };
    if (entry.matchKind === "exact") exact.push(entry);
    else if (base.toLowerCase().includes(classLeaf.toLowerCase())) contains.push(entry);
  }
  return [...exact, ...contains].slice(0, maxResults);
}

function filePreview(filePath, maxLines) {
  if (!exists(filePath)) return null;
  const lines = readText(filePath).split(/\r?\n/);
  const take = Math.min(maxLines, lines.length);
  return {
    path: filePath,
    totalLines: lines.length,
    previewLines: take,
    truncated: lines.length > take,
    text: lines.slice(0, take).join("\n")
  };
}

function liveStateClassMatches(entry, options) {
  const fullName = entry.fullName
    ?? (entry.classNamespace ? `${entry.classNamespace}.${entry.className}` : entry.className)
    ?? "";
  const exact = bool(options.exact);
  const minCount = intOpt(options["min-count"], -1);
  const maxCount = intOpt(options["max-count"], -1);
  return textFilter(fullName, options.match, exact)
    && textFilter(entry.className, options["class-name"], exact)
    && textFilter(entry.classNamespace, options.namespace, exact)
    && textFilter(entry.classPointer, options.pointer, exact)
    && (minCount < 0 || Number(entry.count ?? 0) >= minCount)
    && (maxCount < 0 || Number(entry.count ?? 0) <= maxCount);
}

function textFilter(value, pattern, exact) {
  const rawPattern = stringOpt(pattern);
  if (!rawPattern) return true;
  const candidate = String(value ?? "");
  if (exact) return candidate.toLowerCase() === rawPattern.toLowerCase();
  if (rawPattern.includes("*") || rawPattern.includes("?")) {
    const regex = new RegExp(`^${escapeRegExp(rawPattern).replace(/\\\*/g, ".*").replace(/\\\?/g, ".")}$`, "i");
    return regex.test(candidate);
  }
  return candidate.toLowerCase().includes(rawPattern.toLowerCase());
}

function buildMatcher(pattern, caseSensitive) {
  const raw = stringOpt(pattern);
  if (!raw) return null;
  let regex = null;
  try {
    regex = new RegExp(raw, caseSensitive ? "" : "i");
  } catch {
    const needle = caseSensitive ? raw : raw.toLowerCase();
    return (line) => (caseSensitive ? line : line.toLowerCase()).includes(needle);
  }
  return (line) => regex.test(line);
}

function parseCli(tokens) {
  const rest = [...tokens];
  let selected = "help";
  if (rest[0] && !rest[0].startsWith("--")) {
    selected = rest.shift();
  }
  const parsed = {};
  for (let index = 0; index < rest.length; index += 1) {
    const token = rest[index];
    if (!token.startsWith("--")) continue;
    const body = token.slice(2);
    const equals = body.indexOf("=");
    if (equals >= 0) {
      parsed[body.slice(0, equals)] = parseLiteral(body.slice(equals + 1));
      continue;
    }
    const next = rest[index + 1];
    if (next === undefined || next.startsWith("--")) {
      parsed[body] = true;
      continue;
    }
    parsed[body] = parseLiteral(next);
    index += 1;
  }
  return { command: selected, options: parsed };
}

function parseLiteral(value) {
  if (value === "true") return true;
  if (value === "false") return false;
  return value;
}

function ok(data = null, hints = []) {
  return { ok: true, data, hints };
}

function fail(message, data = null, hints = []) {
  return { ok: false, error: { message }, data, hints };
}

function unsupported(message, data = {}) {
  return fail(message, { unsupported: true, ...data }, ["This command exists for name parity with the old PowerShell .ax surface."]);
}

function writeEnvelope(commandName, result) {
  const envelope = {
    ok: Boolean(result.ok),
    command: commandName,
    timestamp: new Date().toISOString(),
    durationMs: Math.round(performance.now() - started),
    data: result.data ?? null
  };
  if (result.error) envelope.error = result.error;
  if (Array.isArray(result.hints) && result.hints.length > 0) envelope.hints = result.hints;
  process.stdout.write(`${JSON.stringify(envelope, null, 2)}\n`);
}

function exists(filePath) {
  return Boolean(filePath && existsSync(filePath));
}

function readText(filePath) {
  return readFileSync(filePath, "utf8");
}

function sha256(filePath) {
  return createHash("sha256").update(readFileSync(filePath)).digest("hex").toUpperCase();
}

function mtime(filePath) {
  return exists(filePath) ? statSync(filePath).mtime.toISOString() : null;
}

function tailLines(text, last = 30) {
  if (!text || !String(text).trim()) return [];
  return String(text)
    .replace(/\x1b\[[0-9;?]*[ -/]*[@-~]/g, "")
    .split(/\r?\n/)
    .filter((line) => line !== "")
    .slice(-Math.max(1, last));
}

function parseJsonMaybe(text) {
  const trimmed = String(text ?? "").trim();
  if (!trimmed) return "";
  try {
    return JSON.parse(stringifyWideJsonIntegers(trimmed));
  } catch {
    return trimmed;
  }
}

function stringifyWideJsonIntegers(jsonText) {
  return jsonText.replace(/([:\[,]\s*)(-?\d{16,})(?=\s*[,}\]])/g, '$1"$2"');
}

function intOpt(value, fallback) {
  if (value === undefined || value === null || value === "") return fallback;
  const parsed = Number.parseInt(String(value), 10);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function bool(value) {
  return value === true || value === "true";
}

function stringOpt(value) {
  if (value === undefined || value === null || value === false) return "";
  return String(value);
}

function splitList(value) {
  if (value === undefined || value === null || value === false) return [];
  if (Array.isArray(value)) return value.flatMap(splitList);
  return String(value).split(",").map((entry) => entry.trim()).filter(Boolean);
}

function firstLine(text) {
  return String(text ?? "").split(/\r?\n/).find((line) => line.trim())?.trim() ?? "";
}

function rmQuiet(filePath) {
  try {
    rmSync(filePath, { force: true });
  } catch {
  }
}

function sleep(ms) {
  return new Promise((resolveSleep) => setTimeout(resolveSleep, ms));
}

function shellQuote(value) {
  return `'${String(value).replace(/'/g, "'\\''")}'`;
}

function escapePowerShellSingleQuoted(value) {
  return String(value).replace(/'/g, "''");
}

function powerShellSingleQuoted(value) {
  return `'${escapePowerShellSingleQuoted(value)}'`;
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
