// Explicit test host for Config/Shortcut fixtures, which have no Unity runtime.
// The production mapping policies have their own retained fixtures.
#include "keyboard_layout_test_host.h"
#include <array>

namespace keyboard_layout
{
namespace
{
  bool                                      enabled = false;
  DiagnosticState                           diagnostics;
  std::array<ResolvedChord, LayoutKeyCount> mappings{};
  std::array<bool, LayoutKeyCount>          supplied{}, registered{};
} // namespace
void Configure(std::string_view mode)
{
  const bool next = mode == "layout";
  if (enabled == next)
    return;
  enabled = next;
  diagnostics.Publish("", enabled ? "pending" : "physical", enabled ? "awaiting_game_input" : "configured_physical");
}
void RegisterShortcut(KeyCode key) noexcept
{
  if (enabled && IsLayoutKey(key))
    registered[static_cast<int>(key)] = true;
}
DiagnosticSnapshot ReadDiagnostics()
{ return diagnostics.Snapshot(); }
bool DiagnosticsChanged(uint64_t accepted_generation) noexcept
{ return diagnostics.ChangedSince(accepted_generation); }
ResolvedChord ResolveChord(KeyCode configured)
{
  if (enabled && IsLayoutKey(configured) && supplied[static_cast<int>(configured)])
    return mappings[static_cast<int>(configured)];
  return {configured, false};
}
ResolvedChord DescribeChord(KeyCode configured)
{ return ResolveChord(configured); }
KeyCode CaptureIdentity(KeyCode physical, bool)
{ return physical; }
namespace fixture
{
  void Reset()
  {
    enabled     = false;
    diagnostics = {};
    mappings    = {};
    supplied    = {};
    registered  = {};
  }
  void Map(KeyCode configured, KeyCode physical, bool shift)
  {
    mappings[static_cast<int>(configured)] = {physical, shift};
    supplied[static_cast<int>(configured)] = true;
  }
  bool Registered(KeyCode key)
  { return registered[static_cast<int>(key)]; }
  void PublishDiagnostics(std::string_view layout, std::string_view status, std::string_view reason)
  { diagnostics.Publish(layout, status, reason); }
} // namespace fixture
} // namespace keyboard_layout
