# PLAY-SHORTCUT-HINTS — native shortcut badges

Contract v1; complete frozen hotkeys.cc hint mapping, text, visibility,
installation and top-level dispatch read. Target `features/keybind_badges.cc`,
existing native InputAction/TextLocalizer/ShortcutsManager bindings, shortcut
formatter and ship-position mapping. Exact refs: [cohort](../FEATURE-COHORT-20261029-02.md).

| Obligation | Target / qualification |
| --- | --- |
| H1 | Install even if toggle starts unbound; later Settings assignment works without restart. Static installer correction; native assignment trial separate |
| H2 | Clear old override, native UpdateText once, then current mod hint only in mod mode. Actual consumer fixture |
| H3 | Side-chat primary then fallback; unknown/unbound action displays dash. Actual mapping/refresh cases; formatting separately covered by mod logic |
| H4 | Ship hint maps actual fleet-array position to native dock index; unavailable index cannot become0. Owned source array and checked read; gap mapping helper regressions retained |
| H5 | A replaced label/reference cannot receive stale publication. Actual native-rebind fixture |
| H6 | Visible refresh updates text; toggle respects native gate and known current visibility; native-mode switch hides badges. Checked consumers and native trial separate |

HOME remains the default editable toggle. Source initialization and native field
availability are separate from successful hook installation. Temporary roots
end with the callback; no retained shell, scanner or second shortcut registry.
No assumption that a failed visibility read means hidden. Controlled manager
calls do not establish actual modal behavior or Mac hook fit.
