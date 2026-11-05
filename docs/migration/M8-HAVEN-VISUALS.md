# Haven visual ownership and restoration

This continues the isolated [presentation checkpoint](M8-PRESENTATION-PRESERVATION.md)
from documentation revision `aa30ce6b`. The preservation input remains downstream
`45d1deaffddc3706e5c8232cf746ca46ef67be91`; SPUD remains the same included source
snapshot. No active checkout, settings location or state schema is migrated.

## Restored behavior

`graphics.hide_haven_water` returns with its original default, false. The normal
reader, English example and preserving writer share that exact identity. It is
the twentieth registered gameplay key. A saved preference confirms a durable
configuration choice, not that a native renderer changed. Settings UI remains
deferred; the setting can be read through ordinary configuration reload.

The existing [Haven feature](../../src/mod/features/haven.cc) now shares discovery
between water and road depth. Water is classified by the original material and
shader identities. Road material/shader filtering and existing camera expansion
geometry/history are preserved. Qualified visual choices run before zoom-curve
calculations, so disabling expansion or observing a flat curve does not prevent
water handling or release of an old intervention.

Before hiding water, retain its observed enabled state on that exact weak object.
Release uses current-or-original enabled state: another owner's later enable
must not be disabled. Road offsets retain their original values and only restore
while the current value still matches our intervention. A known competing edit
is preserved. Each offset can finish restoration independently after a partial
failure; unknown measurements do not become zero or a successful restoration.

Only the existing HavenZoom hook transaction and descriptor are used. Its frame
callback drains pending restoration after scene qualification is lost; it does
not search globally while idle. Both-disabled/no-owned-work callbacks do not
construct a backend or search the scene. No second camera/update hook is added.

## Ownership and checked observations

[Shared Haven policy](../../src/mod/haven_visuals_policy.h) owns discovery,
membership and restoration decisions. Persistent records are weak; native owner,
arrays, candidates, materials, shader, text and property arguments are strongly
held locally across runtime calls. Unity's signed instance ID and actual weak
object identity are checked together. Zero and negative IDs are observations,
not missing ship identities.

Missing metadata, failed calls, confirmed destruction and observed false/zero
remain distinct. The new Unity accessors reuse existing checked IL2CPP calls;
they add no imports or alternate resolver. `Object::Is<T>` reuses the existing
`ClassOf<T>` lookup so a rooted SDK String can be checked before UTF-8 conversion,
while preserving ordinary bound-class behavior.

Array acquisition declares `System.Array` before invocation, then checks actual
reference-vector shape before traversal. Unity's non-generic component lookup
declares Component[] even when filtering for Renderer; each element is checked
as a Renderer separately. Material arrays keep their own element qualification.

The whole operation, including cadence and qualification flags, is protected
against synchronous reentry. A nested callback cannot restart discovery,
invalidate an in-flight membership span or consume the outer operation's cadence.
Owned failure logging cannot throw back into the game; the native camera
original remains outside that containment and runs once first.

## Bounds and failure behavior

Application cadence remains one second; scene lookup/classification refresh is
five seconds, including unsuccessful discovery backoff. Rechecking the path
detects a replacement owner while an older object remains alive. Any discovery
exception clears cached membership and preserves backoff. Restoration ownership
is separate and survives an unavailable replacement scan.

The retained intervention bounds are 256 road materials and eight water
renderers, inherited from the old feature. Native discovery also has explicit
admission bounds of 4096 component observations and 16 materials per renderer.
These are work/ownership bounds, not proof of complete coverage of every Haven
scene. Oversized or failed discovery is rejected as a whole; a collected prefix
is never published as complete. Previously owned changes can attempt release,
with unconfirmed obligations retained within their existing bounds.

## Verification boundary

[The shared fixture](../../tests/haven_visuals_test.cc) drives the actual cache,
ownership, reentry and restoration algorithms with synthetic native observations.
It covers missing owners/backoff, replacement and streaming, weak expiry, signed
IDs, unavailable reads, failed/throwing mutations, external edits, independent
offset restoration, admission refusal and delayed nested operations.

[Binding fixtures](../../tests/il2cpp_bindings.cc) exercise the actual checked
Unity helpers and String qualification against a controlled runtime. Missing
Array and String classes have separate fresh-process cases. The existing
[gameplay writer fixture](../../tests/basic_gameplay_settings_test.cc) exercises
real isolated file writes, both Boolean values, preserving comments/unknown
data, external deletion conflicts and explicit live reload intents.

One initial new parser assertion incorrectly assumed integer-to-Boolean
conversion was forbidden. The existing TOML reader permits it. The reader was
left unchanged; the fixture now distinguishes accepted Boolean values from
unconvertible array/string input. Superseded development logs remain retained.

These fixtures do not execute actual scene discovery, GPU rendering or the live
camera callback. Native qualification must check water on/off at native and
extended zoom, idle Haven without flicker, scene/streaming replacement, disable
and reload, leaving/reentering Haven, pending release and observable callback
cost on the exact tested client/artifact. macOS and signing remain separate.

### Qualified source checkpoint

Final source: **`c2fb9b5a81244ca9388c737d8f6b7edd7eda32a0`**.

| Exact clean candidate lane | Result |
| --- | --- |
| Windows x64 Zig 0.16, Profiles host | 211/211 steps; 8/8 reported Zig tests; 91 artifacts |
| Windows x64 Zig 0.17, default off | 189/189 steps; 77 artifacts |
| WSL/Linux x64 Zig 0.16 | 172/172 steps; 8/8 reported Zig tests; 37 artifacts |

Both Windows gates ran the actual production uploader against owned temporary
loopback listeners with synthetic data. The host lane also ran three inert
Profiles controls; no account/preferences/catalog lifecycle cases were run in
the developer execution environment. Negative build-option controls were not
repeated. Linux qualifies portable fixtures, not the Windows HTTP or mod runtime.

Independent readback verified **498 unique file facts with zero mismatches**,
including exact clean source/tool/archive/log/artifact identities, the new Haven
and missing-String markers, executed uploader bytes, static CRT, 18 host versus
16 default-off PE exports, one compiled TOML provider and the expected Profiles
host dependency closure. Default off contains no Profiles libraries. Static
inspection did not load either product DLL.

| Product DLL | SHA-256 |
| --- | --- |
| Host 0.16 | `0d2f6d8db1855691ecae7ba53b7da1943e47d0f93618bbe585f33d1faddd2372` |
| Default off 0.17 | `ca62b16db7fb92b42110babbb5d6109c7a9ee1b240cf1cca9933e9d9a17a91a0` |

Evidence root:
`D:/dev/stfc-workspace/.ax/ripper-haven-20261025/candidates/c2fb9b5a81244ca9388c737d8f6b7edd7eda32a0`.
Independent proof `independent-haven-readback.json`, SHA-256
`ee84f412ee5e7c994327b6b622f296b33040dbb2af2abeea6401df7debe77d9e`.

The raw Windows logs contain intermediate command/failure-looking diagnostics,
but both completed gates returned zero and reported every step successful.
Retain those raw logs alongside the final receipts; do not infer a failed final
gate from an unfinished log excerpt or invent a cause for that output. A serial
retry script was prepared but not executed once the completed runs passed.

No game cycle, deployment, signing, macOS runtime or native visual qualification
has occurred in this slice. Direct collaboration and scoped continuity are not
Runner-governed execution. Later source changes need their own checks.

## Runtime routing

The current lifecycle wrapper still assumes the old Xmake repository layout.
Its prebuilt cycle source/target checks cannot simply be pointed at this Zig
worktree. A prebuilt deployment option is not by itself a qualified isolated
cycle path. Preserve those controls; do not switch the active checkout, invent
an old source layout or weaken source/target verification to force a trial.
The new user cycle authorization removes the earlier wave hold, but a correctly
routed candidate remains a prerequisite. Existing game functionality and the
remaining migration cohorts must also remain visible in any trial assessment.
