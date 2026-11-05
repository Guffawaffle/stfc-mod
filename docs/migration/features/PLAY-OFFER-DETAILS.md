# PLAY-OFFER-DETAILS — cooldown action opens details

Contract v1; frozen `1baa55fe`, target base `266a688c`. Source:
`parts/misc.cc::BundleDataWidget_OnActionButtonPressedCallback`,
`prime/BundleDataWidget.h` and miscpatches registration. Target:
`features/shop.cc`, `game/inventory.h`, existing ShopLimits transaction.

The native cooldown flag16384 is a bit, not a whole-state equality test. A widget
with that bit routes to its native auxiliary-view handler; otherwise original
purchase/action delivery runs once. The mod neither purchases the offer nor
changes cooldown values. Unknown field access retains the binding's native
fallback behavior; no action is inferred from missing metadata.

Actual production fixture covers ordinary flags0/1 and cooldown with/without
other bits, counting native/original calls. Source and existing implementation
agree; no rewrite is introduced. Native timer expiry, pooled rows and actual
details presentation remain player qualification. Shared desktop source and
exact checkpoints: [cohort03](../FEATURE-COHORT-20261028-03.md).
