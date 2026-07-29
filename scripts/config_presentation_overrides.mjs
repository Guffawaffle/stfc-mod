/**
 * Sparse player-copy exceptions for the generated launcher presentation.
 *
 * These entries may improve display-only metadata. The schema generator
 * validates every path and field and rejects attempts to redefine runtime
 * types, defaults, constraints, apply behavior, or persistence metadata.
 */
export const configPresentationOverrides = [
  {
    path: "graphics.fr_scale",
    label: "System backdrop scale",
    help: "Fills the system backdrop at extreme zoom distances.",
    group: "Camera",
    unit: "×",
    editorWidth: "compact",
    searchTerms: ["backdrop", "background scale"],
  },
];
