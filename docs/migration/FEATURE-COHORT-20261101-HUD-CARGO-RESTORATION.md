# HUD restoration and cargo counter qualification

Source: existing Ripper Downstream `play-dev-ripper`, starting clean at signed
`272410c86873bf436aa6d4af1975e25f6c5f9045`. Implementation: signed
`ac2314ba965d03ef55bcfa219add530b8e5063e6`. Pinned PLAY
`cad98ef7990dd66167eb64ea5dda525c1fe2c296` and selected frozen play-dev
`1baa55fedf1758b19f843cc59707b8901a0b4998` remain the behavior references.
No branch/worktree change. Settings passes remain stopped.

## Bounded corrections

HUD and cargo consumers were already migrated. This pass corrects deferred
restoration and strengthens operation-local cargo ownership; it adds no feature
or alternate counter calculation.

Missions retains a measured baseline for its exact button because it has no
corresponding native setup refresh. A failed visibility read/write when returning
to Auto retained that baseline but did not enqueue work. Once the failure cleared,
idle frames therefore never restored it. A scope guard now queues known pending
Auto restoration through the existing weak-owner queue, including early returns
while the button is temporarily missing. One owned deferred batch drains at a
time. Confirmed readback retires the debt; forced Always/Never does not schedule
perpetual restoration. Button replacement still resets the detached baseline.
This adds no idle registry scan, worker or timer. It does not guarantee recovery
from every possible allocation/liveness failure.

Windows instant cargo previously held only a raw widget pointer while native
rendering ran and the temporary snap flag was restored. The scope now owns a
temporary strong GC handle until restoration completes, including C++ unwind.
If acquiring that root fails, the hook calls native rendering exactly once
without changing the flag. Existing exact label/runtime-class/field admission,
own-method hook qualification, both prior flag values and native formatting/
interpolation remain intact. This is controlled lifetime hardening, not a
reproduced live GC crash or a changed cargo amount.

Cargo formatting is unchanged: its actual-consumer cases still cover native
formatting order, decimals clamped to 0..6, live/disabled preferences and unrelated,
missing or Unicode-lookalike labels. No hook/default/shortcut/Settings writer,
persistent managed root or diagnostic probe is introduced. Instant cargo remains
Windows-only, as in frozen source; enabling it on Mac still requires hook-fit
qualification.

## Verification

New explicit modes execute the actual production hooks in the existing typed
managed host. Unchanged production independently fails `hud-restoration-retry`
at the missing next-frame retry and `cargo-custody` at absent managed custody
during native render (both exit1).

Corrected HUD cases cover failed observation, failed setter, mutation before
setter failure with successful readback, temporary missing component, replacement
button, completed idle frames and intentional forced-mode idle behavior. Cargo
cases cover both prior snap values, one original call, normal/unwinding cleanup,
root release and unavailable-root native fallback. These substitutes do not
prove real garbage collection, hierarchy, visuals or live frame cost.

Ten explicit modes pass: hud-restoration-retry, cargo-custody, shop-claims-native,
shop-claims-filter, confirmation-targeting, cargo-native, cargo-context-pointer,
cargo-snap-invalid, cargo-inherited-hook and cargo-format-signature. Existing
`test-il2cpp test-mod` passes 312/312 build/run steps. Clean/stable Windows
releasedbg passes 41/41 steps and proxy verification at the implementation
commit. Candidate DLL SHA256: `4E54052323F2A400BCAE74428662C7FA7250C5D52F927504A302DFB943E74A3B`.
Step counts are not test counts; no CI/build-target registration changed.

Evidence: `D:/dev/stfc-workspace/artifacts/hud-cargo-restoration-20261101/`,
including baseline fixtures, before/final command/cwd/base/duration/exit and
executable hashes, exact final inputs, frozen source manifest, signed checkpoints,
native receipt and preserved DLL/PDB. Config defaults, shortcut definitions,
English example, binding/policy owners and build registration are unchanged.

## Qualification and continuation

Source checkpoints and candidate remain local/unpushed and undeployed. No game
input, cycle, TOML edit, reward/resource action or native UI check occurred.
Installed artifact and published prerelease were not changed by this work.
Exact-candidate live HUD/cargo and Mac build/runtime qualification remain open.
Settings passes and parked work remain stopped.

PLAY-HUD and PLAY-CARGO-INSTANT retain their overall classifications. Cargo
formatting has only been rechecked, not reimplemented. P19 remains in progress
until broader native workflow acceptance is satisfied.
