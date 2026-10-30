# PLAY-BASE-CARD — retain a matching enemy station card

Contract v1; corrects the inventory's old name/level description to match its
actual source. Full frozen base_card.cc, headers, recall consumer, config,
Settings and native ABI/failure guards inspected. Exact refs:
[cohort](../FEATURE-COHORT-20261029-02.md). Target `features/base_card.cc` with
the existing `game/base_card.h` qualified observations; no UI redesign.

Retention requires enabled installed behavior, successful course response,
matching exact player fleet ID, a valid foreign station, live matching preview
context/POI/user, and matching target ID or user. Recall may have no target
payload, but still requires the matching current foreign-station context.
Own stations, different/unknown identities, failed responses and unavailable
bindings retain native dismissal. Preview and navigation sources remain distinct.

The24-byte SetCourseResponse value layout is independently checked before
installation. Commands, course submission, semaphores and resource actions remain
native. Existing recall integration must not be counted as a complete live
retention trial merely because its caller was exercised with a fixture sink.

Actual production binding fixtures and source comparison are recorded separately
from hook installation, native fallback delivery, real card behavior and Mac
qualification. No station-label feature is invented from the old inventory title.

Keeping the card open on recall is a hard product requirement. Guff confirmed
that failure of repeated `primary_action` after recall is longstanding. Native
button color while docked does not represent the mod shortcut's availability.
The current bounded fresh-native-scan experiment and its exact-target guards
are documented in the cohort. Runtime acceptance remains pending; neither the
unsuccessful button refresh nor the unsuccessful same-ship rebind is a fix.
