# Battle Bridge Producer Contract

This repository contains producer-side groundwork for three portable Battle
Bridge runtime capabilities:

- `ingest.stfc-sidecar.v1` — authenticated, bounded local ingest using outer
  protocol `stfc.sidecar.ingest.v1`;
- `battle.capture.v1` — canonical Battle capture using
  `stfc.battle.capture.v1`; and
- `fleet.runtime-snapshot.v1` — Fleet runtime observations using
  `stfc.fleet.runtime_snapshot.v1`.

The machine-readable contract is
[`battle-bridge-producer-capabilities.v1.json`](battle-bridge-producer-capabilities.v1.json).
It pins the reviewed Sidecar golden-corpus inventory. After this work reaches a
trusted CI or release build, that workflow emits `stfc-runtime-manifest.json`
beside the exact `version.dll`. The manifest names the three producer
declarations and binds them to the DLL's size, SHA-256, source commit, and
numeric version. The tagged-release workflow also inventories those exact JSON
bytes as the optional `windows-mod-runtime-manifest-x64` artifact in the
Windows release manifest; both that schema-v1 manifest and the producer JSON
remain unsigned.

The runtime manifest is compatibility evidence, not authenticity, safety, or
gameplay authorization. Bridge releases predating the transactional consumer in
[`Guffawaffle/stfc-mod-bridge#134`](https://github.com/Guffawaffle/stfc-mod-bridge/issues/134)
use only their embedded provider manifest. Compatible Bridge releases may
install and consume the adjacent JSON only when its exact bytes and the DLL are
both bound by a launcher-bundled reviewed release certification; neither this
unsigned JSON nor the unsigned schema-v1 release manifest can activate a
capability on its own. The first operational activation still requires a
trusted workflow to publish an exact pair and a deliberate launcher catalog
update to review that pair. The wave-one current-release corpus remains
unchanged with zero accepted runtime contracts. Another provider can publish
any compatible subset without copying the Guffawaffle distribution ID.

## Deliberate boundaries

- Report, analytics, and catalog snapshot events remain source-observed only.
  They are not advertised as Battle Bridge runtime capabilities.
- Fleet alert evidence remains unproven because this producer has no matching
  emitter. It is not advertised.
- A disabled or incomplete `[sidecar.sync]` configuration starts no local
  worker and performs no local transport request. Enabled hooks copy into a
  bounded asynchronous queue; connect and total request timeouts remain 2.5
  and 8 seconds.
- `sidecar.sync.transport = "named_pipe"` is the Windows Battle Bridge path.
  It requires a bare safe pipe name plus the exact 43-character unpadded
  base64url projection of the launcher-owned 32-byte credential. It uses
  length-prefixed byte frames, protocol
  `stfc.battle-bridge.local-ipc.v1`, role `stfc-mod-runtime`, operation
  `ingest`, and a closed bounded response contract. Each frame begins with an
  unsigned 32-bit little-endian byte count; the authentication header and each
  response are capped at 4 KiB, and payload frames at 512 KiB. The client uses
  Windows `SecurityIdentification` quality-of-service so the pipe server can
  identify it without receiving an impersonation-capable token. It does not
  construct a URL, proxy, TLS policy, socket, or network listener. Invalid
  explicit modes, names, and credentials fail closed rather than falling back
  to HTTP.
- `legacy_http` remains the default only for compatibility with older Sidecar
  installations. It retains the existing URL/proxy/TLS behavior and is not the
  Battle Bridge transport. Named-pipe failure never falls through to it.
- Canonical local Battle events are losslessly chunked above 256 KiB in 64 KiB
  pieces. Legacy non-Majel battle delivery is unchanged. Majel targets reject
  both `Battles` and `BattlelogsRealtime` before envelope construction, so raw
  journals and capture tokens cannot enter a Majel request body and partial
  capture groups cannot be lost to its bounded queue.
- Repeated JSONL bounded-suffix warnings are coalesced to at most one report per
  minute, with the next report carrying the suppressed count.

The named-pipe response carries acknowledgement state only; it is not a command
channel. No cloud destination, gameplay automation, or new persistent secret is
introduced by this contract. The existing plaintext TOML token is a
launcher-managed projection of the authoritative DPAPI record and is never
logged. Battle Bridge remains responsible for the pipe ACL, caller-process and
runtime-evidence validation, protocol version, and per-operation authorization;
the header's requested role is not caller proof.
