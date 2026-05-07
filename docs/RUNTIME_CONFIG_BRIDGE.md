# Runtime Config Bridge

Branch note for `guffa/notification-config-rethink`.

This describes how a future companion app can let players test setting changes
while STFC is already running, without BepInEx and without runtime TOML polling.

## Short Answer

Yes, runtime config updates are possible without BepInEx.

The mod is already injected into the STFC process through `version.dll` on
Windows and the dylib path on macOS. That injected code can own a small local IPC
endpoint, accept schema-validated setting patches from the companion app, queue
them, and apply them to in-memory mod state.

BepInEx is not required for this. It would only be another way to get managed
code into the process. We already have native code in the process.

## Non-Negotiables

- Do not re-read `community_patch_settings.toml` during runtime.
- Do not watch config files for changes.
- Do not expose a gameplay command channel.
- Do not let the sidecar poke arbitrary game objects.
- Do not make the injected mod host a rich UI or updater.
- Do not make every setting live-editable.

The runtime bridge is not "reload config." It is "apply these typed setting
changes."

## Ownership Split

Companion app responsibilities:

- render the settings UI from the schema catalog;
- validate edits before sending them;
- persist the sparse user overlay for the next launch;
- send runtime patches only for settings marked live-applyable;
- show which changes applied immediately and which require restart;
- own install, update, launch, diagnostics, and local viewer UX.

Injected mod responsibilities:

- load persistent config once during startup;
- expose a narrow local runtime-config endpoint only when enabled;
- authenticate local requests with a launch-scoped capability token;
- validate paths and value types against the same schema catalog;
- enqueue accepted changes and apply them on a safe thread/context;
- emit result events and update runtime inspection output.

Sidecar responsibilities:

- stay outside the game process;
- talk to the mod only through local, documented IPC;
- never send gameplay actions or arbitrary method invocations.

## Runtime Data Flow

```text
Settings UI
  -> schema validation in companion app
  -> persist sparse overlay for next launch
  -> send JSON patch over local IPC
  -> injected mod validates schema path/type/capability token
  -> enqueue runtime setting update
  -> apply safe changes in memory
  -> return applied/rejected/restart-required response
  -> emit local diagnostics event
```

The TOML file remains useful as import/export and startup persistence, but it is
not part of the live-edit loop.

## IPC Choice

Preferred first transport:

- Windows: named pipe with a per-process name and local-user ACL.
- macOS: Unix domain socket with a per-process path and restrictive file mode.

Why not localhost HTTP first:

- The mod already has HTTP client code for sync, but not a small hardened HTTP
  server.
- A named pipe/socket is easier to keep local-only and capability-scoped.
- Electron, Node, and native launchers can talk to named pipes/sockets without
  requiring the game process to host web infrastructure.

The companion app can still expose its own localhost UI/server. That is separate
from the mod-side runtime bridge.

## Capability Token

At startup, the mod should generate a random launch-scoped token and publish it
only through a local file that inherits game-directory permissions, for example:

```text
community_patch_runtime_endpoint.json
```

Example shape:

```json
{
  "protocolVersion": 1,
  "pid": 12345,
  "transport": "windows-named-pipe",
  "endpoint": "\\\\.\\pipe\\stfc-community-mod-12345",
  "token": "redacted-in-logs",
  "startedAt": "2026-05-03T00:00:00Z"
}
```

Requests without the current token are rejected. The token is regenerated on each
game launch and should never be written to ordinary logs or diagnostic exports.

## Request Shape

Runtime updates should be structured patches, not TOML fragments:

```json
{
  "protocolVersion": 1,
  "requestId": "c9a58c33-0fd4-4f43-9ef0-68e3f92e4f1d",
  "token": "launch-token",
  "patch": [
    {
      "path": "notifications.audio.enabled",
      "value": true
    },
    {
      "path": "notifications.audio.fleet.arrived_in_system",
      "value": true
    }
  ]
}
```

Response:

```json
{
  "ok": true,
  "requestId": "c9a58c33-0fd4-4f43-9ef0-68e3f92e4f1d",
  "results": [
    {
      "path": "notifications.audio.enabled",
      "status": "applied"
    },
    {
      "path": "patches.zoomhooks",
      "status": "restart-required",
      "message": "Hook installation settings are read at startup."
    }
  ]
}
```

Allowed result statuses:

- `applied`
- `rejected`
- `restart-required`
- `unknown-setting`
- `invalid-type`
- `unsupported-platform`

## Schema Metadata

The config schema needs one more dimension: runtime apply behavior.

Suggested values:

- `live`: safe to apply while the game is running.
- `next-session`: can be persisted now but only takes effect on the next game
  launch.
- `restart-required`: requires the game process to restart because hooks,
  resources, or startup-only state are involved.
- `internal`: never exposed in the player settings UI.

Examples:

| Setting | Runtime behavior | Notes |
|---|---|---|
| `notifications.audio.enabled` | `live` | Toggles delivery policy only. |
| `notifications.audio.fleet.arrived_in_system` | `live` | Event observer already checks config at decision time. |
| `notifications.system.enabled` | `live` | Safe if notification delivery reads current policy. |
| `control.hotkeys_enabled` | `live` | Already toggled by hotkey at runtime. |
| `graphics.ui_scale` | `live` or `next-session` | Needs a targeted apply function to refresh scale. |
| `graphics.zoom` | `live` or `next-session` | Safe only through existing camera update paths. |
| `sync.targets.*.url` | `next-session` | Avoid swapping active network credentials mid-send. |
| `patches.*` hook toggles | `restart-required` | Hook install/uninstall is startup-owned. |
| `diagnostics.refinery` | `restart-required` | Hook install is gated at startup. |

The UI can still let a player edit `next-session` and `restart-required` values,
but it should show that they will not affect the current running game.

## Applying Changes Safely

Do not mutate arbitrary `Config::Get()` fields from a background IPC thread.

Preferred implementation path:

1. Add a `RuntimeConfigPatch` queue owned by the mod.
2. The IPC listener thread validates and enqueues patches only.
3. A safe hook tick drains the queue and applies changes.
4. Each live setting has a narrow apply function, for example
   `apply_notifications_audio_enabled(bool)`.
5. For simple read-mostly booleans, use an atomic/runtime store where possible
   instead of relying on unsynchronized public struct fields.

The current code already has precedent for runtime-safe accessors such as
`AllowKeyFallthrough()`, `LiveDebugChannelEnabled()`, and
`SyncSidecarJsonlRecentLogs()`. Future live-editable settings should move toward
that accessor/store style rather than expanding the mutable `Config` struct.

## Persistence Semantics

The companion app persists first, then runtime-applies.

That gives predictable recovery behavior:

- if runtime apply succeeds, the player sees the change immediately;
- if the game is not running, the setting still applies next launch;
- if runtime apply fails, the app can either roll back the persisted edit or mark
  it as pending for next launch;
- if the game crashes later, the persisted overlay remains the source for the next
  launch.

The mod should not write back to the user TOML during live updates. It may update
runtime inspection output or emit a `config.runtime_patch` sidecar event.

## Player UX

The settings app should make this feel direct:

- Immediate settings show an "Applied" state.
- Restart-only settings show "Saved for next launch."
- Failed settings stay visibly unsaved or show the validation error inline.
- A "Test" action can send a non-persistent preview patch for settings that
  support preview semantics, such as audio cues.

For notifications, this is especially useful: a player can enable audio, click a
test button, then wait for a real fleet-arrival event without restarting STFC.

## First Implementation Slice

Keep the first slice intentionally small:

1. Extend the schema pilot with runtime behavior metadata.
2. Add a runtime-config bridge module, compiled only when the sidecar/runtime
   bridge setting is enabled.
3. Implement `ping`, `schema`, and `apply` for booleans only.
4. Support only notification audio booleans first:
   - `notifications.audio.enabled`
   - `notifications.audio.fleet.arrived_in_system`
5. Emit structured result logs/events.
6. Add companion-side UI wiring after the protocol is stable.

This proves live edits with a safe feature that has no hook-install side effects.

## Packaging Direction

Release packaging can offer two tracks:

- raw mod artifact: current `version.dll` or macOS dylib/launcher assets;
- companion installer: app bundle that can install/update the mod, host the local
  viewer/settings UI, and talk to the runtime bridge when STFC is running.

The companion can be Electron first because the sidecar is already TypeScript and
local-web oriented. The core should remain renderer-neutral so Overwolf can later
be another client rather than the foundation.

The sidecar repo owns the companion app plan and UI direction. The intended
visual direction is an original LCARS-inspired interface, but direct use of
TheLCARS.com template files requires redistribution permission before bundling
them in any release artifact.