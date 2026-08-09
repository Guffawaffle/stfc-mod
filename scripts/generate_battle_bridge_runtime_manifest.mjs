#!/usr/bin/env node

import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(scriptDirectory, "..");

export function parseAuthoritativeProducerContract(raw) {
  const containers = [];
  for (let index = 0; index < raw.length; index += 1) {
    const character = raw[index];
    if (character === "{") {
      containers.push(new Set());
      continue;
    }
    if (character === "[") {
      containers.push(null);
      continue;
    }
    if (character === "}" || character === "]") {
      containers.pop();
      continue;
    }
    if (character !== "\"") {
      continue;
    }

    const start = index;
    for (index += 1; index < raw.length; index += 1) {
      if (raw[index] === "\\") {
        index += 1;
      } else if (raw[index] === "\"") {
        break;
      }
    }

    let lookahead = index + 1;
    while (/\s/u.test(raw[lookahead] ?? "")) {
      lookahead += 1;
    }
    const keys = containers.at(-1);
    if (raw[lookahead] === ":" && keys instanceof Set) {
      const key = JSON.parse(raw.slice(start, index + 1));
      if (keys.has(key)) {
        throw new Error(`producer contract contains duplicate key: ${key}`);
      }
      keys.add(key);
    }
  }

  return JSON.parse(raw);
}

export function readNumericRuntimeVersion(versionHeaderPath = path.join(repositoryRoot, "mods", "src", "version.h")) {
  const versionHeader = readFileSync(versionHeaderPath, "utf8");
  const parts = ["MAJOR", "MINOR", "REVISION", "PATCH"].map((part) => {
    const match = versionHeader.match(new RegExp(`#define VERSION_${part}\\s+(\\d+)`));
    if (!match) {
      throw new Error(`missing VERSION_${part} in ${versionHeaderPath}`);
    }
    return match[1];
  });
  return parts.join(".");
}

export function buildBattleBridgeRuntimeManifest({
  dllBytes,
  sourceRevision,
  contract,
  runtimeVersion = readNumericRuntimeVersion(),
}) {
  if (!(dllBytes instanceof Uint8Array) || dllBytes.byteLength === 0) {
    throw new Error("version.dll must be non-empty");
  }
  if (!/^[0-9a-f]{40}$/u.test(sourceRevision)) {
    throw new Error("source revision must be a lowercase 40-character commit SHA");
  }
  if (contract?.contractSchema !== "stfc.battle-bridge.producer-capabilities.v1") {
    throw new Error("unsupported Battle Bridge producer contract");
  }

  const runtimeCapabilities = contract.runtimeCapabilities.map((entry) => entry.id);
  if (new Set(runtimeCapabilities).size !== runtimeCapabilities.length) {
    throw new Error("producer contract contains duplicate runtime capability IDs");
  }

  return {
    manifestSchema: 1,
    distributionId: contract.distributionId,
    runtimeVersion,
    sourceRevision,
    capabilities: ["settings.principal-taxonomy.v1", ...runtimeCapabilities],
    settingsCatalog: {
      schemaVersion: 1,
      revision: "guffawaffle-taxonomy-2026-07-29",
    },
    producerContract: {
      schema: contract.contractSchema,
      capabilityEvidencePin: contract.capabilityEvidencePin,
      runtimeCapabilities: contract.runtimeCapabilities,
      artifact: {
        fileName: "version.dll",
        size: dllBytes.byteLength,
        sha256: createHash("sha256").update(dllBytes).digest("hex"),
      },
      compatibilityEvidenceOnly: true,
      operationalActivation: "requires-bridge-transactional-binding",
    },
  };
}

export function generateBattleBridgeRuntimeManifest({ dllPath, outputPath, sourceRevision }) {
  const contract = parseAuthoritativeProducerContract(readFileSync(
    path.join(repositoryRoot, "docs", "battle-bridge-producer-capabilities.v1.json"),
    "utf8",
  ));
  const manifest = buildBattleBridgeRuntimeManifest({
    dllBytes: readFileSync(dllPath),
    sourceRevision,
    contract,
  });
  writeFileSync(outputPath, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
  return manifest;
}

function parseArguments(argv) {
  const values = new Map();
  for (let index = 0; index < argv.length; index += 2) {
    const key = argv[index];
    const value = argv[index + 1];
    if (!key?.startsWith("--") || value === undefined) {
      throw new Error("expected --dll, --output, and --source-revision values");
    }
    values.set(key, value);
  }
  return {
    dllPath: values.get("--dll"),
    outputPath: values.get("--output"),
    sourceRevision: values.get("--source-revision"),
  };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const options = parseArguments(process.argv.slice(2));
    if (!options.dllPath || !options.outputPath || !options.sourceRevision) {
      throw new Error("expected --dll, --output, and --source-revision values");
    }
    const manifest = generateBattleBridgeRuntimeManifest(options);
    process.stdout.write(`${JSON.stringify({
      output: path.resolve(options.outputPath),
      sourceRevision: manifest.sourceRevision,
      artifactSha256: manifest.producerContract.artifact.sha256,
      capabilities: manifest.capabilities,
    }, null, 2)}\n`);
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  }
}
