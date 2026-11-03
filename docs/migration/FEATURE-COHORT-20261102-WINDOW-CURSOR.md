# Window sizing, profile titles and cursor behavior

Existing `play-dev-ripper`, clean starting head
`5c3637af61a3fb270db28f2610e95fe62666d5c6`; frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Contracts:
[PLAY-WINDOW](features/PLAY-WINDOW.md),
[PLAY-WINDOW-PROFILE-TITLE](features/PLAY-WINDOW-PROFILE-TITLE.md), and
[PLAY-CURSOR](features/PLAY-CURSOR.md). AAI and new upstream features remain deferred.

## Source reconciliation

The typed title port lost the frozen one-second retry cadence and complete desired
title cache. It also gated on a nonempty human label rather than actual custom
routing, and could write a prefix alone after an actual title read returned zero.
Restore bounded retries, actual-read validation, cached composition and custom
activation. An empty custom label uses the selected config filename. Keep the
catalog's deliberate admitted-ID presentation; frozen's generic config prefix is
not the intended named-profile label.

Window forwarding retains the Unicode A/W selection and disabled-Update original
call improvements. An unavailable/null saved Unity procedure must use original
WndProc, rather than treat a default zero read as a callable address. Preserve
existing message values and game-window/DPI ownership.

Frozen contains no installation of its SetWindowLongW detour. F11 borderless
switching and the captured-window fullscreen fallback therefore remain dormant;
this work does not activate them. Cursor dispatch already matches the source's
Windows plain-arrow/clipping behavior and canvas rescale companion. Preserve its
existing signature, registration gates and defaults, and cover the unchanged
dispatch directly.

## Verification and limits

The existing title behavior was extracted unchanged into the private title-state
owner before corrections, so it could be driven directly. The baseline Windows
test build passes 191/191 steps. Six independent cases then fail as intended:
actual title-read failure, retry cadence, cached composition, empty display label,
and null/missing Unity procedure. Seven ordinary/Unicode/named/delayed/one-shot/
cursor/resize controls pass. This is an equivalent extraction, not a claim that
the tested tree was the untouched starting commit.

Corrected Windows core/binding/mod checks pass 423/423 build/run/install steps,
using Zig0.16, ReleaseFast dependencies/core and Debug fixtures. All thirteen
explicit window/title/cursor modes pass, plus retained scale-native and ordinary,
directory-like custom and named File modes. The fixture includes production
window.cc and uses actual typed static-field resolution; only window/cursor
effects and supplied File context are private sinks. The window fixture reuses
the equivalent UTF-8 conversion boundary in tests/loading_artwork_cases.h;
actual std::filesystem operations form the selected dotfile label. Separate
profile-paths modes link production File code and verify selected namespace names.
No new test target or CI registration is added. Formatting and whitespace checks pass.

Evidence is under `D:/dev/stfc-workspace/artifacts/window-cursor-20261102/`:
before-verified-build.log, before-cases.json, final-tests.log and
local-verification.json bind the baseline failures, final source inputs and both
test executables. The first baseline link attempt exposed three unused DPI API
imports in the fixture; private no-observation sinks fixed that test boundary,
without changing build wiring or production DPI behavior.

Both Mac core build requests stop before compilation because the configured
macOS SDK is absent. mac-builds.json and per-architecture logs retain the failure.
This slice changes only the existing Windows feature body and Windows-guarded
test additions; neither Mac architecture is claimed built or runtime-qualified.

Signed source checkpoint `74b4ab9c59e35399e32c454667e7d7fc0669b68b` has a good
signature (local signer trust unknown). General, hostile and independent reviews
of base `5c3637af` and diff `1e29bbac7ea4b1dd695afbb2a238b4bf4befd436` found no
production defect. General/hostile identified the conversion-boundary wording
above; that documentation-only correction and the additional dependency hash do
not change production code, tests or executables. The manifest now records fifteen
source/test inputs and two executables. Review receipts retain static-persona
fallback because their CLI memory lookup was unavailable; scoped AXF continuity
works separately. Documentation correction review is recorded in the evidence
directory alongside the original three-lane receipt.

The three catalog rows remain Implemented, unverified. Controlled fixtures and
production compilation do not qualify real HWND selection, Win32 title operations,
resize/maximize, display changes, visible cursor/clipping behavior, exact-client
ABI/hook fit or Mac runtime behavior. Mcohall is testing the existing Mac
pre-release; that result remains attached to the artifact tested.

No game cycle/input, live configuration edit, deployment, push or pre-release
replacement occurred. Native trials remain separate from local reconciliation.
Continue existing camera pan/backdrop source reconciliation; keep parked orbit
diagnostics, AAI and new upstream functionality deferred.
