# PLAY-CHEST-LIMIT — bounded chest quantities

Contract v1; exact refs: [cohort](../FEATURE-COHORT-20261029-01.md).
Complete source `parts/misc.cc`, InventoryForPopup/InventoryUseRowWidget/Widget,
configuration, Settings and native registration. Present in inspected upstream,
PLAY and frozen play-dev; target `features/shop.cc` reuses the native row seam.

| Obligation | Preserved behavior | Qualification |
| --- | --- | --- |
| C1 | IsChestPurchase is populated too late for the setter; use row SetWidgetData | Complete source/native signature comparison |
| C2 | Extend only a positive native maximum upward; configured extension bounded160 | Actual consumer table of zero/negative/small/large requests and native caps |
| C3 | Unknown tag, missing context and unrelated inventory remain native | Actual checked/empty-context fixtures |
| C4 | Original receives the same row exactly once, after qualified adjustment | Native substitute counts/receiver assertions |
| C5 | Rebinding during a managed read cannot mutate an obsolete context | Reentrant replacement fixture; source defect distinguished from migration parity |
| C6 | Changes apply when the dialog binds again; existing hook installed even when request initially0 | Live setting/next-bind fixture and Settings metadata |

Default maximum160, key `ui.extend_chest_purchase_max` and persistence are
unchanged. A native limit above160 is never reduced.0 disables extension; no
unknown becomes a positive cap. Borrowed managed objects and references into
replaceable Config must not survive intervening managed callbacks without
appropriate custody/value copying. No purchase is performed by the fixture;
native slider rendering and server quantity acceptance remain separate.
