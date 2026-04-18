/**
 * @file fix_pan.cc
 * @brief Fixes system-view camera panning and adds momentum falloff.
 *
 * The game's navigation pan has two issues this patch addresses:
 * 1. Touch input that reports "Stationary" phase is ignored, causing jittery
 *    panning — we reclassify it as "Moved" so the gesture stays smooth.
 * 2. After releasing a pan, the camera stops abruptly. We add configurable
 *    momentum falloff so the camera glides to a stop.
 */
#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include <prime/NavigationPan.h>
#include <prime/TKTouch.h>

#include <hook/hook.h>

/**
 * @brief Hook: TKTouch::populateWithPosition
 *
 * Intercepts touch input population to fix jittery panning.
 * Original method: populates a TKTouch struct with position and phase.
 * Our modification: converts Stationary phase to Moved so the pan gesture
 *   doesn't stall when the finger/cursor barely moves.
 */
typedef TKTouch* (*TKTouch_populateWithPosition_fn)(TKTouch*, uintptr_t, TouchPhase);
static TKTouch_populateWithPosition_fn TKTouch_populateWithPosition_original = nullptr;

TKTouch *TKTouch_populateWithPosition_Hook(TKTouch *_this, uintptr_t pos, TouchPhase phase)
{
  auto r = TKTouch_populateWithPosition_original(_this, pos, phase);
  if (r->phase == TouchPhase::Stationary) {
    r->phase = TouchPhase::Moved;
  }
  return r;
}

/**
 * @brief Hook: NavigationPan::LateUpdate
 *
 * Intercepts the per-frame pan update to add momentum-based camera glide.
 * Original method: processes pan input each frame and moves the camera.
 * Our modification: when no touch/mouse input is active and the camera isn't
 *   blocked or tracking a POI, applies a configurable falloff multiplier to
 *   the last pan delta so the camera decelerates smoothly. Also locks the
 *   extended far-zoom radius to the normal value.
 */
typedef bool (*NavigationPan_LateUpdate_fn)(NavigationPan*);
static NavigationPan_LateUpdate_fn NavigationPan_LateUpdate_original = nullptr;

bool NavigationPan_LateUpdate_Hook(NavigationPan *_this)
{
  auto d = _this->_lastDelta;

  if (!Config::Get().disable_move_keys) {
    NavigationPan_LateUpdate_original(_this);
  }

  static auto GetMouseButton = il2cpp_resolve_icall_typed<bool(int)>("UnityEngine.Input::GetMouseButton(System.Int32)");
  static auto GetTouchCount  = il2cpp_resolve_icall_typed<int()>("UnityEngine.Input::get_touchCount()");

  if (_this->BlockPan() || _this->_trackingPOI) {
    d->x = 0.0f;
    d->y = 0.0f;
  } else if (GetMouseButton(0) || GetTouchCount() > 0) {
    //
  } else {
    d->x = d->x * Config::Get().system_pan_momentum_falloff;
    d->y = d->y * Config::Get().system_pan_momentum_falloff;
    _this->MoveCamera(vec2{d->x, d->y}, true);
  }
  _this->_farMagRadiusRatioSystemExtended = _this->_farMagRadiusRatioSystemNormal;
  return true;
}

/**
 * @brief Installs pan-fix and momentum hooks.
 *
 * Hooks TKTouch::populateWithPosition (Stationary → Moved fix) and
 * NavigationPan::LateUpdate (momentum falloff).
 */
void InstallPanHooks()
{
  if (auto touchHelper = il2cpp_get_class_helper("TouchKit", "", "TKTouch"); !touchHelper.isValidHelper()) {
    ErrorMsg::MissingHelper("<global>", "TKTouch");
  } else {
    if (const auto ptr = touchHelper.GetMethod("populateWithPosition"); ptr == nullptr) {
      ErrorMsg::MissingMethod("TKTouch", "populateWithPosition");
    } else {
      MH_INSTALL(ptr, TKTouch_populateWithPosition_Hook, TKTouch_populateWithPosition_original);
    }
  }

  if (auto navHelper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationPan");
      !navHelper.isValidHelper()) {
    ErrorMsg::MissingHelper("Navigation", "NavigationPan");
  } else {
    if (const auto ptr = navHelper.GetMethod("LateUpdate"); ptr == nullptr) {
      ErrorMsg::MissingMethod("NavigationPan", "LateUpdate");
    } else {
      MH_INSTALL(ptr, NavigationPan_LateUpdate_Hook, NavigationPan_LateUpdate_original);
    }
  }
}
