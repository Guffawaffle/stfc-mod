# Probe: Standard Recruit transaction-limit discovery

- Status: completed; temporary instrumentation removed
- Owner: Guffawaffle / stfc-mod
- Date: 2026-07-30 through 2026-07-31
- Related patch label: chest-purchase-slider discovery logging
- Related timeline refresh ID: 2026-07-30 current-client corpus
- Related diff report: N/A
- Native seam ledger entry: Standard Recruit custom-quantity purchase submission and failure callbacks

## Question

Can the Standard Recruit Chest's observed inclusive transaction ceiling of 160 be discovered from client-visible
bundle, feature-config, request, or failure data before relying on human boundary testing?

## Static Evidence

- `InventoryManager.GetChestsMaxPurchaseCustomQuantity()` reads
  `FeaturesConfig.ChestsMaxPurchaseCustomQuantity` or
  `FeaturesConfig.FactionStoreChestsMaxPurchaseCustomQuantity` and otherwise returns 50.
- The Standard Recruit popup received the native client value 50 before the mod changed it.
- Current-client disassembly of `ShopShowcaseViewController.TryPurchaseMultipleChests(ResourceCostData, uint)`,
  `ShopSceneManager.TryPurchaseChest(...)`, and `ShopSceneManager.RequestAction(...)` shows the selected quantity
  forwarded into the request path without a comparison against 160.
- The generated `Bundle` model exposes purchase state, protobuf metadata, and `AllowCustomQuantity`, but its named
  fields do not expose a maximum custom claim quantity.
- Human boundary evidence already establishes success at 159 and 160 and rejection at 161 for Standard Recruit,
  despite sufficient claim resources.

## Risk

- Risk class: R2 managed log-only
- Confidence rung: payload understood
- Payload confidence: typed `ShopShowcaseViewController`, `Bundle`, quantity, `GSError`, and
  `BundleRewardClaimContext` parameters from current-client signatures. The captured error maps exactly to
  `PlatformError.ReponseCodes.InvalidBundleQuantity`.
- Original/trampoline confidence: submission and common loot-chest failure detours both executed and called their
  originals once with unchanged arguments.
- Behavior change expected: no

## Implementation Plan

- Temporary module/file: `ChestPurchaseDiscoveryHooks`, formerly in `mods/src/patches/parts/misc.cc` and removed
  after capture
- Guard: Windows only, compile-time discovery switch, and `extend_chest_purchase_max > 160`
- Hook descriptors:
  - `ShopShowcaseViewController.TryPurchaseMultipleChests`
  - `ShopSceneManager.OnOpenLootChestFailure(GSError, BundleRewardClaimContext)`
- Log tag: `ChestPurchaseDiscovery`
- Logged data:
  - submitted quantity and target bundle pointer
  - bundle ID and named quantity/purchase-related fields
  - bounded generated `Bundle.ToString()` diagnostic output
  - failure reason and `GSError` type, code, HTTP code, category, message, transaction ID, and request URL

The first runtime action proved that the popup-local
`ShopShowcaseViewController.OnPurchaseFailedEventHandler` is not used for this rejection. The second action likewise
proved that the scene-level premium-store failure event is not used. Static tracing then identified Standard Recruit
as the loot-chest action path: the generated `TryRequestActionBundleType` failure callback forwards its `GSError` to
the two-parameter `ShopSceneManager.OnOpenLootChestFailure` common implementation before the generic platform-error
dialog is shown.

## Disable Path

- The temporary descriptors, detours, and install blocks were removed after capture.
- No request values, game assets, or persistent data are modified.

## Human Smoke Test

Goal:

Capture one rejected Standard Recruit purchase at quantity 161 and determine whether the client-visible preflight or
failure payload contains the authoritative boundary.

Steps:

1. Cycle the instrumented Windows release build.
2. Open Standard Recruit Chest custom quantity.
3. Enter or select 161.
4. Confirm the purchase once.
5. Stop after the expected failure dialog.

Expected log markers:

- `[ChestPurchaseDiscovery] submit ... quantity=161`
- `[ChestPurchaseDiscovery] failure ...`

Stop immediately if:

The client crashes or hangs, a different bundle is targeted, the requested quantity changes, or resources are spent
despite the expected rejection.

## Result

The Windows release probes installed without missing methods, duplicate owners, or detour failures. Submission
captures identified Standard Recruit bundle `145512548` and confirmed that quantities 161 and 520 were forwarded
unchanged. The common loot-chest failure handler then captured both rejections:

- `GSError.Type = PLATFORM`
- `GSError.Code = 24`, which the current corpus names `InvalidBundleQuantity`
- `GSError.Message = "Can not purchase chosen quantity"`
- HTTP response code `0`, with empty category, transaction ID, and request URL
- null `BundleRewardClaimContext`

The quantity-161 rejection arrived about 134 ms after submission. Neither the bundle metadata nor the failure
payload contained the accepted maximum. Existing human boundary evidence remains the only source for the inclusive
Standard Recruit ceiling: 159 and 160 succeed; 161 fails despite sufficient resources. A subsequent human test of a
different recruit chest independently reproduced the boundary: 160 succeeded and 161 failed.

## Exit Decision

The 160 ceiling is enforced by the loot-chest platform/action service but is not dynamically discoverable from the
current client-visible bundle, request path, or failure payload. The repeated recruit-chest evidence supports locking
the release UI extension to 160 without claiming that every chest service shares the same server limit. Clamp larger
configured values to 160 and retain any higher future native ceiling. The temporary discovery hooks were removed;
this probe and the seam ledger preserve the evidence.
