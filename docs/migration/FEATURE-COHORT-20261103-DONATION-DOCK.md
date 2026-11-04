# Donation and dock-assignment continuation

Existing `play-dev-ripper`, starting head
`5d7613d0383c91b6c051fecdc6605f707224ae0e`; frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`. Contracts:
[donation](features/PLAY-DONATION.md) and
[dock assignment](features/PLAY-DOCK-ASSIGN.md). This slice reconciles those two
existing core features. AAI, new upstream features and parked diagnostics remain
deferred. This record qualifies bounded local fixtures and the reviewed signed
source checkpoint below; current-client runtime remains separate. Both inventory
classifications remain Partial.

## Corrections and retained behavior

The Windows donation setter observes the actual donation bool, then decides from
current live policy after that managed observation. An enabled, tagged donation
with native cap 50 retains the existing positive configured-cap override;
nonpositive configuration retains the exact initial native cap. Missing, failed
or false donation tags and disabled policy keep the original call. Keep the typed
bool-value correction rather than restoring the frozen wrapper's boxed-pointer
truthiness defect. No donation or resource operation is performed by the fixture.

The existing non-Windows donation row remains scoped to known donation context
and bounded by known owned amount. Its final managed Context observation can
change or replace Config. The correction decides from policy current after that
observation, including a changed positive or nonpositive cap. This does not
restore the frozen global slider detour. Linux exercises the common non-Windows
branch with controlled bodies; native Mac hooking and resource behavior remain
separate qualification.

Dock assignment distinguishes a known hidden/dead candidate from unavailable
liveness, active-state, parent-canvas or visibility observations. Unknown state
must abort the pending action rather than selecting another dock. Selection stops
at the first known shown dock even if its button is absent or later invalidated.
The correction retains custody of the selected owner, canvas and acquired raw
wrapper/widget/listener/button chain, validates their identities after managed
observations, and rechecks current policy before Press. A field-chain replacement
made before acquisition is the current chain, not evidence of a stale read. Local
roots and same-parent/field-chain rechecks do not lock the hierarchy or make the
managed observations atomic.

Shortcut capture suppresses Enter assignment only. Physical double-click keeps
the frozen mouse policy, shared pin-input precedence and original-first callback.
The same-ship 400 ms pairing and consumed-pair behavior remain. Enter retains the
intentional single-edge behavior and independent hotkey-patch gates. Ordinary text
focus is frame-cached; this slice does not establish a fresh focus observation
after managed reentry. No global tracker redesign, persistent dock cache or
episode-reset policy is introduced.

## Preserved before-repair evidence

The Windows `build-tests` baseline succeeds in 191/191 build steps. The preserved
executable runs 32 explicit modes: 22 controls pass and 10 intended cases fail.
Eleven donation modes produce nine passes and two late-policy failures;
21 dock modes produce 13 passes and eight failures.

The two donation failures are `donation-policy-off` and
`donation-policy-replace-off`. The eight dock failures are
`dock-assign-disable-reentry`, `dock-assign-capture-reentry`,
`dock-assign-hide-reentry`, `dock-assign-destroy-reentry`,
`dock-assign-unknown-active`, `dock-assign-unknown-parent`,
`dock-assign-unknown-visible` and `dock-assign-unknown-alive`.
The capture-reentry case invokes the real Enter frame consumer, not the physical
double-click route. Controls include first-shown stopping, known false/dead
skipping, missing-button stopping, acquisition after a rebind, pairing, Enter,
disabled policy, pin consumption and original native click ordering.

The Linux baseline succeeds in 184/184 build steps. Its 29 explicit modes produce
17 passing controls and 12 intended failures: four late-policy donation-row
failures and the same eight dock failures. Four donation-row controls and the
same 13 dock controls pass. The donation failures are `donation-row-off`,
`donation-row-replace-off`, `donation-row-cap-change` and
`donation-row-cap-zero`. This is a distinct non-Windows branch baseline, not a
native Mac result or the Windows setter repeated under another host.

Evidence is under
`D:/dev/stfc-workspace/artifacts/donation-dock-20261103/`.
`before-inputs.json`, `before-windows-build.log`, `before-windows-cases.json`
and `before-windows-binary.json` bind the Windows baseline. The retained Windows
`before-il2cpp-bindings-tests.exe` SHA-256 is
`e46e458b30249ff294c777941cd075d505265056789ea261f9e19058679a009a`.
The Linux environment, source hashes, build log, case results and binary hash
are preserved separately in the `before-linux-*` records. The retained Linux
ELF SHA-256 is
`9d14f055b8b1aa6572e95f8d0795e1a75f86181812039f9f493bf8ba5c856fa9`.
The preserved source-input hashes distinguish these baseline bytes from later
working-tree corrections.

These fixtures call production callbacks and typed bindings using controlled
metadata, managed bodies, GC/lifetime observations, input and counted native
button bodies. They perform no game input, donation, purchase, assignment or
resource operation. They demonstrate bounded consumer behavior; they do not
establish current-client ABI fit, native Unity collection, visible dialogs,
account persistence or platform gameplay acceptance.

## Guard-order self-review delta

Six additional dock modes extend the original 21-mode baseline: physical mouse
input while shortcut capture is active, an acquired field-chain replacement,
dead/unknown button liveness, an early parent rebind and a late parent rebind.
They are not part of the before-repair counts above.

The late-parent mode was added during self-review of the initial correction.
Its second controlled Canvas.Visible callback changes the selected widget's
parent after the first correction had already checked it. The preserved
`late-parent-before-build.log` records 191/191 build steps, and
`late-parent-before.json` records the intended failing exit code 1 for
`dock-assign-late-parent-rebind`. Its retained executable SHA-256 is
`f7674c4f0e7aa3a090a4dcccdbb36e95fb1f42eee9a03c0a6cd11e137e4e0a37`;
the record also binds the production dock and fixture source hashes.
The correction places the same-parent observation after the final Visible and
active-state queries. This is a failing-before/passing-after check of the new
guard order, not an additional missing frozen-source feature or a whole-hierarchy
synchronization guarantee.

## Controlled build and fixture qualification

Corrected Windows `mods build-tests test-il2cpp test-mod` passes 423/423
build/run/install steps with Zig 0.16, ReleaseFast core/dependencies and Debug
fixtures. A step count is not a test count. All 38 explicit modes pass: 11
Windows donation setters and 27 dock consumers. All ten retained modes also pass:
shop limits, native/filter claims, confirmation targeting, pin presentation,
Settings choice/slider consumers, chat pointer input and camera pan/backdrop.
The existing targets are retained; no new test target or CI registration is added.

`local-verification.json`, `after-windows-build.log`, `after-windows-cases.json`
and `retained-windows-cases.json` bind those results. The retained
`after-il2cpp-bindings-tests.exe` SHA-256 is
`20579ad04cc6c6401e41b997e7f85d6fa8181ed01874f80722ebc0ebac7c08ef`.
The Windows core `mods.lib` SHA-256 is
`d42028ed91125f69b4175821f4ed51ef6e081013c161e20b4da09fe57577a788`.
`final-source-inputs.json` records 479 source/build/test input hashes; the receipt
records the same manifest count and confirms the retained fixture matches the
built executable. This is a local core library and fixture qualification, not a
deployed DLL or current-client runtime receipt.

The final Linux/WSL x86_64 rerun with Zig 0.16 passes the existing `test-il2cpp`
target in 157/157 build/run steps. All 36 explicit modes pass: eight scoped
donation-row cases, the same 27 dock cases and retained shop limits.
`after-linux-test-il2cpp.log`, `after-linux-results.json`,
`after-linux-summary.txt` and `after-linux-binary.sha256` bind the final result.
The retained `after-linux-il2cpp-bindings-tests` ELF SHA-256 is
`658f0f6674e089b62103d406e2c2b8dd8179ebedb151683afbac060c2cf85bf8`.
`after-linux-input-check.log` confirms all six changed production/fixture input
hashes match: shop, dock, UI bindings, fixture registration and the two new case
headers. Environment and source-hash records remain separate from Windows.
This covers the non-Windows common branch; it is not a native Mac build or trial.

Both native Mac core requests (`aarch64-macos.14.6` and
`x86_64-macos.14.6`) were rechecked after the final guard-order change. Both stop
during configuration because the macOS SDK is missing; no Mac source compile is
established. `mac-builds.json` and the two `after-macos-*` logs preserve this
limitation. Linux common-branch fixtures cannot qualify native Mac hooking,
rendering or resource behavior.

## Signed source and review

Signed source checkpoint `5a1ae2aa5e578bede65a4532c56749747a32bef3` has a good
signature; local signer trust remains unknown. `source-signature.log` and
`source-checkpoint.json` preserve that result. This is a local source checkpoint,
not a signed release or deployment.

General, hostile and independent read-only reviews found no confirmed production
defects or blocking findings against base `5d7613d0`, diff hash
`1bddcbca5bff0f82e8c58ce1bd1e0f64ca335917`. `review-receipt.json` binds the exact
head, base, merge-base, ten changed files and constraint input. All 479 source
hashes, the retained baseline/final fixture binaries, late-parent baseline and
Windows core library matched their records. Reviewers did not execute fixtures
or native game actions; the earlier controlled runs supply that evidence.

General and independent review identified one P3 maintenance suggestion: the new
argument-only modes are absent from the ordinary `test-il2cpp` run lists. Their
explicit 38 Windows and 36 Linux invocations qualify this checkpoint, while
automatic registration with the existing target remains deferred coverage work.
An ordinary target run alone does not exercise these new cases.

The receipt uses static-persona constraints because the LexSona memory lookup
was unavailable. Exact Git and input hashes still bind the review. The documented
borrowed-snapshot, frame-cached focus, non-atomic hierarchy and native platform
limits remain open; review does not turn them into qualified runtime behavior.

Guff's earlier basic Windows donation/dock acceptance remains attached to its
tested pilot artifact. It does not qualify this continuation's changed bytes,
all dock lifetime/order edges or the intentional Mac donation scope difference.
The unresolved borrowed-snapshot/whole-registry custody boundary remains recorded
in the dock contract; these bounded fixes do not claim full source equivalence.

No runtime session, game/sidecar cycle, live TOML/config change, deployment,
pre-release replacement or remote write belongs to this controlled evidence
record. The two scoped inventory assessments retain Partial; current-artifact
native/gameplay qualification and promotion remain open.
