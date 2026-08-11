# Probe: Chest purchase slider extension

- Status: release-promoted
- Owner: Guffawaffle / stfc-mod
- Date: 2026-07-30 through 2026-07-31
- Related patch label: chest-purchase-slider-experiment
- Related timeline refresh ID: 2026-07-30 current-client corpus
- Related diff report: N/A
- Native seam ledger entry: `Digit.Prime.Inventories.InventoryUseRowWidget.SetWidgetData()`

## Question

Can the client-side quantity ceiling for explicitly tagged chest-purchase rows be raised without changing any other
inventory, donation, or artifact-conversion slider?

## Static Evidence

- Symbol: `Digit.Prime.Inventories.InventoryUseRowWidget.SetWidgetData`
- Method signature: `void SetWidgetData()`, RVA `0x11A2890` in the 2026-07-30 corpus.
- String/script/config evidence:
  - `InventoryForPopup` retains the dedicated `IsChestPurchase` property and backing field at offset `0x90`.
  - `InventoryForPopup.MaxItemsToUse` is a signed 64-bit value at offset `0x20`.
  - `InventoryManager.GetChestsMaxPurchaseCustomQuantity` returns the generic or faction-store feature-config cap,
    with a fallback of 50.
  - Current-client disassembly shows `InventoryUseRowWidget.SetWidgetData` checking `IsChestPurchase` before
    configuring the chest row and reading `MaxItemsToUse`.
- Old/new diff context: this is a new release-supported patch; no prior chest-purchase slider hook exists.
- Why this target is the narrowest candidate: the row is already fully tagged before `SetWidgetData` runs, allowing
  the experiment to gate on the game's own chest-purchase discriminator. Earlier setters run before that tag exists,
  while a global `UnityEngine.UI.Slider` hook would affect unrelated UI.

## Risk

- Risk class: R5
- Confidence rung: state-correlated
- Payload confidence: typed managed `InventoryUseRowWidget` and its typed `InventoryForPopup` context only; no
  callback or opaque payload is interpreted.
- Original/trampoline confidence: runtime-observed returning successfully; the original is called once after the
  guarded context adjustment.
- Behavior change expected: yes

## Implementation Plan

- Module/file: `mods/src/patches/parts/misc.cc`
- Config or compile guard: `[ui].extend_chest_purchase_max`, Windows-only, default 160; non-positive values disable
  the hook, values from 1 through 160 are exposed, and larger configured values clamp to 160.
- Hook descriptor name: `InventoryUseRowWidget.SetWidgetData`
- Target assembly: `Assembly-CSharp`
- Target namespace: `Digit.Prime.Inventories`
- Target class: `InventoryUseRowWidget`
- Target method: `SetWidgetData`
- Install path: `InstallMiscPatches`
- Log tag or event kind: `ChestPurchaseSlider`

Registry requirements:

- Use `HookDescriptor`.
- Use `HookModuleHealth`.
- Use `HOOK_REGISTRY_SPUD_STATIC_DETOUR`.
- Do not use raw `SPUD_STATIC_DETOUR`.

## Disable Path

- Flag or code path to disable: set `[ui].extend_chest_purchase_max = 0`.
- File/entry to delete if it crashes: remove the one guarded install block and hook body from `misc.cc`.
- Expected boot log when disabled: `ChestPurchaseSliderHooks` reports the hook skipped because the configured
  maximum is non-positive.

## Human Smoke Test

Goal:

Verify that an eligible chest-purchase quantity slider reaches the configured higher ceiling while donation,
artifact-conversion, and ordinary inventory-use sliders retain their native ceilings.

Steps:

1. Set `extend_chest_purchase_max = 160`.
2. Start the client and open an eligible multi-purchase chest popup.
3. Move the quantity slider to its maximum without confirming a purchase.
4. Open one donation or ordinary inventory-use popup and confirm its ceiling is unchanged.
5. Disable the flag, restart, and confirm the native chest ceiling returns.

Expected log marker/event:

`ChestPurchaseSliderHooks` installs one hook; `ChestPurchaseSlider` logs the native and effective ceilings when
it changes a tagged context.

Stop immediately if:

The client crashes or hangs, an untagged slider changes, the chest row becomes unusable, or disabling the flag does
not restore native behavior.

Report back:

Observed native maximum, configured maximum, displayed maximum, whether any purchase was attempted, and whether the
disable path restored the native ceiling.

## Result

- Build/deploy command: AXF `stfc-mod-private.cycle`, Windows release mode.
- Runtime command: the cycle stopped the existing client, deployed the hash-matched release DLL, launched
  `prime.exe`, and verified fresh boot activity.
- Human action performed: opened an eligible custom-quantity chest popup and moved its slider to the maximum without
  confirming a purchase.
- Observed log/event evidence: the pre-promotion `ChestPurchaseSliderExperiment` resolved
  `InventoryUseRowWidget.SetWidgetData` and reported `installed=1 failed=0 skipped=0 total=1`. When the tagged row
  rendered, the runtime logged `[ChestPurchaseSlider] extended quantity ceiling from 50 to 123`; the human confirmed
  the displayed slider reached 123.
- Crash/hang/recovery notes: no crash or hang during install/boot. The cycle reported the pre-existing
  `MissionHudTweaks.UpdateButtons` missing-method audit failure, unrelated to this experiment.
- Disable-path evidence: a release cycle with `extend_chest_purchase_max = 0` reported
  `ChestPurchaseSliderHooks` as `installed=0 failed=0 skipped=1 total=1` with detail
  `configured maximum is non-positive`. The setting was then raised to 999 for boundary discovery and a fresh cycle
  installed the hook.
- Exploratory high-config evidence: rendering the same tagged chest row during discovery logged
  `[ChestPurchaseSlider] extended quantity ceiling from 50 to 999`.
- End-to-end boundary: confirming a Standard Recruit Chest purchase at the extended 999 quantity produced the
  game's generic `The purchase failed to go through` error. The client offered its normal latinum top-up path for
  missing claim tokens, but available claim resources were not the limiting factor. Follow-up boundary tests
  established that 159 and 160 completed successfully while 161 failed, making 160 the observed inclusive
  transaction ceiling for Standard Recruit Chests. Current-client disassembly of
  `ShopShowcaseViewController.TryPurchaseMultipleChests(ResourceCostData, uint)` shows the selected quantity being
  forwarded unchanged into the purchase service, with no second client-side 50-item clamp.
- Discovery payload: temporary log-only hooks later captured quantities 161 and 520 entering the Standard Recruit
  loot-chest path and failing with `GSError` platform code 24, `InvalidBundleQuantity`, and message
  `Can not purchase chosen quantity`. The payload contains no accepted maximum, so 160 remains empirical rather than
  dynamically discoverable.
- Independent boundary confirmation: a different recruit chest also accepted 160 and rejected 161, reproducing the
  same inclusive transaction boundary.
- Release-config evidence: the repo default and live config were finalized at 160; a clean Windows release cycle
  installed `ChestPurchaseSliderHooks` with `installed=1 failed=0 skipped=0 total=1`.
- Answer to the question: yes. The game's chest-purchase discriminator isolated the row, the configured ceiling
  replaced the native 50, the original renderer returned successfully, and the slider displayed 123 in the initial
  smoke. Two recruit chests then succeeded at the bounded ceiling of 160 and rejected 161.

## Exit Decision

Promote with a configurable and enforced UI ceiling of 160. Larger configured values clamp to 160, 0 disables the
hook, and a future native game ceiling above the configured value remains authoritative rather than being lowered.

Next action: revalidate the empirical transaction boundary after relevant game updates or when testing a materially
different non-recruit chest category.
