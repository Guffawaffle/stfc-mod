# SYNC-RECORDS — owned records, targets and native intake

Contract v1, 2026-11-02. Existing `play-dev-ripper` checkout, clean starting head
`5c71cf56e56aaabdabf0f48fdb9287dd4c3d30ea`. Frozen preservation source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Scope: existing sync, not AAI,
new upstream functionality, live target configuration or a new persistence store.

| Inspected local ref | Exact revision | Presence |
| --- | --- | --- |
| upstream/dev | e884efa7c6ff66d0776b4cd4d6b76ce193296be5 | Original sync owner/config present in `mods/src/patches/parts/sync.cc` and `mods/src/config.{h,cc}`. |
| origin/play | cad98ef7990dd66167eb64ea5dda525c1fe2c296 | Same public producer/config domain, before the frozen play-dev additions. |
| origin/play-dev and frozen/play-dev-20261026 | 1baa55fedf1758b19f843cc59707b8901a0b4998 | Selected complete sync owner, EntityGroup bindings, session headers, bounded queues and config. |
| refactor base | 5c71cf56e56aaabdabf0f48fdb9287dd4c3d30ea | `src/mod/sync/{sync,records,state,upload,battlelogs}.cc`, shared headers, `game/server.h`, Config and existing fixtures. |

These are exact retained local observations, not a fresh remote-head claim.

## Ownership and obligations

Native response/RTC hooks copy admitted bytes and local identity into the bounded
owned-input queue. A single processing worker decodes those jobs. Canonical
immutable records belong to RecordStore; each target owns its revision, delivery
cursor, retry/ACK state and uploader. Session generation fences stale work.
Config reload publishes immutable snapshots and the frame owner follows targets.
Runtime configuration and state file owners are unchanged.

| ID | Selected source contract / retained decision | Target mapping and verification layer |
| --- | --- | --- |
| R1 | Global defaults false after parsing; target inheritance/overrides, singular officer alias, legacy url/token pair, proxy/TLS and retired file output. Frozen `config.cc:594–659,1316–1391`. | Existing Config reader/immutable Sync snapshots and parser tests; constructor-only true defaults are dormant, not missing behavior. No config/default changes. |
| R2 | Preserve reachable record types/keys/fields, supported slots and three removal forms. Frozen sync owner: away1408–1532, buffs1666–1721, Haven1752–1867, inventory1869–1914, jobs1916–2008, missions/officers/research/ships/tech/traits1597–1658,2010–2156,2415–2492; JSON2494–2629 and slots2284–2413,2631–2734. | `records.cc`, stream specs in `state.cc`; production encoder/Delivery fixtures. Unsupported record kinds stay filters. Frozen BattleReport is a logging placeholder. |
| R3 | Every emitted Haven-building batch carries the full-sync marker. Frozen `sync.cc:1829–1830`. | Restore `kHaven` to Always. Two-building fixture checks first/changed/local-owner batches and unchanged suppression. Retain canonical full-snapshot batching; see explicit difference below. |
| R4 | Native bodies/results still execute once; copy payloads while borrowed native data is valid. Required input loss cannot masquerade as observation. | Checked binding results, one qualified collection count/item scan, rooted managed references/byte vector and immediate owned queue copy. Actual production intake hooks run in a controlled managed host; no native worker/HTTP starts. |
| R5 | Unknown data is distinct from valid false/zero/null/empty. Preserve accepted target empty baselines and optional enrichment. | Checked type/group/RTC/identity acquisition; unavailable required evidence seals captured generation. Real observed byte[0]/empty collections remain valid. Unsupported types are classified before payload acquisition; optional name failures remain best effort. |
| R6 | Accepted M8 per-target 2xx ACK, failure retry/coalescing, removals after uncertain writes and revision/generation fences. | Existing real RecordStore/Delivery tests; production CPR uploader against owned synthetic loopback, including held-response reconfiguration and retirement/re-add. This simulates registry ownership; it does not invoke native FollowSettings. |
| R7 | Source credentials/payloads never outlive their session as authorizing work; terminal processing failure cannot appear healthy. | Existing InputHealth/owned dispatch/session tests and intake failure/recovery fixtures. In-flight HTTP cannot be retracted; diagnostic best-effort delivery remains independent/default-off. |
| R8 | Legacy sent IDs are attempted admission, not account/target ACK. Accepted prior decision: preserve file/path untouched and allow restart resend. | Existing restart fixture and M8 decision retained. No new persistent history/account inference/exactly-once claim. |
| R9 | Platform paths and native ABI/fit remain explicit; no new hook ownership. | Existing signatures/installers unchanged. Current Windows compilation/controlled host and Mac pre-release user testing are distinct evidence. No new Mac qualification is inferred from local Windows checks. |

Review additionally exposed a shared matching defect: IL2CPP makes Object
assignable from unmanaged-pointer classes. Bound reference slots now require
managed-reference metadata kinds before assignability, preserving untyped-pointer
compatibility. Prime and Platform pointer modes reproduce the initial failure
and check refusal before getter invocation under the real Object rule.

## Deliberate differences and correction boundaries

Frozen Haven changed subsets were marked full. The target's existing Delivery
policy sends the complete cached set for a full-marked batch. This correction
restores the source marker while retaining that canonical snapshot policy; it is
not a claim of identical batching or verified remote receiver behavior.

The refactor already fixes source defects around empty initial/mission baselines,
slot content changes, safe empty fleet presets, emerald parsing and unsorted
completed missions. Keep those accepted improvements. In particular, a successfully
observed empty protobuf is valid; unavailable byte storage must not invent one.
Profile/store-offer producers are existing target-era functionality absent from
frozen source, not omitted features to port. Their intake safety remains covered.

The separate SYNC-BATTLES source audit found a target-interest gap around delayed
journal requests after removing the last target. It remains a named correction
follow-up, not closed by uploader retirement tests. No real game-server request
is required for its eventual regression test.

## Evidence and remaining acceptance

The [cohort](../FEATURE-COHORT-20261102-SYNC-RECORDS.md) binds current failing-before
cases, final source hashes, build/test commands, review and checkpoints.
Source inspection, controlled native adapter, pure encoder/delivery and actual
local HTTP uploader are separate layers. None establishes native client hook fit,
live external receiver behavior or comprehensive Mac acceptance. Overall parity
remains open; AAI is deferred by current user direction.
