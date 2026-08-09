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
  worker and performs no local HTTP request. Enabled hooks copy into a bounded
  asynchronous queue; connect and request timeouts remain 2.5 and 8 seconds.
- Canonical local Battle events are losslessly chunked above 256 KiB in 64 KiB
  pieces. Legacy non-Majel battle delivery is unchanged. Majel targets reject
  both `Battles` and `BattlelogsRealtime` before envelope construction, so raw
  journals and capture tokens cannot enter a Majel request body and partial
  capture groups cannot be lost to its bounded queue.
- Repeated JSONL bounded-suffix warnings are coalesced to at most one report per
  minute, with the next report carrying the suppressed count.

No command channel, cloud destination, gameplay automation, or persistent
secret is introduced by this contract.
