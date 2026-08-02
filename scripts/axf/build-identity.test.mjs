import test from "node:test";
import assert from "node:assert/strict";
import {
  assertIdentityMatches,
  createAxBuildIdentity,
  identityEnvironment,
  identityXmakeArguments,
  parseIdentityComment
} from "./build-identity.mjs";

const provenance = {
  sourceStateId: `dirty-sha256:${"a".repeat(64)}`,
  baseCommit: "b".repeat(40)
};

test("AX identity correlates provenance, configure arguments, and environment", () => {
  const identity = createAxBuildIdentity(provenance, {
    buildInvocationId: "ax:00000000-0000-4000-8000-000000000000",
    buildMode: "releasedbg"
  });

  assert.equal(identity.sourceStateId, provenance.sourceStateId);
  assert.equal(identity.baseCommit, provenance.baseCommit);
  assert.equal(identity.buildMode, "releasedbg");
  assert.equal(identityEnvironment(identity).STFC_BUILD_INVOCATION_ID, identity.buildInvocationId);
  assert.ok(identityXmakeArguments(identity).includes(`--stfc_source_state_id=${provenance.sourceStateId}`));
});

test("schema-v1 PE comment round-trips and matches the expected cycle", () => {
  const identity = createAxBuildIdentity(provenance, {
    buildInvocationId: "ax:00000000-0000-4000-8000-000000000000"
  });
  const parsed = parseIdentityComment(
    `stfc-identity-v1;distribution=${identity.distributionId};source=${identity.sourceStateId};` +
      `base=${identity.baseCommit};build=${identity.buildInvocationId};mode=${identity.buildMode};` +
      `channel=${identity.buildChannel}`
  );

  assert.deepEqual(assertIdentityMatches(parsed, identity), identity);
});

test("malformed, future, and conflicting identities fail closed", () => {
  const identity = createAxBuildIdentity(provenance, {
    buildInvocationId: "ax:00000000-0000-4000-8000-000000000000"
  });
  assert.throws(() => parseIdentityComment("stfc-identity-v2;build=nope"), /schema-v1/);
  assert.throws(
    () => parseIdentityComment("stfc-identity-v1;distribution=x;source=x;base=x;build=x;mode=x"),
    /missing channel/
  );
  assert.throws(
    () => assertIdentityMatches({ ...identity, buildInvocationId: "ax:different" }, identity),
    /buildInvocationId/
  );
});

test("identity fields reject private or delimiter-bearing values", () => {
  assert.throws(
    () => createAxBuildIdentity(provenance, { buildInvocationId: "C:\\Users\\private" }),
    /safe build identity/
  );
  assert.throws(
    () => createAxBuildIdentity(provenance, { buildInvocationId: "ax:good;token=bad" }),
    /safe build identity/
  );
});
