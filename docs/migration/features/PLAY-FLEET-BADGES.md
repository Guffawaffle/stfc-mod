# PLAY-FLEET-BADGES — configured shortcuts on owned system fleets

Contract version: 1. Status: source-preserving implementation; current-candidate
execution and native qualification pending. Source scan: 2026-10-28.

## Inputs and scope

- Source: Downstream `frozen/play-dev-20261026`, exactly
  `1baa55fedf1758b19f843cc59707b8901a0b4998`; read through Git.
- Target: existing `play-dev-ripper` checkout, assessment base
  `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0`.
- Inventory: `PLAY-FLEET-BADGES`; shared lifecycle dependency
  `PLAY-FLEET-LABELS`, selection relationship `PLAY-FLEET-SELECT`.
- Scope: preserve the system fleet's existing native text owner while displaying
  its actual mod shortcut. No new badges, selection behavior, settings schema,
  saved state, input handler, hook registry or UI framework.
- The delegated owner implements only badge/label sources, their two contracts
  and the production-consumer fixture fragment. The coordinator owns shared
  fixture/build registration, inventory, all execution and native qualification.

| Inspected branch | Exact revision | Presence | Evidence and differences |
| --- | --- | --- | --- |
| upstream dev | `e884efa7c6ff66d0776b4cd4d6b76ce193296be5` | Named consumer absent | No `ship_hotkey_badges` setting or `ship_shortcut_badges` source in the inspected tree; native keybind hints are a separate feature. |
| PLAY | `cad98ef7990dd66167eb64ea5dda525c1fe2c296` | Consumer and controls present | `mods/src/patches/parts/ship_shortcut_badges.cc`, `settings/fleet_labels.cc` and configuration paths. |
| play-dev / frozen source | `1baa55fedf1758b19f843cc59707b8901a0b4998` | Consumer and controls present | Complete badge implementation/header, zoom lifecycle/frame callers, MapKey hints, native bindings, config/defaults and registered Settings control assessed. |
| refactor | `5ec09aa0f273cfaa7af2d6d6a81b5404c9aadbc0` | Consumer and controls present | `src/mod/features/ship_shortcut_badges.cc`, shared lifecycle in `fleet_labels.cc`, `settings/config_settings.cc`; existing implementation, not an absent port. |

Both platforms share this behavior. `[patches].zoomhooks` controls installation;
`graphics.ship_hotkey_badges` defaults true and changes live. Display additionally
requires installed/enabled mod hotkeys and excludes Scopely hotkey mode. The
original registered control is on Fleet Labels; the target catalog retains the
same TOML identity and preserving writer. A badge update itself writes no file.

## Source workflow and ownership

Frozen `mods/src/patches/parts/ship_shortcut_badges.cc` owns metadata availability,
`Bind`, `Release` and the 250-ms `Refresh`. Its header promises no private hooks.
`parts/zoom.cc` owns fleet enable/bind/disable/release and calls refresh from the
shared navigation update. On context binding it releases the old badge **before**
the native original, then binds the new context afterward. Disable and context
release also restore before the original.

The frozen badge selects local-player fleets only, resolves the native
`FleetsManager.GetPlayerFleetIndex(ID)` in range 0..7, then uses the first enabled
MapKey binding. `patches/mapkey.cc` compacts modifiers/keys for the limited native
font; `prime/NavigationFleetWidget.h`, `FleetDeployedData.h`, `FleetsManager.h`
and `il2cpp/method_contract.h` supply the original native boundary. A failed text
write does not advance the applied-hint cache. Disabled/native mode clears its
override, exposing the game's own dock text.

The typed target already uses process-lifetime weak widget records, operation-local
strong roots, observed fleet identity, generations and snapshotted frame keys.
The shared `EnsureNavigationFleetLifecycle` remains the sole widget hook owner.
`features/ship_shortcut_hint.h` maps native dock index to the actual FleetData
position used by `RequestSelect`, preserving the existing gapped/reordered-dock
correction. The acquired FleetData collection and every observed element remain
borrowed until explicitly rooted; retaining only the widget is not collection
custody. Shared `Shortcut::Hint` owns formatting and first-binding selection.

## Behavioral obligations

| ID | Source behavior and evidence | Target mapping / necessary adaptation | Verification method and result |
| --- | --- | --- | --- |
| FB1 | `Methods` validates native label/local-player/index/text contracts; unavailable setup preserves dock letters. | Typed installer already checks the corresponding metadata and shared lifecycle. A truthful feature report controls Settings availability. | Production installer/control availability and missing-contract cases; execution pending. |
| FB2 | `Bind` releases an old record, requires a local fleet and valid manager index, then applies immediately. | Existing checked local/ID/index reads and positive ID are retained. Valid index zero is distinct from failed observation. | Actual `Bind` consumer with local/foreign/null, unavailable reads, zero/large ID and index -1/0/7/8. Pending. |
| FB3 | First enabled configured binding supplies compact text; no binding gives native text. `MapKey::GetShortcutHint`. | Reuse `Shortcut::Hint`, including current alternatives and modifier spelling; no duplicate parser. | Existing `tests/mod_logic.cc::ShipHints` and chord fixtures establish pure mapping/formatting only. Native text-consumer tests pending. |
| FB4 | Native fleet index selects the source badge action. | Retain the accepted positional mapping through `ShipShortcutHintAction`. Root the bar/context/vector and all inspected elements, use checked Index reads, then verify vector/context/slot identity. Unavailable evidence no longer becomes dock zero. | Real hint consumer with gapped order, valid zero, unavailable/throwing Index and in-place vector replacement. Fixture implemented; execution pending. |
| FB5 | `Update` writes only when desired text differs and caches only successful writes. | Reacquire key/generation after managed hint/identity queries. Track an attempted override separately from successful applied text, allowing release during a native text callback to clean its partial operation. | Release during hint acquisition and override callback, failed native write and retry. Fixture implemented; execution pending. |
| FB6 | `Refresh` updates visible badges every 250 ms, including live setting and binding changes. | Retain shared feature-frame delivery and key snapshots; no second camera/input owner. | Production refresh with disable/re-enable, native mode, hotkeys off and binding changes; deterministic timing where needed. Pending. |
| FB7 | `Release` erases bookkeeping and clears an owned override before native disable/release. | The binding retains the actual overridden localizer weakly; release roots that old owner instead of reading a replacement field. Pending native override is cleanup authority, never saved/success evidence. | Old/new localizer ownership, in-flight release, foreign context and final weak/strong-handle release. Fixture implemented; failed native clear remains source-compatible, not a guaranteed restoration. |
| FB8 | Frozen `NavigationFleetWidget_OnDidBindContext_Hook` releases before original, then binds after. | Restore this source ordering in the existing shared lifecycle callback. | Actual callback replaces `_fleetLevel` inside original; old override cleared first, new label gets only its own hint. Fixture implemented; execution pending. |
| FB9 | Widgets without context at enable do not receive a badge; disable/release leave native lifecycle intact. | Reuse shared lifecycle and original exactly once. Stale generation must not publish after replacement. | Enable-before-context, repeated bind, disable and release reentry with original-call counts. Pending. |
| FB10 | Registered control/defaults preserve the existing live key and save status. | Existing catalog/writer is reused; badge rendering is independent of save completion. | Catalog/default/parser tests plus eventual native live toggle and persistence trial. No new writer. |

## Design and behavior decisions

The target's positional dock mapping is a deliberate existing correction to the
frozen badge's native-index assumption; do not regress it to a direct eight-key
lookup. Missing evidence must produce no advertised shortcut rather than claim
that the first dock is selected. The original source also caches its index until
bind; any broader dynamic-index policy is a separate decision, not an inferred
missing feature.

Weak records, exact-generation checks and reentrant-safe snapshots improve the
old raw-pointer map while preserving native text ownership. The specific stale
iterator and release-order differences are corrected in the existing consumers;
they do not introduce a new UI lifecycle system. A provisional bind generation
also prevents reentrant source queries from replacing a newer record.

## Validation evidence

Historical Windows batch 01 already has explicit player acceptance of owned ship
shortcut labels and native other-player labels. Its DLL SHA-256 is
`3fcd225bdb98b3260f196c7e85fa2545f932626eea0a6ee8e2bac61e5462b0c5`;
see [the recorded native checkpoint](../PLAY-RESTORATION-PLAN.md#native-development-checkpoints--25-october-batches-0103).
That is genuine prior behavior evidence, not qualification of a later candidate.

Existing `tests/mod_logic.cc` tests gapped/reordered native-to-position mapping,
unbound actions, first alternative and compact hints. These are pure helpers,
not badge `Bind`/`Update`/`Release`, managed collection custody or rendered labels.
`tests/fleet_presentation_bindings_cases.h` now includes the actual badge and
label sources. Its `fleet-presentation-native` and
`fleet-presentation-missing-index` cases exercise the obligations above through
controlled native metadata and method bodies. Only configured hint text is
substituted; no badge algorithm is reproduced. No tests/builds/native actions
were executed by this delegated implementation pass.
Current-candidate execution and artifact identities belong to the coordinator's
cohort record. Windows native and macOS qualification remain distinct.

## Disposition and recovery

- Migration coverage: source counterpart and control already present; complete
  workflow assessed at the stated base, with bounded typed/lifecycle corrections
  and actual-consumer fixtures implemented.
- Correctness: older basic Windows behavior accepted; current pooling,
  reentry/failed-observation and cleanup cases require the listed checks.
- Native procedure: observe own/foreign fleets; change first shortcut binding;
  test a gapped dock order, toggle badge/mod/native-hotkey modes; leave/reenter
  system and reuse widgets. Verify native dock text returns without stale text.
- Next action: coordinator exercises actual consumers and records exact
  source/build/player/platform evidence. Preserve
  earlier acceptance and unresolved limits; no overall-parity claim.

## Coordinator qualification checkpoint — 2026-10-28

Implementation `d61b383d9e3b83d5c8ea4852c8c495e35d1a9084` passed the combined Windows assertion suite
(399/399 build/run/install steps), native AAI-enabled build (41/41), and
main-game startup with all 73 groups and frame delivery ready. See
[the cohort receipt](../FEATURE-COHORT-20261028-01.md#native-qualification) for
exact artifact, store differences and diagnostic limits. This supersedes earlier
execution-pending notes above only for the exercised fixture and startup layers.
Current player workflow and macOS acceptance remain pending.
