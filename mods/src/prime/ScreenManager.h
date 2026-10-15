#pragma once

#include "CanvasController.h"
#include "CanvasScaler.h"
#include "MonoSingleton.h"

struct ScreenManager : MonoSingleton<ScreenManager> {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "ScreenManager">;

  static CanvasController* GetTopCanvas(bool visibleOnly = false)
  { return Class::Call<"GetTopCanvas", CanvasController*>(visibleOnly); }

  CanvasScaler* canvasRootScaler()
  { return Field<"m_canvasRootScaler", CanvasScaler*>(); }
  float minimumCanvasScaleFactor()
  { return Field<"_minimumCanvasScaleFactor", float>(); }
  float maximumCanvasScaleFactor()
  { return Field<"_maximumCanvasScaleFactor", float>(); }
};
