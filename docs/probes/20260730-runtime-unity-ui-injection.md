# Probe: Runtime Unity UI injection

- Status: observed; prototype removed after proof
- Owner: Guffawaffle / stfc-mod
- Date: 2026-07-30
- Related patch label: chest-purchase-slider UI proof
- Related timeline refresh ID: 2026-07-30 current-client corpus
- Related diff report: N/A
- Native seam ledger entry: runtime Unity UI composition from
  `Digit.Prime.Inventories.InventoryUseRowWidget.SetWidgetData()`

## Question

Can the injected mod create visible, arbitrarily positioned text on an active game canvas by composing the game's
existing Unity UI objects at runtime?

## Static Evidence

- Symbols:
  - `UnityEngine.Component.get_transform()` and `get_gameObject()`
  - `UnityEngine.Transform.get_parent()`, `SetParent()`, and `SetAsLastSibling()`
  - `UnityEngine.Object.Instantiate(Object, Transform, bool)`
  - `UnityEngine.GameObject.GetComponent(Type)`
  - `UnityEngine.RectTransform` layout setters
  - `Digit.Client.UI.TextLocalizer.OverrideLocalizedText(string)`
  - `TMPro.TMP_Text.set_fontSize(float)`
- Composition strategy: clone a text object already owned by the chest popup, parent it to the popup's root canvas,
  reposition its `RectTransform`, and replace its localized text. Reusing an existing object carries forward a
  compatible font, material, and renderer without loading a new UI asset.
- Why this is a capability proof rather than a feature: it establishes that the mod can add a visible object to an
  active game canvas. It does not establish safe lifecycle management, reusable layout behavior, or compatibility
  across game screens and client updates.

## Risk

- Risk class: R5
- Confidence rung: runtime observed
- Payload confidence: the source `TextLocalizer`, popup widget, and root canvas were identified successfully for the
  tested Standard Recruit popup only.
- Original/trampoline confidence: the existing chest-row hook returned successfully before the UI object was
  composed; no additional native detour was introduced for this proof.
- Behavior change expected: yes

Open safety questions:

- Whether the clone is destroyed reliably with every owning canvas and scene transition.
- Whether cached managed pointers or handles remain safe after Unity destroys the cloned object.
- Whether repeated popup opens, resolution changes, canvas scaling, and unusual aspect ratios produce duplicates or
  off-screen/clipped content.
- Whether cloned localization, sibling, animation, accessibility, raycast, or reactive components retain unwanted
  behavior from the source object.
- Whether layering remains correct across modal stacks and whether a notice could obscure or intercept interaction.
- Whether the Unity calls are always reached on the game UI thread.
- Whether this composition path survives class, method, field, font, and prefab changes in future clients.

## Implementation Plan

- Module/file: temporary proof formerly in `mods/src/patches/parts/misc.cc`; removed after capture
- Historical prototype guard: reached only when `[ui].extend_chest_purchase_max` was above the verified 160 boundary
  during discovery. The prototype and that above-boundary release behavior were removed.
- Existing hook descriptor: `InventoryUseRowWidget.SetWidgetData`
- Install path: `InstallMiscPatches`
- Log tag: `ChestPurchaseSlider`

This proof deliberately reuses an already registered hook. It does not justify adding an unregistered UI lifecycle
hook or promoting a general overlay service.

## Disable Path

- The prototype composition call and helpers were removed after the visual proof.
- The proof creates no persistent files or game assets.

## Human Smoke Test

Goal:

Demonstrate visible mod-authored text on the Standard Recruit chest popup.

Steps:

1. Set `extend_chest_purchase_max = 999`.
2. Cycle the Windows release DLL and open the Standard Recruit custom-quantity popup.
3. Confirm that the centered purple-backed message is visible.

Expected log marker:

`[ChestPurchaseSlider] displayed experimental-limit in-game notice`

Stop immediately if:

The client crashes or hangs, the popup becomes non-interactive, the cloned object obscures purchase controls, or the
notice persists incorrectly after leaving the owning UI.

## Result

- Build/deploy command: Windows release build followed by AXF `stfc-mod-private.cycle`.
- Human action performed: opened the Standard Recruit custom-quantity popup at a configured maximum of 999.
- Observed result: a cloned game-native text object appeared in the center of the active popup canvas with the
  mod-authored text `STFC MOD UI TEST` and `Chest purchases above 160 are experimental.` The human supplied a
  screenshot confirming the text, purple mark background, inherited game font, and placement within the modal.
- Log evidence: `[ChestPurchaseSlider] displayed experimental-limit in-game notice`.
- Crash/hang/recovery notes: no seam-related crash or hang was observed during this single proof.
- Answer to the question: yes. Runtime Unity UI composition from the injected mod is possible in the current client.

## Exit Decision

Retain the documentation as an observed capability proof, not as evidence of a production-safe overlay system. The
prototype runtime code was removed so the release-supported chest slider retains no experimental UI lifecycle.

Next action: if a product UI surface is desired, build and test a bounded lifecycle contract covering creation,
deduplication, destruction, canvas changes, scaling, input/raycast behavior, and rollback before promotion.
