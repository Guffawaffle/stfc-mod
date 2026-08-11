#!/usr/bin/env node

import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  buildBattleBridgeRuntimeManifest,
  parseAuthoritativeProducerContract,
} from "./generate_battle_bridge_runtime_manifest.mjs";

const contract = parseAuthoritativeProducerContract(readFileSync(
  new URL("../docs/battle-bridge-producer-capabilities.v1.json", import.meta.url),
  "utf8",
));
const embeddedProviderManifest = JSON.parse(readFileSync(
  new URL("../docs/windows-launcher/runtime-manifest.guffawaffle.v1.json", import.meta.url),
  "utf8",
));

test("producer contract freezes only declarations justified by bounded fixture evidence", () => {
  assert.equal(contract.contractSchema, "stfc.battle-bridge.producer-capabilities.v1");
  assert.equal(contract.distributionId, "guffawaffle.stfc-community-mod");
  assert.equal(contract.distributionId, embeddedProviderManifest.distributionId);
  assert.equal(contract.activationStatus, "producer-groundwork");
  assert.deepEqual(
    contract.runtimeCapabilities.map((entry) => [entry.id, entry.schema]),
    [
      ["ingest.stfc-sidecar.v1", "stfc.sidecar.ingest.v1"],
      ["battle.capture.v1", "stfc.battle.capture.v1"],
      ["fleet.runtime-snapshot.v1", "stfc.fleet.runtime_snapshot.v1"],
    ],
  );
  assert.deepEqual(
    contract.runtimeCapabilities.map((entry) => entry.evidenceStatus),
    ["payload-fixture-only", "payload-fixture-only", "payload-fixture-only"],
  );
  assert.deepEqual(
    contract.sourceObservedOnly.map((entry) => entry.id),
    ["battle.report.v0", "battle.analytics.v0", "battle.catalog-snapshot.v0"],
  );
  assert.deepEqual(contract.unproven.map((entry) => entry.id), ["fleet.alert-evidence.v0"]);
  assert.deepEqual(contract.transportPolicy, {
    majelBattlelogs: "unsupported",
    majelBattlelogsRealtime: "unsupported",
    legacyBattlelogs: "unchanged",
    legacyBattlelogsRealtime: "unchanged",
    canonicalLocalBattleStream: "unchanged",
  });
  assert.equal(
    contract.capabilityEvidencePin.sha256,
    "aa6b738eab60a3bcb17a0c37cfcd533198096dd05d98abfd4a43308e891a9cc9",
  );
});

function readIntegerConstant(relativePath, name) {
  const source = readFileSync(new URL(`../${relativePath}`, import.meta.url), "utf8");
  const match = source.match(new RegExp(`${name}\\s*=\\s*([0-9' *]+);`, "u"));
  assert.ok(match, `missing ${name} in ${relativePath}`);
  return match[1]
    .split("*")
    .map((factor) => Number.parseInt(factor.trim().replaceAll("'", ""), 10))
    .reduce((product, factor) => product * factor, 1);
}

test("published producer bounds remain contract-checked against runtime source", () => {
  assert.equal(
    contract.bounds.localQueueDepth,
    readIntegerConstant("mods/src/patches/sidecar_local_ingest.cc", "kSidecarLocalQueueMaxDepth"),
  );
  assert.equal(
    contract.bounds.localConnectTimeoutMs,
    readIntegerConstant("mods/src/patches/sidecar_local_ingest.cc", "kSidecarLocalConnectTimeoutMs"),
  );
  assert.equal(
    contract.bounds.localRequestTimeoutMs,
    readIntegerConstant("mods/src/patches/sidecar_local_ingest.cc", "kSidecarLocalRequestTimeoutMs"),
  );
  assert.equal(
    contract.bounds.localChunkThresholdBytes,
    readIntegerConstant("mods/src/patches/sidecar_local_chunking.h", "kChunkingThresholdBytes"),
  );
  assert.equal(
    contract.bounds.localChunkDataBytes,
    readIntegerConstant("mods/src/patches/sidecar_local_chunking.h", "kChunkDataBytes"),
  );
  assert.equal(
    contract.bounds.warningCoalescingIntervalMs,
    readIntegerConstant("mods/src/patches/sync_battle_logs.cc", "kBattleFeedWarningIntervalMs"),
  );
});

test("authoritative producer JSON rejects duplicate keys", () => {
  assert.throws(
    () => parseAuthoritativeProducerContract('{"contractSchema":"one","contractSchema":"two"}'),
    /duplicate key: contractSchema/u,
  );
});

test("exact-build runtime manifest binds compatibility declarations to DLL bytes without secrets", () => {
  const dllBytes = Buffer.from("synthetic-version-dll", "utf8");
  const sourceRevision = "0123456789abcdef0123456789abcdef01234567";
  const manifest = buildBattleBridgeRuntimeManifest({
    dllBytes,
    sourceRevision,
    contract,
    runtimeVersion: "2.1.0.0",
  });

  assert.equal(manifest.manifestSchema, 1);
  assert.equal(manifest.sourceRevision, sourceRevision);
  assert.deepEqual(manifest.capabilities, [
    "settings.principal-taxonomy.v1",
    "ingest.stfc-sidecar.v1",
    "battle.capture.v1",
    "fleet.runtime-snapshot.v1",
  ]);
  assert.deepEqual(manifest.settingsCatalog, embeddedProviderManifest.settingsCatalog);
  assert.equal(manifest.producerContract.compatibilityEvidenceOnly, true);
  assert.equal(manifest.producerContract.operationalActivation, "requires-bridge-transactional-binding");
  assert.equal(manifest.producerContract.artifact.fileName, "version.dll");
  assert.equal(manifest.producerContract.artifact.size, dllBytes.byteLength);
  assert.equal(
    manifest.producerContract.artifact.sha256,
    createHash("sha256").update(dllBytes).digest("hex"),
  );
  assert.doesNotMatch(JSON.stringify(manifest), /token|authorization|https?:\/\//iu);
});

test("runtime manifest rejects malformed source identity and empty artifacts", () => {
  assert.throws(
    () => buildBattleBridgeRuntimeManifest({
      dllBytes: Buffer.from("dll"),
      sourceRevision: "main",
      contract,
    }),
    /40-character commit SHA/u,
  );
  assert.throws(
    () => buildBattleBridgeRuntimeManifest({
      dllBytes: Buffer.alloc(0),
      sourceRevision: "0123456789abcdef0123456789abcdef01234567",
      contract,
    }),
    /must be non-empty/u,
  );
});
