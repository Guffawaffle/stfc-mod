const IDENTITY_PREFIX = "stfc-identity-v1";
const SAFE_VALUE = /^[A-Za-z0-9._:+/-]+$/;

export const BUILD_IDENTITY_SCHEMA_VERSION = 1;

function requireIdentityValue(name, value) {
  if (typeof value !== "string" || value.length === 0 || value.length > 160 || !SAFE_VALUE.test(value)) {
    throw new Error(`${name} is not a safe build identity value.`);
  }
  return value;
}

export function createAxBuildIdentity(
  sourceProvenance,
  {
    buildInvocationId,
    buildMode = "release",
    buildChannel = "local",
    distributionId = "guffawaffle.stfc-community-mod"
  } = {}
) {
  if (!sourceProvenance || typeof sourceProvenance !== "object") {
    throw new Error("Source provenance is required for an AX build identity.");
  }
  return {
    schemaVersion: BUILD_IDENTITY_SCHEMA_VERSION,
    distributionId: requireIdentityValue("distributionId", distributionId),
    sourceStateId: requireIdentityValue("sourceStateId", sourceProvenance.sourceStateId),
    baseCommit: requireIdentityValue("baseCommit", sourceProvenance.baseCommit),
    buildInvocationId: requireIdentityValue("buildInvocationId", buildInvocationId),
    buildMode: requireIdentityValue("buildMode", buildMode),
    buildChannel: requireIdentityValue("buildChannel", buildChannel)
  };
}

export function identityEnvironment(identity) {
  return {
    STFC_DISTRIBUTION_ID: identity.distributionId,
    STFC_SOURCE_STATE_ID: identity.sourceStateId,
    STFC_BASE_COMMIT: identity.baseCommit,
    STFC_BUILD_INVOCATION_ID: identity.buildInvocationId,
    STFC_BUILD_CHANNEL: identity.buildChannel
  };
}

export function identityXmakeArguments(identity) {
  return [
    `--stfc_distribution_id=${identity.distributionId}`,
    `--stfc_source_state_id=${identity.sourceStateId}`,
    `--stfc_base_commit=${identity.baseCommit}`,
    `--stfc_build_invocation_id=${identity.buildInvocationId}`,
    `--stfc_build_channel=${identity.buildChannel}`
  ];
}

export function parseIdentityComment(comment) {
  if (typeof comment !== "string" || !comment.startsWith(`${IDENTITY_PREFIX};`)) {
    throw new Error("DLL does not contain an STFC schema-v1 identity comment.");
  }
  const values = new Map();
  for (const field of comment.slice(IDENTITY_PREFIX.length + 1).split(";")) {
    const separator = field.indexOf("=");
    if (separator <= 0) throw new Error("DLL identity comment contains a malformed field.");
    const key = field.slice(0, separator);
    const value = field.slice(separator + 1);
    if (values.has(key)) throw new Error(`DLL identity comment repeats ${key}.`);
    values.set(key, requireIdentityValue(key, value));
  }
  const required = ["distribution", "source", "base", "build", "mode", "channel"];
  for (const key of required) {
    if (!values.has(key)) throw new Error(`DLL identity comment is missing ${key}.`);
  }
  if (values.size !== required.length) {
    throw new Error("DLL identity comment contains unsupported schema-v1 fields.");
  }
  return {
    schemaVersion: BUILD_IDENTITY_SCHEMA_VERSION,
    distributionId: values.get("distribution"),
    sourceStateId: values.get("source"),
    baseCommit: values.get("base"),
    buildInvocationId: values.get("build"),
    buildMode: values.get("mode"),
    buildChannel: values.get("channel")
  };
}

export function assertIdentityMatches(actual, expected) {
  for (const field of [
    "schemaVersion",
    "distributionId",
    "sourceStateId",
    "baseCommit",
    "buildInvocationId",
    "buildMode",
    "buildChannel"
  ]) {
    if (actual[field] !== expected[field]) {
      throw new Error(`DLL build identity ${field} does not match the AX cycle.`);
    }
  }
  return actual;
}
