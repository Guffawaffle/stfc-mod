#pragma once

#include <cstdint>
#include <string_view>

struct GameObject;
struct ScreenManager;
struct Transform;

namespace dev::overlay
{
struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Color {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
};

struct Insets {
  float left   = 0.0f;
  float top    = 0.0f;
  float right  = 0.0f;
  float bottom = 0.0f;
};

struct LayoutContext {
  float  width  = 0.0f;
  float  height = 0.0f;
  Insets reserved;
};

struct NativeElement {
  void* game_object_handle = nullptr;
  void* transform_handle   = nullptr;
  void* component_handle   = nullptr;
};

using ReservedRegionProvider = Insets (*)(float canvas_width, float canvas_height);
using PanelTick              = void (*)(Transform* host, const LayoutContext& context);
using PanelReset             = void (*)();

#ifdef _MODDBG
bool RegisterReservedRegionProvider(std::string_view owner, ReservedRegionProvider provider);
bool RegisterPanel(std::string_view owner, PanelTick tick, PanelReset reset);
void SetEnabled(bool enabled);
bool IsEnabled();
void Tick(ScreenManager* screen_manager);
void Reset();

NativeElement CreateElement(std::string_view name, Transform* parent, std::string_view assembly,
                            std::string_view namespc, std::string_view component);
GameObject*   GetGameObject(const NativeElement& element);
Transform*    GetTransform(const NativeElement& element);
void*         GetComponent(const NativeElement& element);
Transform*    GetComponentTransform(void* component);
bool          TryGetRectSize(Transform* transform, Vec2& size);
bool          IsAlive(const NativeElement& element);
void          ReleaseElement(NativeElement& element);
void          SetActive(const NativeElement& element, bool active);
bool          ConfigureRect(const NativeElement& element, Vec2 anchor_min, Vec2 anchor_max, Vec2 pivot, Vec2 size_delta,
                            Vec2 anchored_position);
bool          SetRaycastTarget(const NativeElement& element, bool enabled);
bool          SetGraphicColor(const NativeElement& element, Color color);
bool          SetText(const NativeElement& element, std::string_view text);
bool          ConfigureText(const NativeElement& element, float font_size, int32_t alignment, Color color);
void          BringToFront(const NativeElement& element);
#else
inline bool RegisterReservedRegionProvider(std::string_view, ReservedRegionProvider)
{ return false; }
inline bool RegisterPanel(std::string_view, PanelTick, PanelReset)
{ return false; }
inline void SetEnabled(bool) {}
inline bool IsEnabled()
{ return false; }
inline void Tick(ScreenManager*) {}
inline void Reset() {}
#endif
} // namespace dev::overlay
