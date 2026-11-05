# PLAY-BUFF-GUARD — qualified null-list crash protection

Frozen1baa55fe source: misc.cc ExtractBuffsOfType_Hook and
InstallTempCrashFixes; supporting prime/IList.h, IBuffData.h, ClientModifierType,
config and patches.cc registration. Target: features/buffs.cc and game/server.h.
Exact refs: [cohort03](../FEATURE-COHORT-20261029-03.md).

| Obligation | Outcome | Evidence |
| --- | --- | --- |
| B1 | Null list delegates exactly once with original modifier and pointer | Actual production hook under controlled host |
| B2 | Observed empty/all-nonnull list delegates unchanged; no list copies or mutation | Empty/full fixtures and modifier identity checks |
| B3 | A null element at any inspected position returns nullptr without native extraction | Each position in representative list |
| B4 | Count/item access failure remains unavailable instead of becoming zero/null observation | Count-failure baseline reproduces migration's unsafe delegation; checked result correction; item-failure case |
| B5 | Retain collection through inspection and original consumption; release on early returns | Native substitute asserts active list root and final root cleanup |

B4 intentionally strengthens the defensive boundary. Frozen IList reads _size
and caches a runtime item accessor; it does not establish graceful missing-
metadata behavior. Target uses shared checked calls against the runtime
collection class, untyped managed references and no IBuffData interpretation,
so interface drift cannot turn every valid element into an apparent null.
Known null input still delegates; an unknown operation returns no extracted
buffs. This rare failure decision must stay visible, not be called equivalence.

[patches].tempcrashfixes defaults true and remains startup-only. No new feature
setting/UI, filtering, classification, list cap or recovery poll is added.
Other old TempCrashFixes functions retain separate feature ownership.
No crash was deliberately triggered in the real game. Native installation,
real GC behavior and both macOS runtimes require their own qualification.
