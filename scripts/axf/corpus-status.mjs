#!/usr/bin/env node

import { createHash } from "node:crypto";
import { createReadStream, lstatSync, realpathSync } from "node:fs";
import { dirname, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const CORPUS_FILES = ["dump.cs", "script.json"];

async function sha256File(path) {
  const hash = createHash("sha256");
  for await (const chunk of createReadStream(path)) {
    hash.update(chunk);
  }
  return hash.digest("hex");
}

function isContained(root, path) {
  const relation = relative(root, path);
  return (
    relation === "" || (!relation.startsWith(`..${sep}`) && relation !== "..")
  );
}

function inspectPhysicalPath(path, { repoRoot, allowedRoot }) {
  if (!isContained(repoRoot, allowedRoot) || !isContained(allowedRoot, path)) {
    return { exists: false, error: "path-outside-allowed-root" };
  }

  try {
    const repoStat = lstatSync(repoRoot);
    if (
      repoStat.isSymbolicLink() ||
      !repoStat.isDirectory() ||
      realpathSync(repoRoot) !== repoRoot
    ) {
      return { exists: false, error: "unsafe-repository-root" };
    }
  } catch {
    return { exists: false, error: "repository-root-unavailable" };
  }

  const segments = relative(repoRoot, path).split(sep).filter(Boolean);
  let current = repoRoot;
  for (const [index, segment] of segments.entries()) {
    current = join(current, segment);
    let stat;
    try {
      stat = lstatSync(current);
    } catch (error) {
      if (error?.code === "ENOENT") return { exists: false };
      return { exists: false, error: "path-inspection-failed" };
    }
    if (stat.isSymbolicLink()) {
      return { exists: false, error: "symlink-not-allowed" };
    }
    const isTarget = index === segments.length - 1;
    if (!isTarget && !stat.isDirectory()) {
      return { exists: false, error: "path-component-not-directory" };
    }
    if (isTarget && !stat.isFile()) {
      return { exists: false, error: "not-a-file" };
    }
    if (isTarget) return { exists: true, stat };
  }

  return { exists: false, error: "not-a-file" };
}

async function inspectFile(path, { hash = true, repoRoot, allowedRoot } = {}) {
  const physical = inspectPhysicalPath(path, { repoRoot, allowedRoot });
  if (!physical.exists) {
    return {
      path,
      exists: false,
      sizeBytes: null,
      modifiedAt: null,
      sha256: null,
      ...(physical.error ? { error: physical.error } : {}),
    };
  }

  return {
    path,
    exists: true,
    sizeBytes: physical.stat.size,
    modifiedAt: physical.stat.mtime.toISOString(),
    sha256: hash ? await sha256File(path) : null,
  };
}

function classifyDuplicate(canonical, candidate) {
  if (!candidate.exists) return null;
  if (!canonical.exists) return "legacy-only";
  if (candidate.sha256 === canonical.sha256) return "identical-copy";
  return Date.parse(candidate.modifiedAt) <= Date.parse(canonical.modifiedAt)
    ? "stale-copy"
    : "divergent-newer-copy";
}

function duplicateWarning(name, relation, canonicalPath, candidatePath) {
  const prefix = `${candidatePath} is a plausible duplicate of ${canonicalPath}`;
  switch (relation) {
    case "identical-copy":
      return `${prefix}. It is currently identical, but raw research must use the canonical ${name} path.`;
    case "stale-copy":
      return `${prefix} and is stale. Do not use it as current research evidence.`;
    case "divergent-newer-copy":
      return `${prefix} and is newer but content-divergent. Reconcile it before treating either update as current evidence.`;
    case "legacy-only":
      return `${candidatePath} exists while the canonical ${name} is missing. Restore the canonical corpus before research.`;
    default:
      return null;
  }
}

export async function inspectCorpus(options = {}) {
  const repoRoot = resolve(options.repoRoot ?? REPO_ROOT);
  const canonicalRoot = resolve(
    options.canonicalRoot ?? join(repoRoot, "tools", "il2cpp-dump"),
  );
  const legacyRoot = resolve(
    options.legacyRoot ?? join(repoRoot, ".ax-priv", "tools", "Il2CppDumper"),
  );
  const indexPath = resolve(
    options.indexPath ?? join(repoRoot, ".ax-priv", "cache", "stfc.db"),
  );

  const canonical = {};
  const legacy = {};
  const duplicates = [];
  const warnings = [];
  const inspectionErrors = [];

  for (const name of CORPUS_FILES) {
    canonical[name] = await inspectFile(join(canonicalRoot, name), {
      repoRoot,
      allowedRoot: canonicalRoot,
    });
    legacy[name] = await inspectFile(join(legacyRoot, name), {
      repoRoot,
      allowedRoot: legacyRoot,
    });
    for (const entry of [canonical[name], legacy[name]]) {
      if (!entry.error) continue;
      inspectionErrors.push(entry);
      warnings.push(
        `${entry.path} was not inspected because its path is unsafe (${entry.error}).`,
      );
    }
    const relation = classifyDuplicate(canonical[name], legacy[name]);
    if (!relation) continue;
    duplicates.push({
      artifact: name,
      relation,
      canonicalPath: canonical[name].path,
      candidatePath: legacy[name].path,
      canonicalSha256: canonical[name].sha256,
      candidateSha256: legacy[name].sha256,
    });
    warnings.push(
      duplicateWarning(name, relation, canonical[name].path, legacy[name].path),
    );
  }

  const missingCanonical = CORPUS_FILES.filter(
    (name) => !canonical[name].exists,
  );
  if (missingCanonical.length > 0) {
    warnings.unshift(
      `Canonical corpus is incomplete at ${canonicalRoot}; missing: ${missingCanonical.join(", ")}.`,
    );
  }

  const index = await inspectFile(indexPath, {
    hash: false,
    repoRoot,
    allowedRoot: dirname(indexPath),
  });
  if (index.error) {
    inspectionErrors.push(index);
    warnings.push(
      `${index.path} was not inspected because its path is unsafe (${index.error}).`,
    );
  }
  if (!index.exists) {
    warnings.push(`The canonical dump index is missing at ${indexPath}.`);
  }

  const canonicalReady = missingCanonical.length === 0;
  const pathsSafe = inspectionErrors.length === 0;
  return {
    ok: canonicalReady && pathsSafe,
    state: !pathsSafe
      ? "unsafe-path"
      : canonicalReady
        ? duplicates.length > 0
          ? "duplicate-warning"
          : "ready"
        : "canonical-missing",
    guidance: {
      canonicalRoot,
      canonicalDump: join(canonicalRoot, "dump.cs"),
      canonicalMetadata: join(canonicalRoot, "script.json"),
      canonicalIndex: indexPath,
      rawSearchRoot: canonicalRoot,
      statusCapability: "global.stfc-mod-private.corpus-status",
      refreshCapability: "global.stfc-mod-private.dump-refresh",
    },
    canonical,
    legacyCandidates: legacy,
    index,
    duplicateCandidates: duplicates,
    warnings,
  };
}

async function main() {
  try {
    if (process.argv.length > 2) {
      throw new Error("corpus-status does not accept path overrides");
    }
    const data = await inspectCorpus();
    process.stdout.write(`${JSON.stringify({ ok: data.ok, data })}\n`);
    process.exitCode = 0;
  } catch (error) {
    process.stdout.write(
      `${JSON.stringify({
        ok: false,
        error: {
          message: error instanceof Error ? error.message : String(error),
        },
      })}\n`,
    );
    process.exitCode = 1;
  }
}

if (resolve(process.argv[1] ?? "") === fileURLToPath(import.meta.url)) {
  await main();
}
