#pragma once

// Input System keyboard observations and its device-change notification boundary.
// Call only from normal game-thread input processing, after runtime startup.
#include "input.h"

#include <atomic>
#include <string>

namespace unity::keyboard_layout
{
struct Keyboard : il2cpp::Object {
  using Class = il2cpp::Class<"Unity.InputSystem", "UnityEngine.InputSystem", "Keyboard">;
};
using InputSystem      = il2cpp::Class<"Unity.InputSystem", "UnityEngine.InputSystem", "InputSystem">;
using InputActionState = il2cpp::Class<"Unity.InputSystem", "UnityEngine.InputSystem", "InputActionState">;

inline bool Observe(std::string& layout)
{
  const auto current = Keyboard::Class::TryCall<"get_current", Keyboard*>();
  auto       root    = il2cpp::GcHandle::Strong(current.value);
  if (!current || !root)
    return false;
  const auto name = root.Get<Keyboard>()->TryCall<"get_keyboardLayout", Il2CppString*>();
  auto       text = il2cpp::GcHandle::Strong(reinterpret_cast<Il2CppObject*>(name.value));
  if (!name || !text)
    return false;
  layout = il2cpp::ToUtf8(reinterpret_cast<Il2CppString*>(text.Get()));
  return !layout.empty();
}

enum class ObserverResult { Started, Unsupported, Failed };

#if _WIN32
namespace detail
{
  struct Observer {
    MethodInfo        method{};
    const MethodInfo *add = nullptr, *remove = nullptr;
    il2cpp::GcHandle  root;
    void (*notify)() noexcept = nullptr;
    std::atomic<bool> accepting{false};
    bool              attempted = false, active = false, subscribed = false;
  };
  inline Observer& State()
  {
    // Delegate metadata and uncertain subscriptions must outlive any native callback.
    static auto* state = new Observer;
    return *state;
  }
  inline void Changed(Il2CppObject*, int32_t change, const MethodInfo*) noexcept
  {
    auto& state = State();
    if (state.accepting.load(std::memory_order_acquire) && ((change >= 0 && change <= 5) || change == 7))
      state.notify();
  }
  inline bool Type(const Il2CppType* type, Il2CppTypeEnum expected)
  { return type && !type->byref && type->type == expected; }
} // namespace detail
#endif

inline void Stop()
{
#if _WIN32
  auto& state = detail::State();
  state.accepting.store(false, std::memory_order_release);
  state.active = false;
  if (state.subscribed) {
    auto* callback = state.root.Get();
    void* arguments[]{callback};
    if (!callback || !il2cpp::detail::TryInvokeBoxed(state.remove, nullptr, arguments))
      return; // Keep the root/metadata if listener removal is uncertain.
    state.subscribed = false;
  }
  state.root.Reset();
#endif
}

inline ObserverResult Start(void (*notify)() noexcept)
{
#if _WIN32
  auto& state = detail::State();
  if (state.attempted)
    return state.active ? ObserverResult::Started : ObserverResult::Failed;
  state.attempted = true;
  state.notify    = notify;
  try {
    state.add         = InputSystem::StaticMethod<void(Il2CppObject*)>("add_onDeviceChange").Info();
    state.remove      = InputSystem::StaticMethod<void(Il2CppObject*)>("remove_onDeviceChange").Info();
    const auto* donor = InputActionState::StaticMethod<void(Il2CppObject*, int32_t)>("OnDeviceChange").Info();
    if (!notify || !state.add || !state.remove || !donor || !state.add->parameters || !state.remove->parameters
        || !donor->parameters || donor->is_generic || donor->is_inflated || donor->has_full_generic_sharing_signature
        || !il2cpp_type_equals(state.add->parameters[0], state.remove->parameters[0]))
      return ObserverResult::Failed;
    auto*       delegate_class = il2cpp_class_from_type(state.add->parameters[0]);
    const auto* invoke = delegate_class ? il2cpp_class_get_method_from_name(delegate_class, "Invoke", 2) : nullptr;
    if (!invoke || !invoke->parameters || !detail::Type(invoke->return_type, IL2CPP_TYPE_VOID)
        || (invoke->flags & METHOD_ATTRIBUTE_STATIC))
      return ObserverResult::Failed;
    for (unsigned index = 0; index < 2; ++index) {
      const auto* parameter = donor->parameters[index];
      const auto* target    = invoke->parameters[index];
      if (!parameter || !target || parameter->byref || target->byref || !il2cpp_type_equals(parameter, target)
          || parameter->type != (index == 0 ? IL2CPP_TYPE_CLASS : IL2CPP_TYPE_VALUETYPE))
        return ObserverResult::Failed;
      if (index == 1) {
        auto* enumeration = il2cpp_class_from_type(parameter);
        if (!enumeration || !il2cpp_class_is_enum(enumeration)
            || !detail::Type(il2cpp_class_enum_basetype(enumeration), IL2CPP_TYPE_I4))
          return ObserverResult::Failed;
      }
    }
    state.method                      = *donor;
    state.method.methodPointer        = reinterpret_cast<Il2CppMethodPointer>(&detail::Changed);
    state.method.virtualMethodPointer = state.method.methodPointer;
    auto* callback                    = il2cpp::NewDelegate(delegate_class, il2cpp::Method(&state.method));
    state.root                        = il2cpp::GcHandle::Strong(callback);
    auto* bound                       = reinterpret_cast<Il2CppDelegate*>(state.root.Get());
    if (!bound || bound->method != &state.method || bound->method_ptr != state.method.methodPointer) {
      state.root.Reset();
      return ObserverResult::Failed;
    }
    state.accepting.store(true, std::memory_order_release);
    state.subscribed = true; // Failed invocation can still have changed the event.
    void* arguments[]{callback};
    state.active = il2cpp::detail::TryInvokeBoxed(state.add, nullptr, arguments).succeeded;
  } catch (...) {
    state.active = false;
  }
  if (!state.active)
    Stop();
  return state.active ? ObserverResult::Started : ObserverResult::Failed;
#else
  (void)notify;
  return ObserverResult::Unsupported;
#endif
}
} // namespace unity::keyboard_layout
