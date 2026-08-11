import test from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import {
  cpSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  symlinkSync,
  writeFileSync
} from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import {
  assertIsolatedMirrorRoot,
  collectSourceProvenance,
  collectSourceSnapshot,
  fingerprintSourcePaths,
  normalizeGitPath,
  normalizeSourceReceipt
} from "./source-provenance.mjs";

const roots = [];

function run(repoRoot, args) {
  const result = spawnSync("git", ["-C", repoRoot, ...args], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout.trim();
}

function fixture() {
  const root = mkdtempSync(join(tmpdir(), "stfc-source-provenance-"));
  roots.push(root);
  run(root, ["init", "-q"]);
  run(root, ["config", "user.name", "STFC Test"]);
  run(root, ["config", "user.email", "stfc-test@example.invalid"]);
  run(root, ["config", "commit.gpgsign", "false"]);
  writeFileSync(join(root, ".gitignore"), "ignored/\n");
  writeFileSync(join(root, "tracked.cpp"), "int value = 1;\n");
  run(root, ["add", "."]);
  run(root, ["commit", "-qm", "fixture"]);
  return root;
}

test.afterEach(() => {
  for (const root of roots.splice(0)) {
    rmSync(root, { recursive: true, force: true });
  }
});

test("clean source identifies a reproducible commit", () => {
  const root = fixture();
  const provenance = collectSourceProvenance(root);

  assert.equal(provenance.worktreeDirty, false);
  assert.equal(provenance.sourceStateKind, "clean-commit");
  assert.equal(provenance.sourceStateId, `git:${provenance.baseCommit}`);
  assert.equal(provenance.worktreeFingerprint, null);
  assert.deepEqual(provenance.changedPaths, []);
});

test("modified source cannot be mistaken for clean HEAD", () => {
  const root = fixture();
  writeFileSync(join(root, "tracked.cpp"), "int value = 2;\n");
  const provenance = collectSourceProvenance(root);

  assert.equal(provenance.worktreeDirty, true);
  assert.equal(provenance.sourceStateKind, "dirty-worktree");
  assert.match(provenance.sourceStateId, /^dirty-sha256:[a-f0-9]{64}$/);
  assert.match(provenance.worktreeFingerprint, /^sha256:[a-f0-9]{64}$/);
  assert.deepEqual(provenance.changedPaths, [{ status: " M", path: "tracked.cpp" }]);
  assert.match(provenance.baseCommitDescription, /base commit/);
});

test("source-state identity changes when dirty file content changes", () => {
  const root = fixture();
  writeFileSync(join(root, "tracked.cpp"), "int value = 2;\n");
  const first = collectSourceProvenance(root).sourceStateId;
  writeFileSync(join(root, "tracked.cpp"), "int value = 3;\n");
  const second = collectSourceProvenance(root).sourceStateId;

  assert.notEqual(first, second);
});

test("staged changes retain index status evidence", () => {
  const root = fixture();
  writeFileSync(join(root, "tracked.cpp"), "int staged = 1;\n");
  run(root, ["add", "tracked.cpp"]);

  assert.deepEqual(collectSourceProvenance(root).changedPaths, [
    { status: "M ", path: "tracked.cpp" }
  ]);
});

test("untracked files contribute path and content identity", () => {
  const root = fixture();
  writeFileSync(join(root, "new.cpp"), "int new_value = 1;\n");
  const first = collectSourceProvenance(root);
  writeFileSync(join(root, "new.cpp"), "int new_value = 2;\n");
  const second = collectSourceProvenance(root);

  assert.deepEqual(first.changedPaths, [{ status: "??", path: "new.cpp" }]);
  assert.notEqual(first.sourceStateId, second.sourceStateId);
});

test("ignored private artifacts are neither disclosed nor fingerprinted", () => {
  const root = fixture();
  mkdirSync(join(root, "ignored"));
  writeFileSync(join(root, "ignored", "secret.txt"), "private-one");
  const firstSnapshot = collectSourceSnapshot(root);
  const first = firstSnapshot.provenance;
  writeFileSync(join(root, "ignored", "secret.txt"), "private-two");
  const secondSnapshot = collectSourceSnapshot(root);
  const second = secondSnapshot.provenance;

  assert.equal(first.worktreeDirty, false);
  assert.deepEqual(second, first);
  assert.deepEqual(secondSnapshot.manifest, firstSnapshot.manifest);
  assert.equal(
    firstSnapshot.manifest.paths.includes("ignored/secret.txt"),
    false
  );
  assert.equal(JSON.stringify(first).includes("secret"), false);
});

test("changed path evidence is ordered, bounded, and reports truncation", () => {
  const root = fixture();
  for (const name of ["z.cpp", "a.cpp", "m.cpp"]) {
    writeFileSync(join(root, name), name);
  }

  const provenance = collectSourceProvenance(root, { pathLimit: 2 });
  assert.equal(provenance.changedPathCount, 3);
  assert.deepEqual(
    provenance.changedPaths.map((entry) => entry.path),
    ["a.cpp", "m.cpp"]
  );
  assert.equal(provenance.changedPathsTruncated, true);
  assert.equal(provenance.changedPathLimit, 2);
});

test("path evidence is separator-normalized and contains no source bodies", () => {
  assert.equal(normalizeGitPath("mods\\src\\patches\\file.cc"), "mods/src/patches/file.cc");

  const root = fixture();
  const sourceBody = "super-secret-source-body";
  writeFileSync(join(root, "tracked.cpp"), sourceBody);
  const serialized = JSON.stringify(collectSourceProvenance(root));

  assert.equal(serialized.includes(sourceBody), false);
  assert.equal(readFileSync(join(root, "tracked.cpp"), "utf8"), sourceBody);
});

test("dirty initialized submodules fail closed", () => {
  const submodule = fixture();
  const root = fixture();
  run(root, [
    "-c",
    "protocol.file.allow=always",
    "submodule",
    "add",
    "-q",
    submodule,
    "vendor/submodule"
  ]);
  run(root, ["commit", "-qm", "add submodule"]);

  const clean = collectSourceSnapshot(root);
  assert.equal(
    clean.manifest.paths.includes("vendor/submodule/tracked.cpp"),
    true
  );

  writeFileSync(
    join(root, "vendor", "submodule", "tracked.cpp"),
    "int submodule_value = 2;\n"
  );
  assert.throws(
    () => collectSourceSnapshot(root),
    /Dirty or mismatched submodule source is not supported/
  );
  writeFileSync(
    join(root, "vendor", "submodule", "tracked.cpp"),
    "int submodule_value = 3;\n"
  );
  assert.throws(
    () => collectSourceSnapshot(root),
    /Dirty or mismatched submodule source is not supported/
  );
});

test("receipt normalization preserves artifacts and pins canonical source identity", () => {
  const root = fixture();
  writeFileSync(join(root, "tracked.cpp"), "int dirty = 1;\n");
  const provenance = collectSourceProvenance(root);
  const receipt = normalizeSourceReceipt(
    {
      ok: true,
      commit: provenance.baseCommit,
      buildHash: "ARTIFACT-SHA256",
      deployedHash: "ARTIFACT-SHA256",
      hashMatch: true
    },
    provenance
  );

  assert.equal(receipt.baseCommit, provenance.baseCommit);
  assert.equal(receipt.commit, provenance.baseCommit);
  assert.deepEqual(receipt.sourceProvenance, provenance);
  assert.equal(receipt.buildHash, "ARTIFACT-SHA256");
  assert.equal(receipt.deployedHash, "ARTIFACT-SHA256");
  assert.equal(receipt.hashMatch, true);
});

test("receipt normalization accepts and canonicalizes a legacy short commit", () => {
  const root = fixture();
  const provenance = collectSourceProvenance(root);
  const receipt = normalizeSourceReceipt(
    {
      ok: true,
      commit: provenance.baseCommit.slice(0, 7),
      buildHash: "ARTIFACT-SHA256",
      deployedHash: "ARTIFACT-SHA256",
      hashMatch: true
    },
    provenance
  );

  assert.equal(receipt.commit, provenance.baseCommit);
  assert.equal(receipt.baseCommit, provenance.baseCommit);
  assert.equal(receipt.hashMatch, true);
});

test("receipt normalization rejects invalid or conflicting source identity", () => {
  const root = fixture();
  const provenance = collectSourceProvenance(root);

  assert.throws(
    () => normalizeSourceReceipt("not-json", provenance),
    /did not return an object receipt/
  );
  assert.throws(
    () =>
      normalizeSourceReceipt(
        { commit: "f".repeat(40), baseCommit: provenance.baseCommit },
        provenance
      ),
    /commit conflicts with canonical base commit/
  );
  assert.throws(
    () =>
      normalizeSourceReceipt(
        { commit: provenance.baseCommit.slice(0, 6) },
        provenance
      ),
    /commit conflicts with canonical base commit/
  );
  assert.throws(
    () =>
      normalizeSourceReceipt(
        { commit: provenance.baseCommit, baseCommit: "e".repeat(40) },
        provenance
      ),
    /baseCommit conflicts with canonical base commit/
  );
});

test("destination fingerprint detects copied-source drift", () => {
  const root = fixture();
  writeFileSync(join(root, "untracked.cpp"), "int untracked = 1;\n");
  const snapshot = collectSourceSnapshot(root);
  const destination = mkdtempSync(
    join(tmpdir(), "stfc-source-destination-")
  );
  roots.push(destination);

  for (const path of snapshot.manifest.paths) {
    const target = join(destination, path);
    mkdirSync(dirname(target), { recursive: true });
    cpSync(join(root, path), target, { verbatimSymlinks: true });
  }

  assert.equal(
    fingerprintSourcePaths(destination, snapshot.manifest.paths),
    snapshot.manifest.fingerprint
  );
  writeFileSync(join(destination, "untracked.cpp"), "int untracked = 2;\n");
  assert.notEqual(
    fingerprintSourcePaths(destination, snapshot.manifest.paths),
    snapshot.manifest.fingerprint
  );
});

test("Windows-colliding source paths fail closed", () => {
  const root = fixture();
  writeFileSync(join(root, "Case.cpp"), "int upper = 1;\n");
  writeFileSync(join(root, "case.cpp"), "int lower = 1;\n");

  assert.throws(
    () => collectSourceSnapshot(root),
    /Source paths collide on Windows/
  );
});

test("mirror isolation resolves symlinked parents for build, stage, and backup paths", () => {
  const root = mkdtempSync(join(tmpdir(), "stfc-mirror-guard-"));
  roots.push(root);
  const protectedRoot = join(root, "game");
  const alias = join(root, "alias");
  mkdirSync(protectedRoot);
  symlinkSync(protectedRoot, alias, "dir");

  for (const candidate of [
    join(alias, "build-mirror"),
    join(alias, "build-mirror.stage-test"),
    join(alias, "build-mirror.previous-test")
  ]) {
    assert.throws(
      () => assertIsolatedMirrorRoot(candidate, [protectedRoot]),
      /Mirror root overlaps a protected path/
    );
  }

  const safe = join(root, "safe", "build-mirror");
  assert.equal(assertIsolatedMirrorRoot(safe, [protectedRoot]), safe);
});

test("assume-unchanged tracked source fails closed", () => {
  const root = fixture();
  run(root, ["update-index", "--assume-unchanged", "tracked.cpp"]);
  writeFileSync(join(root, "tracked.cpp"), "int hidden = 2;\n");
  assert.equal(run(root, ["status", "--porcelain"]), "");

  assert.throws(
    () => collectSourceSnapshot(root),
    /Tracked source uses assume-unchanged or skip-worktree/
  );
});

test("skip-worktree tracked source fails closed", () => {
  const root = fixture();
  run(root, ["update-index", "--skip-worktree", "tracked.cpp"]);
  writeFileSync(join(root, "tracked.cpp"), "int hidden = 3;\n");
  assert.equal(run(root, ["status", "--porcelain"]), "");

  assert.throws(
    () => collectSourceSnapshot(root),
    /Tracked source uses assume-unchanged or skip-worktree/
  );
});
