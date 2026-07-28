import test from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import {
  copyFileSync,
  mkdtempSync,
  mkdirSync,
  rmSync,
  symlinkSync,
  utimesSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { inspectCorpus } from "./corpus-status.mjs";

const roots = [];

function fixture() {
  const repoRoot = mkdtempSync(join(tmpdir(), "stfc-corpus-status-"));
  roots.push(repoRoot);
  const canonicalRoot = join(repoRoot, "tools", "il2cpp-dump");
  const legacyRoot = join(repoRoot, ".ax-priv", "tools", "Il2CppDumper");
  const indexPath = join(repoRoot, ".ax-priv", "cache", "stfc.db");
  mkdirSync(canonicalRoot, { recursive: true });
  mkdirSync(legacyRoot, { recursive: true });
  mkdirSync(join(repoRoot, ".ax-priv", "cache"), { recursive: true });
  return { repoRoot, canonicalRoot, legacyRoot, indexPath };
}

function writeCanonical(paths, suffix = "current") {
  writeFileSync(
    join(paths.canonicalRoot, "dump.cs"),
    `canonical-dump-${suffix}\n`,
  );
  writeFileSync(
    join(paths.canonicalRoot, "script.json"),
    JSON.stringify({ corpus: suffix }),
  );
  writeFileSync(paths.indexPath, `sqlite-index-${suffix}\n`);
}

function installPublicScript(paths) {
  const scriptDir = join(paths.repoRoot, "scripts", "axf");
  mkdirSync(scriptDir, { recursive: true });
  const scriptPath = join(scriptDir, "corpus-status.mjs");
  copyFileSync(join(import.meta.dirname, "corpus-status.mjs"), scriptPath);
  return scriptPath;
}

function invokePublicScript(paths) {
  const result = spawnSync(process.execPath, [installPublicScript(paths)], {
    cwd: paths.repoRoot,
    encoding: "utf8",
  });
  assert.equal(result.status, 0);
  return JSON.parse(result.stdout);
}

test.afterEach(() => {
  for (const root of roots.splice(0)) {
    rmSync(root, { recursive: true, force: true });
  }
});

test("reports the tracked dump root as the only canonical raw-search path", async () => {
  const paths = fixture();
  writeCanonical(paths);

  const result = await inspectCorpus({ repoRoot: paths.repoRoot });

  assert.equal(result.ok, true);
  assert.equal(result.state, "ready");
  assert.equal(result.guidance.canonicalRoot, paths.canonicalRoot);
  assert.equal(result.guidance.rawSearchRoot, paths.canonicalRoot);
  assert.equal(result.guidance.canonicalIndex, paths.indexPath);
  assert.deepEqual(result.duplicateCandidates, []);
  assert.deepEqual(result.warnings, []);
});

test("missing canonical artifacts fail closed even when a legacy copy exists", async () => {
  const paths = fixture();
  writeFileSync(join(paths.legacyRoot, "dump.cs"), "legacy-only\n");

  const result = await inspectCorpus({ repoRoot: paths.repoRoot });

  assert.equal(result.ok, false);
  assert.equal(result.state, "canonical-missing");
  assert.deepEqual(
    result.duplicateCandidates.map((entry) => [entry.artifact, entry.relation]),
    [["dump.cs", "legacy-only"]],
  );
  assert.match(result.warnings.join("\n"), /Restore the canonical corpus/);
});

test("classifies content-divergent older legacy artifacts as stale", async () => {
  const paths = fixture();
  writeCanonical(paths);
  const legacyDump = join(paths.legacyRoot, "dump.cs");
  const legacyMetadata = join(paths.legacyRoot, "script.json");
  writeFileSync(legacyDump, "old-dump\n");
  writeFileSync(legacyMetadata, JSON.stringify({ corpus: "old" }));
  const oldTime = new Date("2025-01-01T00:00:00.000Z");
  utimesSync(legacyDump, oldTime, oldTime);
  utimesSync(legacyMetadata, oldTime, oldTime);

  const result = await inspectCorpus({ repoRoot: paths.repoRoot });

  assert.equal(result.ok, true);
  assert.equal(result.state, "duplicate-warning");
  assert.deepEqual(
    result.duplicateCandidates.map((entry) => entry.relation),
    ["stale-copy", "stale-copy"],
  );
  assert.match(result.warnings.join("\n"), /must use|Do not use/);
});

test("identical legacy copies still warn because the path is non-canonical", async () => {
  const paths = fixture();
  writeCanonical(paths);
  writeFileSync(join(paths.legacyRoot, "dump.cs"), "canonical-dump-current\n");
  writeFileSync(
    join(paths.legacyRoot, "script.json"),
    JSON.stringify({ corpus: "current" }),
  );

  const result = await inspectCorpus({ repoRoot: paths.repoRoot });

  assert.equal(result.ok, true);
  assert.deepEqual(
    result.duplicateCandidates.map((entry) => entry.relation),
    ["identical-copy", "identical-copy"],
  );
  assert.match(
    result.warnings.join("\n"),
    /raw research must use the canonical/,
  );
});

test("newer divergent legacy copies are surfaced without leaking file bodies", async () => {
  const paths = fixture();
  writeCanonical(paths);
  const privateBody = "private-legacy-source-body";
  const legacyDump = join(paths.legacyRoot, "dump.cs");
  writeFileSync(legacyDump, privateBody);
  const future = new Date("2030-01-01T00:00:00.000Z");
  utimesSync(legacyDump, future, future);

  const result = await inspectCorpus({ repoRoot: paths.repoRoot });
  const serialized = JSON.stringify(result);

  assert.equal(result.ok, true);
  assert.equal(result.duplicateCandidates[0].relation, "divergent-newer-copy");
  assert.match(result.warnings.join("\n"), /Reconcile it/);
  assert.equal(serialized.includes(privateBody), false);
});

test("missing index is reported separately from canonical artifact readiness", async () => {
  const paths = fixture();
  writeCanonical(paths);
  rmSync(paths.indexPath);

  const result = await inspectCorpus({ repoRoot: paths.repoRoot });

  assert.equal(result.ok, true);
  assert.equal(result.index.exists, false);
  assert.match(result.warnings.join("\n"), /canonical dump index is missing/);
});

test("the public capability rejects path overrides", () => {
  const result = spawnSync(
    process.execPath,
    [join(import.meta.dirname, "corpus-status.mjs"), "--repo-root", "/tmp"],
    { encoding: "utf8" },
  );

  assert.equal(result.status, 1);
  assert.deepEqual(JSON.parse(result.stdout), {
    ok: false,
    error: {
      message: "corpus-status does not accept path overrides",
    },
  });
});

test("the public capability refuses a symlinked canonical artifact", () => {
  const paths = fixture();
  const outsideRoot = mkdtempSync(join(tmpdir(), "stfc-corpus-outside-"));
  roots.push(outsideRoot);
  const privateBody = "outside-canonical-private-body";
  const outsideDump = join(outsideRoot, "outside-dump.cs");
  writeFileSync(outsideDump, privateBody);
  symlinkSync(outsideDump, join(paths.canonicalRoot, "dump.cs"));
  writeFileSync(
    join(paths.canonicalRoot, "script.json"),
    JSON.stringify({ corpus: "current" }),
  );
  writeFileSync(paths.indexPath, "sqlite-index-current\n");

  const envelope = invokePublicScript(paths);
  const serialized = JSON.stringify(envelope);

  assert.equal(envelope.ok, false);
  assert.equal(envelope.data.state, "unsafe-path");
  assert.equal(envelope.data.canonical["dump.cs"].sha256, null);
  assert.equal(envelope.data.canonical["dump.cs"].error, "symlink-not-allowed");
  assert.equal(serialized.includes(privateBody), false);
});

test("the public capability refuses a symlinked legacy path component", () => {
  const paths = fixture();
  writeCanonical(paths);
  const outsideRoot = mkdtempSync(join(tmpdir(), "stfc-corpus-outside-"));
  roots.push(outsideRoot);
  const privateBody = "outside-legacy-private-body";
  writeFileSync(join(outsideRoot, "dump.cs"), privateBody);
  rmSync(paths.legacyRoot, { recursive: true });
  symlinkSync(outsideRoot, paths.legacyRoot);

  const envelope = invokePublicScript(paths);
  const serialized = JSON.stringify(envelope);

  assert.equal(envelope.ok, false);
  assert.equal(envelope.data.state, "unsafe-path");
  assert.equal(envelope.data.legacyCandidates["dump.cs"].sha256, null);
  assert.equal(
    envelope.data.legacyCandidates["dump.cs"].error,
    "symlink-not-allowed",
  );
  assert.equal(serialized.includes(privateBody), false);
});
