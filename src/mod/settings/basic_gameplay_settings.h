#pragma once

// Shared identities and mutations for existing native shortcuts. No game
// objects, files or UI catalog: callers supply the registered save boundary.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <string>

namespace gameplay_settings
{
struct Key {
  const char*               section;
  const char*               name;
  std::chrono::milliseconds delay{};
};
enum class Boolean : std::size_t {
  Hotkeys,
  PreviewLocate,
  PreviewRecall,
  CargoDefault,
  CargoPlayer,
  CargoStation,
  CargoHostile,
  CargoArmada,
  Queue,
  HideHavenWater,
  Count
};
inline constexpr std::array<Key, static_cast<std::size_t>(Boolean::Count)> booleans{{
    {"control", "hotkeys_enabled"},
    {"ui", "disable_preview_locate"},
    {"ui", "disable_preview_recall"},
    {"ui", "show_cargo_default"},
    {"ui", "show_player_cargo"},
    {"ui", "show_station_cargo"},
    {"ui", "show_hostile_cargo"},
    {"ui", "show_armada_cargo"},
    {"control", "queue_enabled"},
    {"graphics", "hide_haven_water"},
}};
inline constexpr const Key&                                                Identity(Boolean option)
{ return booleans.at(static_cast<std::size_t>(option)); }

enum class Scale : std::size_t { Interface, Viewer, Ship, Count };
struct ScaleDefinition {
  Key   identity;
  float maximum;
  float step_multiplier;
  bool  zero_is_native;
};
inline constexpr std::array<ScaleDefinition, static_cast<std::size_t>(Scale::Count)> scales{{
    {{"graphics", "ui_scale", std::chrono::milliseconds(250)}, 2.0f, 1.0f, true},
    {{"graphics", "ui_scale_viewer", std::chrono::milliseconds(250)}, 2.0f, 0.25f, true},
    {{"graphics", "ui_scale_ship", std::chrono::milliseconds(250)}, 20.0f, 1.0f, false},
}};
struct ScaleChange {
  bool  applied;
  float previous, requested, value;
};
template <class Submit> ScaleChange AdjustScale(float& value, float step, bool up, Scale option, Submit&& submit)
{
  const auto& definition = scales.at(static_cast<std::size_t>(option));
  const auto  previous   = value;
  if (definition.zero_is_native && value == 0.0f)
    return {false, previous, previous, previous};
  const auto requested = value + (up ? 1.0f : -1.0f) * step * definition.step_multiplier;
  value                = std::clamp(requested, 0.1f, definition.maximum);
  submit(definition.identity, static_cast<double>(value)); // Exact float; no display rounding.
  return {true, previous, requested, value};
}
template <class Submit> void SetBoolean(bool& value, bool desired, Boolean option, Submit&& submit)
{
  value = desired;
  submit(Identity(option), value);
}
template <class Submit> void ToggleBoolean(bool& value, Boolean option, Submit&& submit)
{ SetBoolean(value, !value, option, submit); }

enum class Zoom : std::size_t { Preset1, Preset2, Preset3, Preset4, Preset5, Default, Count };
inline constexpr std::array<Key, static_cast<std::size_t>(Zoom::Count)> zooms{{
    {"graphics", "system_zoom_preset_1", std::chrono::milliseconds(250)},
    {"graphics", "system_zoom_preset_2", std::chrono::milliseconds(250)},
    {"graphics", "system_zoom_preset_3", std::chrono::milliseconds(250)},
    {"graphics", "system_zoom_preset_4", std::chrono::milliseconds(250)},
    {"graphics", "system_zoom_preset_5", std::chrono::milliseconds(250)},
    {"graphics", "default_system_zoom", std::chrono::milliseconds(250)},
}};
inline constexpr const Key&                                             Identity(Zoom option)
{ return zooms.at(static_cast<std::size_t>(option)); }
inline float CurrentZoom(float distance, float minimum, float maximum, float configured_maximum)
{ return (distance - minimum) / (maximum - minimum) * configured_maximum; }
template <class Submit> void StoreZoom(float& value, float observed, Zoom option, Submit&& submit)
{
  value = observed;
  submit(Identity(option), static_cast<double>(value));
}
template <class Retain> void UsePresetAsDefault(float& value, float observed, Retain&& retain)
{
  value = observed;
  // Old automatic StoreZoom supplied no save key: keep it session-only.
  retain(Identity(Zoom::Default), static_cast<double>(value));
}

inline constexpr Key                     warp_mode{"ui", "auto_confirm_instant_warp"};
inline constexpr std::array              warp_names{"none", "warp", "jump"};
template <class Mode, class Submit> bool CycleWarp(Mode& value, Submit&& submit)
{
  const auto current = static_cast<std::size_t>(value);
  if (current >= warp_names.size())
    return false;
  const auto next = (current + 1) % warp_names.size();
  value           = static_cast<Mode>(next);
  submit(warp_mode, std::string(warp_names[next]));
  return true;
}

template <class Register> bool RegisterKeys(Register&& registrar)
{
  bool ready = true;
  for (const auto& key : booleans)
    ready = registrar(key) && ready;
  for (const auto& scale : scales)
    ready = registrar(scale.identity) && ready;
  for (const auto& key : zooms)
    ready = registrar(key) && ready;
  return registrar(warp_mode) && ready;
}
} // namespace gameplay_settings
