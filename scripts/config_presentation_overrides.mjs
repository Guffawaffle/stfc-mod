const savedZoomFamily = (memberLabel, memberOrder) => ({
  id: "camera.saved-zoom-positions",
  parentGroup: "Camera",
  label: "Save zoom positions",
  help: "Store the current system zoom as the default or in a numbered preset.",
  displayOrder: 10,
  presentationHint: "compact-binding-list",
  memberLabel,
  memberOrder,
});

/**
 * Sparse player-copy exceptions for the generated launcher presentation.
 *
 * These entries may improve display-only metadata. The schema generator
 * validates every path and field and rejects attempts to redefine runtime
 * types, defaults, constraints, apply behavior, or persistence metadata.
 */
export const configPresentationOverrides = [
  {
    path: "input.bindings.set_zoom_default",
    label: "Save current zoom as default",
    help: null,
    group: "Camera",
    family: savedZoomFamily("Default", 0),
  },
  ...[1, 2, 3, 4, 5].map((preset) => ({
    path: `input.bindings.set_zoom_preset${preset}`,
    label: `Save current zoom to preset ${preset}`,
    help: null,
    group: "Camera",
    family: savedZoomFamily(`Preset ${preset}`, preset),
  })),
  {
    path: "graphics.fr_scale",
    label: "System backdrop scale",
    help: "Fills the system backdrop at extreme zoom distances.",
    group: "Camera",
    unit: "×",
    editorWidth: "compact",
    searchTerms: ["backdrop", "background scale"],
  },
  {
    path: "sidecar.logging.jsonl",
    label: "Save local evidence log",
    help: "Writes a local diagnostic feed for offline review. Leave this off unless you need evidence capture.",
    group: "Local sidecar",
  },
  {
    path: "sidecar.logging.jsonl_recent_logs",
    label: "Retained battle-log groups",
    help: "Limits how many recent battle-log groups remain in the local evidence feed.",
    group: "Local sidecar",
    unit: "groups",
    editorWidth: "compact",
  },
  {
    path: "sidecar.logging.jsonl_replay_seconds",
    label: "Replay window",
    help: "Limits how long entries remain available for local replay.",
    group: "Local sidecar",
    unit: "seconds",
    editorWidth: "compact",
  },
  {
    path: "sidecar.sync.enabled",
    label: "Local sidecar delivery",
    help: "Sends supported mod data to the STFC Mod Sidecar running on this PC.",
    group: "Local sidecar",
  },
  {
    path: "sidecar.sync.battlelog_enrichment",
    label: "Battle-log enrichment",
    help: "Turns local battle journals into player, ship, catalog, and analytics details.",
    group: "Local sidecar",
  },
  {
    path: "sidecar.sync.battlelogs_realtime",
    label: "Real-time battle capture",
    help: "Sends raw battle events to the local sidecar as they occur.",
    group: "Local sidecar",
  },
  {
    path: "sidecar.sync.fleet_runtime",
    label: "Fleet runtime snapshots",
    help: "Shares low-rate fleet status with the local sidecar.",
    group: "Local sidecar",
  },
  {
    path: "sidecar.sync.fleet_runtime_mode",
    label: "Fleet runtime behavior",
    help: "Controls how fleet runtime data moves through the local sidecar pipeline.",
    group: "Local sidecar",
    enumOptions: [
      {
        value: "normal",
        label: "Normal",
        help: "Read, queue, and deliver fleet runtime snapshots.",
      },
      {
        value: "request_only",
        label: "Requests only",
        help: "Record snapshot requests without reading fleet state.",
      },
      {
        value: "snapshot_only",
        label: "Build snapshots only",
        help: "Read and build owned snapshots without queueing or publishing them.",
      },
      {
        value: "enqueue_no_transport",
        label: "Queue without delivery",
        help: "Keep the latest snapshot queued without sending it over HTTP.",
      },
    ],
  },
  {
    path: "sidecar.sync.url",
    label: "Sidecar address",
    help: "Address of the STFC Mod Sidecar running on this PC.",
    group: "Local sidecar",
  },
  {
    path: "sidecar.sync.token",
    label: "Sidecar access token",
    help: "Credential used to authenticate with the local sidecar.",
    group: "Local sidecar",
  },
  {
    path: "sidecar.sync.proxy",
    label: "Sidecar proxy",
    help: null,
    group: "Local sidecar",
  },
  {
    path: "sidecar.sync.verify_ssl",
    label: "Verify sidecar TLS certificates",
    help: "Rejects sidecar connections whose TLS certificate cannot be verified.",
    group: "Local sidecar",
  },
  {
    path: "sidecar.sync.allow_unsafe_tls_without_certificate_validation",
    label: "Allow unverified sidecar TLS",
    help: "Allows an encrypted sidecar connection without checking its certificate. Use only for a trusted local setup.",
    group: "Local sidecar",
  },
  {
    path: "sync.resolver_cache_ttl",
    label: "DNS cache lifetime",
    help: "Controls how long resolved sync addresses remain cached.",
    unit: "seconds",
    editorWidth: "compact",
  },
  {
    path: "sync.allow_unsafe_tls_without_certificate_validation",
    label: "Allow unverified sync TLS",
    help: "Allows encrypted sync connections without checking their certificates. Use only with trusted targets.",
    group: "Sync connection",
  },
  {
    path: "sync.battlelogs",
    label: "Battle-log reports",
    help: "Shares completed battle reports with configured sync targets.",
    group: "Shared data",
  },
  {
    path: "sync.battlelogs_realtime",
    label: "Real-time battle events",
    help: "Shares battle events with configured sync targets as they occur.",
    group: "Shared data",
  },
  {
    path: "sync.debug",
    label: "Sync debug logging",
    help: "Adds diagnostic detail to sync logs.",
    group: "Sync diagnostics",
  },
  {
    path: "sync.logging",
    label: "Raw sync payload logging",
    help: "Records complete sync payloads for diagnostics. These logs may contain sensitive game data.",
    group: "Sync diagnostics",
  },
  {
    path: "sync.fleet_runtime",
    label: "Fleet runtime state",
    help: "Shares low-rate fleet-bar state with configured sync targets.",
    group: "Shared data",
  },
];
