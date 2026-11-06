# Local diagnostic captures

The capture service records bounded, observation-only event snapshots without doing
file work in a game hook. It provides shared plumbing for future recorders such as
claim diagnostics. This change does not register a game recorder, install a new
hook, start recording, add a settings page, or restore the old claim logger.

`src/mod/diagnostics/capture.h` is the controller and producer interface.
`storage.h` is its local-file backend. Neither layer calls game APIs or uploads
captures. Session controls are independent of a UI framework so a later STFCkitUI
page can use the same interface.

## Ownership and concurrency

A controller owns one `diagnostics::Service`, its immutable recorder registry,
build/client description, and an explicit absolute destination. The service owns
one writer thread shared by all its recorders. Each recorder has its own queue,
session, state, counters, and admission limits. Global limits bound the combined
memory use, including a snapshot that the writer is currently processing.

The writer services active recorders fairly. A busy recorder cannot consume every
other recorder's queue allowance. A slow filesystem can still delay all recorders
on that writer; producers remain bounded and report dropped submissions. Start
with one service. Introduce another service and writer only when measurements
justify isolation, using a separately owned destination.

Game-facing producers read game state on the thread that owns it, copy only the
necessary scalar values, and submit an owned `Event`. They never enqueue managed
objects, pointers into game memory, callbacks that read the game later, or raw
protobuf/response dumps. The writer serializes and writes the copied values.

Call `IsRecording` before constructing a snapshot. `TrySubmit` does not wait for
the writer or queue lock: it reports whether the event was accepted, recording was
off, the queue was busy/full, or the event was invalid/too large. This does not make
snapshot construction allocation-free. Keep producer work small and bounded.

For a snapshot that completes asynchronously, obtain a `CaptureTicket` with
`BeginSnapshot` before gathering it and pass that ticket to `TrySubmit`. A ticket
identifies the recorder, session generation, and service instance. A stopped or
restarted session refuses an old ticket, so delayed observations cannot enter a
new capture accidentally. The index-only overload is for immediate snapshots.

## Recorder and session lifecycle

The registry is fixed when the service is constructed. Inclusion is a build fact;
availability is a separate assessment of the current client. Included recorders
start Off, or Unavailable with a reason when their capability is absent. A future
settings page must not show a recorder omitted from the build as merely Off.

`Start` and `Stop` request state changes. The writer opens and closes sessions;
returning from either call does not mean its file work has completed. Status
distinguishes Unavailable, Off, Starting, Recording, Stopping, and Failed.

`Start` accepts an optional duration. Zero records until explicitly stopped.
`Mark` submits a local note through the same bounded event path. Neither operation
changes gameplay. Recovery actions, injected faults, forced transitions, and other
active experiments belong outside this observation-only API.

`GetStatus` returns in-memory snapshots for a controller. It does not read files
and does not poll the game. Status exposes pending work and drop/error counters.
Accepted, written, and flushed counts describe the current or last completed
session. Drop counters span the service's lifetime, across session restarts, so
a concurrent producer's refusal is not lost or assigned to a new session. Do not
interpret those lifetime totals as per-session loss counts.

Accepted, written, and flushed counts mean different things:

| Count | Meaning |
| --- | --- |
| Accepted | The service admitted the owned snapshot into its bounded work. |
| Written | The sink accepted its serialized record. |
| Flushed | The sink's stream flush completed for that record. |

A stream flush is not an `fsync` durability guarantee. No count establishes that
every relevant game event was observed or that a recipient received a capture.
Controllers must expose observed loss and writer errors. Matching accepted and
written counts alone does not establish that a capture is complete evidence.

Controller teardown calls `Shutdown` to drain accepted events and join the writer.
This can wait for disk work. Do not call it from an input callback or game hook.
All `Sink` methods and the optional sink factory run exclusively on the writer.
A custom sink must not call game APIs and must tolerate cleanup after a partial
open failure.

## Destination and retention

The destination is an explicit machine-local absolute path. The backend owns its
`captures-v1` subtree and takes an exclusive lease there, so two services or game
processes cannot concurrently write the same capture store. It does not select a
game installation, infer an account, or silently fall back to another directory.

The destination is immutable for a service lifetime. To change it, stop and drain
the old service, then construct a new service with the new path. This plumbing
does not persist a path in shared TOML, choose a profile's location, provide a
folder picker, or move existing captures.

Storage enforces per-file, per-recorder, and total size limits plus a free-space
reserve. Retention applies only to recognized captures beneath its owned subtree.
It does not clean arbitrary files, the old `science/claims` directory, mod logs,
settings, or other diagnostic stores. Filesystem errors fail the affected capture
and are exposed to the controller; they do not trigger a gameplay recovery action.
Recorder-specific write errors do not shut down unrelated recorders. A shared
destination or volume failure can affect several recorders, and the single writer
does not isolate them from a slow or unavailable filesystem.

## Evidence and data boundaries

Every recorder defines its event schema and increments its schema version when
the meaning changes. Use `std::monostate` for an unknown/null observation rather
than translating a failed read into false or zero. Positive observations and
missing observations remain different evidence.

The controller supplies source/build, client, and platform descriptions for the
capture manifest. Those descriptions establish context, not artifact authenticity.
Targeted diagnostic deployments must still bind the selected recorder set to the
actual artifact and its verified signing/provenance evidence.

Recorder authors must allowlist fields deliberately. The generic scalar API does
not identify or redact secrets automatically. Do not submit authentication tokens,
account/session credentials, chat contents, or unrelated personal data. Treat
locally captured player or alliance identifiers as private diagnostic data and
design their representation for the stated investigation.

Captures remain local. Sharing, export packaging, uploads, and any eventual UI
explanation of the recorded fields are separate work; no automatic transfer is
provided here.

## Run the focused Windows fixtures

The standalone runner compiles only the capture service, storage backend, and
`tests/diagnostics_capture_test.cc`. It does not build the mod, modify the main
build graph or CI, deploy files, or start the game.

From the repository root, with Zig 0.16 or 0.17 on PATH:

```powershell
pwsh scripts/test-diagnostics.ps1
```

Use `-ZigExecutable` for a compiler outside PATH. The runner uses an existing
pinned JSON include under `zig-pkg`, or accepts `-JsonInclude` pointing to the
directory containing `nlohmann/json.hpp`. For the Windows SDK, use an x64 Visual
Studio developer shell, let the runner find Visual Studio through `vswhere`, or
pass `-VsDevShell` or `-MsvcSysroot` explicitly. It downloads no dependencies.

Each run writes its executable, synthetic captures, logs, and receipt to a unique
directory under `.zig-cache/diagnostics-tests`. `-OutputDirectory` overrides that
local output location. The tests use synthetic records, not an STFC session.
Passing them does not establish a game's hook fit, recorder coverage, or native
Mac runtime behavior.
