#pragma once

// Configurable shortcuts: the actions they trigger, parsing [shortcuts] from the settings file, and
// matching the parsed key chords against this frame's input.

#include <toml++/impl/forward_declarations.hpp>
#include <unity/input.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// What a shortcut does. Each [shortcuts] setting binds key chords to one action.
enum class Action : uint8_t {
#define SHORTCUT(action, ...) action,
#include "shortcuts.def"
#undef SHORTCUT
  kCount
};

// One key with its required modifiers, such as CTRL-SHIFT-F1.
struct KeyChord {
  unity::KeyCode key = unity::KeyCode::None;
  struct Modifier {
    std::string_view name;
    // SHIFT accepts either side; LSHIFT stores LeftShift twice.
    std::array<unity::KeyCode, 2> keys;
    std::string_view              hint;
  };
  // Points into the immutable modifier catalog; spelling, matching and badge text share one definition.
  std::vector<const Modifier*> modifiers;

  // Parses one chord. Unknown tokens are ignored; a chord without a non-modifier key has key == None. With two
  // keys, the last one counts.
  static KeyChord Parse(std::string_view text);
  // Editor validation uses the same parser but refuses ignored tokens, multiple keys and duplicate modifiers.
  static std::optional<KeyChord> ParseExact(std::string_view text);

  // "CTRL-SHIFT-F1"
  std::string Text() const;
  // Compact form for the game's keybind badges: "^+F1".
  std::string Hint() const;

private:
  std::string key_name_;
};

namespace Shortcut
{
using Bindings = std::array<std::vector<KeyChord>, std::to_underlying(Action::kCount)>;
struct ActionInfo {
  Action           action;
  std::string_view key, defaults;
  bool             extended = false, experimental = false;
};
std::span<const ActionInfo> Catalog() noexcept;
struct BindingSnapshot {
  std::vector<std::string> alternatives;
  std::uint64_t            generation = 0;
};
BindingSnapshot Read(Action action);
// Prepare and validate all alternatives before replacing one action. Reload/replacement invalidates old drafts.
// This owns no persistence: the Settings definition submits the verified canonical value to its registered writer.
bool Replace(Action action, std::span<const std::string> alternatives, std::uint64_t expected_generation);
// Undo may retain duplicates already present in an observed source list; it still validates every chord and epoch.
bool Restore(Action action, const BindingSnapshot& previous, std::uint64_t expected_generation);
// Potential simultaneous match, using the dispatcher's current qualified layout. Unavailable mapping is unknown.
std::optional<bool> MayOverlap(const KeyChord& first, const KeyChord& second);
// Canonical token from the existing parser catalog; modifiers/unsupported keys have no standalone token.
std::string KeyToken(unity::KeyCode key);

// Parses [shortcuts] without changing live input. Records effective bindings and their sources in `parsed`.
Bindings Parse(const toml::table& config, toml::table& parsed, bool extended, bool experimental);
// Activates a complete set of bindings and releases keys owned by the previous set. Game thread only.
void Set(Bindings bindings) noexcept;

// The native editor claims before reading raw input, and releases on acceptance, cancellation or page departure.
// Tokens prevent a pooled editor's delayed cleanup from releasing its replacement. Game thread only.
using CaptureToken = std::uint64_t;
CaptureToken ClaimCapture() noexcept;
bool         CaptureOwnedBy(CaptureToken token) noexcept;
bool         CaptureReady(CaptureToken token) noexcept;
bool         ReleaseCapture(CaptureToken token) noexcept;
bool         CaptureBlocksShortcuts() noexcept;
// Once per frame, after Key::Poll and before feature callbacks. Focus is application focus; held includes all
// physical keys/modifiers and popup Enter/Escape, not just keys used by the configured shortcuts.
void UpdateCapture(bool focused, bool held) noexcept;

bool HasBinding(Action action);
// Printable chord identities follow keyboard_layout_mode; the input snapshot
// and ownership remain physical. Displayed/configured chord text is unchanged.
// Any binding of `action` is held with its modifiers. A binding without modifiers needs none held; one with
// modifiers still fires when extra modifiers are held (CTRL-F1 fires under CTRL+SHIFT).
bool IsPressed(Action action);
// Any binding of `action` went down this frame with its modifiers held.
bool IsDown(Action action);
// Ship towing reuses a configured selection chord with an additional held Shift.
// Bare chords still reject other modifiers and keep the usual physical ownership.
bool IsDownWithShift(Action action);
// Capture owns held keys through its release boundary. Otherwise, a binding with modifiers matched on `key`,
// which is still held. Plain bindings on it wait for the release.
bool Owns(unity::KeyCode key);

// "CTRL-G | F8"
std::string Describe(Action action);
// The badge text of the first binding, or empty when unbound.
std::string Hint(Action action);
} // namespace Shortcut
