#include "camera_settings.h"
#include "config.h"
#include "patches/runtime_config.h"

namespace mod_settings
{
SliderSetting& HavenZoomSetting()
{
  static SliderSetting setting({"community_mod.graphics.haven_zoom", "Haven zoom-out distance (0 = native)",
                                [] {
                                  return HavenCameraControlAvailable()
                                             ? ValueReadResult<float>::Known(Config::Get().haven_zoom, 1)
                                             : ValueReadResult<float>{};
                                },
                                [](float value, std::uint64_t generation) {
                                  if (generation != 1 || !HavenCameraControlAvailable())
                                    return ApplyResult::Rejected;
                                  Config::Get().haven_zoom = value;
                                  runtime_config::SaveSetting("graphics", "haven_zoom", static_cast<double>(value),
                                                              std::chrono::milliseconds(150));
                                  return ApplyResult::Applied;
                                }},
                               0, 6000, 250, HavenCameraControlAvailable, SliderLabel::Value, 0);
  return setting;
}

BooleanSetting& HavenWaterSetting()
{
  static BooleanSetting setting({"community_mod.graphics.hide_haven_water", "Hide Haven water",
                                 [] {
                                   return HavenWaterControlAvailable()
                                              ? ReadResult::Known(Config::Get().hide_haven_water, 1)
                                              : ReadResult{};
                                 },
                                 [](bool value, std::uint64_t generation) {
                                   if (generation != 1 || !HavenWaterControlAvailable())
                                     return ApplyResult::Rejected;
                                   Config::Get().hide_haven_water = value;
                                   runtime_config::SaveSetting("graphics", "hide_haven_water", value);
                                   return ApplyResult::Applied;
                                 }});
  return setting;
}

SliderSetting& KeyboardZoomSpeedSetting()
{
  // A convenient editing range, not a new TOML constraint. The slider's checked
  // reader leaves out-of-range player-authored values untouched and unavailable.
  static SliderSetting setting({"community_mod.graphics.keyboard_zoom_speed", "Keyboard zoom speed",
                                [] {
                                  return KeyboardZoomControlAvailable()
                                             ? ValueReadResult<float>::Known(Config::Get().keyboard_zoom_speed, 1)
                                             : ValueReadResult<float>{};
                                },
                                [](float value, std::uint64_t generation) {
                                  if (generation != 1 || !KeyboardZoomControlAvailable())
                                    return ApplyResult::Rejected;
                                  Config::Get().keyboard_zoom_speed = value;
                                  runtime_config::SaveSetting("graphics", "keyboard_zoom_speed",
                                                              std::round(static_cast<double>(value)),
                                                              std::chrono::milliseconds(150));
                                  return ApplyResult::Applied;
                                }},
                               0, 1000, 25, KeyboardZoomControlAvailable, SliderLabel::Value, 0);
  return setting;
}

SliderSetting& PanGlideSetting()
{
  // This is the retained motion per existing pan update. Stop short of 1.0,
  // which would retain all momentum indefinitely; do not change the pan formula.
  static SliderSetting setting({"community_mod.graphics.system_pan_momentum_falloff", "Pan glide (higher = longer)",
                                [] {
                                  return PanGlideControlAvailable() ? ValueReadResult<float>::Known(
                                                                          Config::Get().system_pan_momentum_falloff, 1)
                                                                    : ValueReadResult<float>{};
                                },
                                [](float value, std::uint64_t generation) {
                                  if (generation != 1 || !PanGlideControlAvailable())
                                    return ApplyResult::Rejected;
                                  Config::Get().system_pan_momentum_falloff = value;
                                  runtime_config::SaveSetting("graphics", "system_pan_momentum_falloff",
                                                              std::round(static_cast<double>(value) * 100.0) / 100.0,
                                                              std::chrono::milliseconds(150));
                                  return ApplyResult::Applied;
                                }},
                               0, 0.99f, 0.01f, PanGlideControlAvailable);
  return setting;
}
} // namespace mod_settings
