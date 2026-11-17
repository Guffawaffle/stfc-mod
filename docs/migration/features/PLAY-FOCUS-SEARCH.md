# PLAY-FOCUS-SEARCH — focus the active native list search

Contract v1; complete frozen focus_search.cc and hotkey caller read, with
CanvasController, inventory/officer/assignment input bindings, config aliases,
Settings and shared keyboard focus/capture owners. Exact refs:
[cohort](../FEATURE-COHORT-20261029-02.md).

CTRL-F remains editable. Inventory/roster precedes officer assignment, then ship
assignment. Only visible active owners and active fields qualify. Target keeps
the existing shared outer dispatcher for chat, text input and capture precedence;
the helper also checks current feature enablement/input focus. Managed owner and
field custody plus rechecking field identity prevent stale focus after a rebind.

Actual controlled consumers cover enable/disable, hidden/inactive owners/fields,
replacement, and all three native field families. The void Focus attempt consumes
the shortcut once; successful delivery is not proof native focus settled. No
secondary focus is attempted after a potentially partially executed native call.

Canonical `ui.focus_search` retains precedence over legacy `patches.focussearch`,
including malformed-canonical fallback. Existing parser regressions cover those
semantics. No defaults, config paths, saved values or text-input architecture
changes. Real typing/chat/native Canvas and Mac checks remain separate.
