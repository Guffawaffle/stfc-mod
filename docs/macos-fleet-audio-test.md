# Focused macOS fleet-audio baseline

`testing/macos-fleet-audio` combines upstream 1.1.8.0 (`4bf238c1`) with:

- Cross-platform notification audio (#253, `7933965b`).
- The named-event `All` audio filter (#259, `cfe2227a`).
- Fleet-transition notifications (#268 consumer commits `d5c8a010`, `49d8bc75`).
- Independent generated fleet audio (`ae7e64bc`).
- Xcode launcher build compatibility (#291, `bfa22bbd`), including the macOS 14.6
  deployment target and updated CI tools.

The Fleet Watch observer already comes from upstream. This branch does not include
the rest of the personal play stack and does not yet change arrival detection.
The component revisions above are pinned; later feeder-branch refreshes do not
automatically change this test build.

## Testing

Requires macOS 14.6 or newer. Use the universal macOS installer or mod archive from this branch's successful
Build run. Back up the existing mod and TOML first, close the game before replacing
the mod, and retain the exact run URL with the results. Do not switch builds during
a comparison. Verify the startup log identifies the installed test build.

First test with the same settings that showed the problem. Record the relevant
`[audio]` settings and whether it is the generated cue or the original game sound
that goes missing. `All` affects the named Fabric event route, not every game audio
path; generated mod cues remain independent.

For an isolated generated-arrival test, edit these keys inside the existing
`[audio]` section (do not append a duplicate section):

```toml
disabled_events = "All"
alert_fleet_arrived_in_system = "arrival"
alert_fleet_arrived_at_destination = "none"
```

Temporarily set other `alert_*` keys to `"none"` if isolating overlapping sounds.
Fleet cues do not require desktop notifications or toast banner hooks. There is no
generated departure cue in this baseline.

Compare short and long warps, one fleet and several fleets, viewed and off-screen
destinations, and a focused versus background game window. For a missed cue, record
the approximate time, fleet, destination, warp duration, visible fleet state and
whether another alert played. Keep the session log. `trace_events = true` logs named
game audio events only; it does not trace generated fleet transitions.

Restore the previous mod and configuration to undo the test. Build success is not
macOS runtime validation: the purpose of this baseline is to establish a repeatable
case before stacking a separate arrival correction.
