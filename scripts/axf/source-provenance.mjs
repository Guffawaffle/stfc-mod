import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import {
  existsSync,
  lstatSync,
  readFileSync,
  readlinkSync,
  realpathSync
} from "node:fs";
import {
  basename,
  dirname,
  isAbsolute,
  join,
  relative,
  resolve,
  sep
} from "node:path";

export const SOURCE_PROVENANCE_VERSION = 1;
export const DEFAULT_CHANGED_PATH_LIMIT = 64;

function stableCompare(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

export function normalizeGitPath(value) {
  return String(value).split(sep).join("/").replaceAll("\\", "/").replace(/^\.\/+/, "");
}

function git(repoRoot, args, { allowFailure = false } = {}) {
  const result = spawnSync("git", ["-C", repoRoot, ...args], {
    encoding: "utf8",
    maxBuffer: 128 * 1024 * 1024
  });
  if (result.status !== 0 && !allowFailure) {
    throw new Error(
      `git ${args.join(" ")} failed: ${String(result.stderr || result.error || "unknown error").trim()}`
    );
  }
  return result.status === 0 ? result.stdout : null;
}

function parsePorcelain(output) {
  const records = String(output ?? "").split("\0");
  const entries = [];

  for (let index = 0; index < records.length; index += 1) {
    const record = records[index];
    if (!record) continue;
    const status = record.slice(0, 2);
    const path = normalizeGitPath(record.slice(3));
    const entry = { status, path };
    if (/[RC]/.test(status)) {
      const originalPath = records[index + 1];
      if (originalPath) {
        entry.originalPath = normalizeGitPath(originalPath);
        index += 1;
      }
    }
    entries.push(entry);
  }

  return entries.sort((left, right) =>
    stableCompare(
      `${left.path}\0${left.status}\0${left.originalPath ?? ""}`,
      `${right.path}\0${right.status}\0${right.originalPath ?? ""}`
    )
  );
}

function hashText(value) {
  return createHash("sha256").update(value).digest("hex");
}

function repositoryRelativePath(repoRoot, candidate) {
  const target = resolve(repoRoot, candidate);
  const relativeTarget = relative(resolve(repoRoot), target);
  if (
    relativeTarget === ".." ||
    relativeTarget.startsWith(`..${sep}`) ||
    isAbsolute(relativeTarget)
  ) {
    throw new Error(`Source manifest path escapes the repository: ${candidate}`);
  }
  return target;
}

function pathContains(parent, child) {
  const childRelative = relative(resolve(parent), resolve(child));
  return (
    childRelative === "" ||
    (!childRelative.startsWith(`..${sep}`) &&
      childRelative !== ".." &&
      !isAbsolute(childRelative))
  );
}

export function resolvePhysicalCandidate(candidate) {
  let existing = resolve(candidate);
  const suffix = [];
  while (!existsSync(existing)) {
    const parent = dirname(existing);
    if (parent === existing) {
      throw new Error(`No existing ancestor for path: ${candidate}`);
    }
    suffix.unshift(basename(existing));
    existing = parent;
  }
  return resolve(realpathSync(existing), ...suffix);
}

export function assertIsolatedMirrorRoot(candidate, protectedRoots) {
  const physicalCandidate = resolvePhysicalCandidate(candidate);
  if (physicalCandidate === dirname(physicalCandidate)) {
    throw new Error(`Unsafe mirror root: ${physicalCandidate}`);
  }
  for (const protectedRoot of protectedRoots) {
    const physicalProtected = resolvePhysicalCandidate(protectedRoot);
    if (
      pathContains(physicalCandidate, physicalProtected) ||
      pathContains(physicalProtected, physicalCandidate)
    ) {
      throw new Error(
        `Mirror root overlaps a protected path: ${physicalCandidate}`
      );
    }
  }
  return physicalCandidate;
}

function submoduleGitlinks(repoRoot) {
  const links = new Map();
  for (const record of git(repoRoot, ["ls-files", "--stage", "-z"]).split("\0")) {
    if (!record) continue;
    const match = record.match(/^160000 ([a-f0-9]{40,64}) \d+\t(.+)$/s);
    if (match) {
      links.set(normalizeGitPath(match[2]), match[1]);
    }
  }
  return links;
}

function assertNoHiddenIndexEntries(repoRoot, prefix = "") {
  for (const record of git(
    repoRoot,
    ["ls-files", "-v", "-z", "--cached"]
  ).split("\0")) {
    if (!record) continue;
    const tag = record.slice(0, 1);
    if (tag === "S" || /^[a-z]$/.test(tag)) {
      const path = normalizeGitPath(record.slice(2));
      throw new Error(
        `Tracked source uses assume-unchanged or skip-worktree: ${normalizeGitPath(`${prefix}${path}`)}`
      );
    }
  }
}

function listRepositoryFiles(repoRoot, prefix = "") {
  assertNoHiddenIndexEntries(repoRoot, prefix);
  const links = submoduleGitlinks(repoRoot);
  const files = [];
  const listed = git(repoRoot, [
    "ls-files",
    "-z",
    "--cached",
    "--others",
    "--exclude-standard"
  ]).split("\0");

  for (const rawPath of listed) {
    if (!rawPath) continue;
    const path = normalizeGitPath(rawPath);
    const submoduleCommit = links.get(path);
    if (submoduleCommit) {
      const submoduleRoot = repositoryRelativePath(repoRoot, path);
      if (!existsSync(join(submoduleRoot, ".git"))) {
        continue;
      }
      const actualCommit = git(submoduleRoot, ["rev-parse", "HEAD"]).trim();
      const dirty = git(submoduleRoot, [
        "status",
        "--porcelain=v1",
        "-z",
        "--untracked-files=all",
        "--ignored=no"
      ]);
      if (actualCommit !== submoduleCommit || dirty) {
        throw new Error(
          `Dirty or mismatched submodule source is not supported: ${normalizeGitPath(`${prefix}${path}`)}`
        );
      }
      files.push(
        ...listRepositoryFiles(
          submoduleRoot,
          normalizeGitPath(`${prefix}${path}/`)
        )
      );
      continue;
    }

    const target = repositoryRelativePath(repoRoot, path);
    try {
      const stat = lstatSync(target);
      if (stat.isFile() || stat.isSymbolicLink()) {
        files.push(normalizeGitPath(`${prefix}${path}`));
      }
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
  }

  return files;
}

function manifestEvidence(repoRoot, path) {
  const target = repositoryRelativePath(repoRoot, path);
  const stat = lstatSync(target);
  if (stat.isSymbolicLink()) {
    return {
      path,
      kind: "symlink",
      digest: hashText(readlinkSync(target))
    };
  }
  if (stat.isFile()) {
    return {
      path,
      kind: "file",
      digest: createHash("sha256").update(readFileSync(target)).digest("hex")
    };
  }
  throw new Error(`Unsupported source manifest entry: ${path}`);
}

function createManifest(repoRoot) {
  const paths = [...new Set(listRepositoryFiles(repoRoot))].sort(stableCompare);
  const windowsKeys = new Map();
  for (const path of paths) {
    const key = path.normalize("NFC").toLowerCase();
    const existing = windowsKeys.get(key);
    if (existing && existing !== path) {
      throw new Error(
        `Source paths collide on Windows: ${existing} and ${path}`
      );
    }
    windowsKeys.set(key, path);
  }
  const evidence = paths.map((path) => manifestEvidence(repoRoot, path));
  return {
    paths,
    fingerprint: `sha256:${hashText(JSON.stringify(evidence))}`
  };
}

export function fingerprintSourcePaths(repoRoot, paths) {
  const normalizedPaths = [...paths].map(normalizeGitPath).sort(stableCompare);
  const evidence = normalizedPaths.map((path) =>
    manifestEvidence(repoRoot, path)
  );
  return `sha256:${hashText(JSON.stringify(evidence))}`;
}

function collectStableSourceState(repoRoot) {
  const normalizedRoot = resolve(repoRoot);
  for (let attempt = 0; attempt < 2; attempt += 1) {
    const baseCommit = git(normalizedRoot, ["rev-parse", "HEAD"]).trim();
    const status = git(normalizedRoot, [
      "status",
      "--porcelain=v1",
      "-z",
      "--untracked-files=all",
      "--ignored=no"
    ]);
    const manifest = createManifest(normalizedRoot);
    const baseAfter = git(normalizedRoot, ["rev-parse", "HEAD"]).trim();
    const statusAfter = git(normalizedRoot, [
      "status",
      "--porcelain=v1",
      "-z",
      "--untracked-files=all",
      "--ignored=no"
    ]);
    if (baseCommit === baseAfter && status === statusAfter) {
      return {
        baseCommit,
        status,
        entries: parsePorcelain(status),
        manifest
      };
    }
  }
  throw new Error("Repository source changed while provenance was collected.");
}

/**
 * Describe one stable, canonical source snapshot and return the exact file
 * manifest that must be used to materialize it.
 */
export function collectSourceSnapshot(
  repoRoot,
  { pathLimit = DEFAULT_CHANGED_PATH_LIMIT } = {}
) {
  if (!Number.isInteger(pathLimit) || pathLimit < 0) {
    throw new Error("pathLimit must be a non-negative integer");
  }

  const state = collectStableSourceState(repoRoot);
  const worktreeDirty = state.entries.length > 0;
  const fingerprint = worktreeDirty
    ? hashText(
        JSON.stringify({
          schemaVersion: SOURCE_PROVENANCE_VERSION,
          baseCommit: state.baseCommit,
          sourceManifestFingerprint: state.manifest.fingerprint
        })
      )
    : null;

  return {
    provenance: {
      schemaVersion: SOURCE_PROVENANCE_VERSION,
      sourceStateKind: worktreeDirty ? "dirty-worktree" : "clean-commit",
      worktreeDirty,
      baseCommit: state.baseCommit,
      baseCommitDescription: worktreeDirty
        ? "HEAD is the base commit; sourceStateId also includes the synchronized working-tree state."
        : "HEAD identifies this clean source state.",
      sourceStateId: worktreeDirty
        ? `dirty-sha256:${fingerprint}`
        : `git:${state.baseCommit}`,
      worktreeFingerprint: fingerprint ? `sha256:${fingerprint}` : null,
      sourceManifestFingerprint: state.manifest.fingerprint,
      sourceFileCount: state.manifest.paths.length,
      changedPathCount: state.entries.length,
      changedPaths: state.entries.slice(0, pathLimit),
      changedPathsTruncated: state.entries.length > pathLimit,
      changedPathLimit: pathLimit,
      ignoredPathsIncluded: false
    },
    manifest: state.manifest
  };
}

export function collectSourceProvenance(repoRoot, options = {}) {
  return collectSourceSnapshot(repoRoot, options).provenance;
}

export function normalizeSourceReceipt(parsed, sourceProvenance) {
  if (
    parsed === null ||
    typeof parsed !== "object" ||
    Array.isArray(parsed)
  ) {
    throw new Error("Windows dispatcher did not return an object receipt.");
  }
  const canonicalBase = sourceProvenance.baseCommit.toLowerCase();
  for (const field of ["baseCommit", "commit"]) {
    if (!(field in parsed)) continue;
    const value = parsed[field];
    if (
      typeof value !== "string" ||
      !/^[a-f0-9]{7,64}$/i.test(value) ||
      !canonicalBase.startsWith(value.toLowerCase())
    ) {
      throw new Error(
        `Windows dispatcher ${field} conflicts with canonical base commit.`
      );
    }
  }
  return {
    ...parsed,
    baseCommit: sourceProvenance.baseCommit,
    ...("commit" in parsed ? { commit: sourceProvenance.baseCommit } : {}),
    sourceProvenance
  };
}
