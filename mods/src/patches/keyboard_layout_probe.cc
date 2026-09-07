#include "keyboard_layout_probe.h"

#if defined(_WIN32) && defined(_MODDBG)
#include "il2cpp/il2cpp_helper.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

namespace keyboard_layout::probe
{
namespace
{
  enum class Phase { Cold, FirstEvent, Detached, Reattached, Done };
  struct State {
    Phase phase = Phase::Cold;
    MethodInfo native_method{}; // Private copy; never modify a game's MethodInfo.
    const MethodInfo *add = nullptr, *remove = nullptr, *queue = nullptr, *current = nullptr, *updates = nullptr;
    Il2CppGCHandle delegate_root = 0, keyboard_root = 0;
    std::atomic<uintptr_t> keyboard_identity{0}; // Identity comparison only in callback.
    std::atomic<unsigned> configs{0}, direct_calls{0};
    std::atomic<bool> accepting{true};
    bool subscribed = false; // Includes uncertain add failures, for conservative cleanup.
    unsigned baseline = 0, add_calls = 0, remove_calls = 0;
    uint32_t detached_at = 0;
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  };

  State* live_state = nullptr; // Only the game thread initializes or inspects this pointer.

  State& Get()
  {
    // Callback metadata/state must outlive any subscription, including failed removal.
    static auto* state = [] { live_state = new State; return live_state; }();
    return *state;
  }

  void Changed(void* device, int32_t change, const MethodInfo*) noexcept
  {
    auto& state = Get();
    if (!state.accepting.load())
      return;
    if (!device && change == -1) {
      ++state.direct_calls; // Private delegate invocation test, never a queued device event.
    } else if (change == 7 && reinterpret_cast<uintptr_t>(device) == state.keyboard_identity.load()) {
      ++state.configs;
    }
    // No logging, Unity calls, allocation, key polling, or action resolution here.
  }

  bool Invoke(const MethodInfo* method, void* self, void** args, Il2CppObject** result = nullptr)
  {
    if (!method)
      return false;
    Il2CppException* exception = nullptr;
    auto* value = il2cpp_runtime_invoke(method, self, args, &exception);
    if (result)
      *result = exception ? nullptr : value;
    return exception == nullptr;
  }

  bool Subscribe()
  {
    auto& state = Get();
    if (state.subscribed)
      return true;
    auto* delegate = il2cpp_gchandle_get_target(state.delegate_root);
    if (!delegate)
      return false;
    void* args[]{delegate};
    state.subscribed = true; // An exception need not imply that the event was untouched.
    ++state.add_calls;
    return Invoke(state.add, nullptr, args);
  }

  bool Unsubscribe()
  {
    auto& state = Get();
    if (!state.subscribed)
      return true;
    auto* delegate = il2cpp_gchandle_get_target(state.delegate_root);
    if (!delegate)
      return false;
    void* args[]{delegate};
    ++state.remove_calls;
    if (!Invoke(state.remove, nullptr, args))
      return false;
    state.subscribed = false;
    return true;
  }

  void Stop(const char* reason, bool passed = false)
  {
    auto& state = Get();
    const bool removed = Unsubscribe();
    state.accepting = false;
    if (removed) {
      if (state.delegate_root) il2cpp_gchandle_free(state.delegate_root);
      if (state.keyboard_root) il2cpp_gchandle_free(state.keyboard_root);
      state.delegate_root = state.keyboard_root = 0;
    } // Retain roots on uncertain removal; never leave a dangling subscribed delegate.
    state.phase = Phase::Done;
    spdlog::info("[LayoutProbe] result={} reason={} removed={} adds={} removes={} direct={} config_events={}",
                 passed && removed ? "pass" : "fail", reason, removed, state.add_calls, state.remove_calls,
                 state.direct_calls.load(), state.configs.load());
  }

  bool UpdateCount(uint32_t& count)
  {
    Il2CppObject* boxed = nullptr;
    if (!Invoke(Get().updates, nullptr, nullptr, &boxed) || !boxed)
      return false;
    count = *static_cast<uint32_t*>(il2cpp_object_unbox(boxed));
    return true;
  }

  bool QueueConfiguration()
  {
    auto& state = Get();
    auto* keyboard = il2cpp_gchandle_get_target(state.keyboard_root);
    Il2CppObject* current = nullptr;
    if (!keyboard || !Invoke(state.current, nullptr, nullptr, &current) || keyboard != current)
      return false;
    double time = -1;
    void* args[]{keyboard, &time};
    // Unity invalidation event only: no OS layout change, key/text events, or manual InputSystem.Update.
    return Invoke(state.queue, nullptr, args);
  }

  bool Start()
  {
    auto& state = Get();
    auto input = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputSystem");
    auto keyboard = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "Keyboard");
    auto actions = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputActionState");
    auto input_state = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem.LowLevel", "InputState");
    state.add = input.GetMethodInfo("add_onDeviceChange", 1);
    state.remove = input.GetMethodInfo("remove_onDeviceChange", 1);
    state.queue = input.GetMethodInfo("QueueConfigChangeEvent", 2);
    state.current = keyboard.GetMethodInfo("get_current", 0);
    state.updates = input_state.GetMethodInfo("get_updateCount", 0);
    const auto* signature = actions.GetMethodInfo("OnDeviceChange", 2);
    if (!state.add || !state.remove || !state.queue || !state.current || !state.updates || !signature)
      return false;
    auto* delegate_class = il2cpp_class_from_type(il2cpp_method_get_param(state.add, 0));
    if (!delegate_class)
      return false;
    const auto* ctor = il2cpp_class_get_method_from_name(delegate_class, ".ctor", 2);
    const auto* invoke = il2cpp_class_get_method_from_name(delegate_class, "Invoke", 2);
    // Use a real non-generic static method's exact signature, not fabricated parameter metadata.
    if (!ctor || !ctor->invoker_method || !invoke || !(signature->flags & 0x0010)
        || signature->is_generic || signature->is_inflated || signature->parameters_count != 2
        || il2cpp_type_get_type(signature->return_type) != IL2CPP_TYPE_VOID)
      return false;
    for (unsigned i = 0; i < 2; ++i) {
      if (il2cpp_class_from_type(il2cpp_method_get_param(signature, i))
          != il2cpp_class_from_type(il2cpp_method_get_param(invoke, i)))
        return false;
    }
    Il2CppObject* current = nullptr;
    if (!Invoke(state.current, nullptr, nullptr, &current) || !current)
      return false;
    state.keyboard_root = il2cpp_gchandle_new(current, true);
    if (!state.keyboard_root)
      return false;
    state.keyboard_identity = reinterpret_cast<uintptr_t>(current);
    auto* delegate = il2cpp_object_new(delegate_class);
    if (!delegate)
      return false;
    state.delegate_root = il2cpp_gchandle_new(delegate, true);
    if (!state.delegate_root)
      return false;
    state.native_method = *signature;
    state.native_method.methodPointer = reinterpret_cast<Il2CppMethodPointer>(&Changed);
    state.native_method.virtualMethodPointer = state.native_method.methodPointer;
    const MethodInfo* entry = &state.native_method;
    void* ctor_args[]{nullptr, &entry};
    // Existing native delegate precedent: invoke the AOT constructor's invoker directly.
    // runtime_invoke's special delegate-constructor path rejects a null static target here.
    try {
      ctor->invoker_method(ctor->methodPointer, ctor, delegate, ctor_args, nullptr);
    } catch (...) {
      return false;
    }
    auto* typed = reinterpret_cast<Il2CppDelegate*>(delegate);
    if (typed->method != entry || typed->method_ptr != state.native_method.methodPointer)
      return false;
    int32_t sentinel = -1;
    void* test_args[]{nullptr, &sentinel};
    if (!Invoke(invoke, delegate, test_args) || state.direct_calls != 1)
      return false;
    if (!Subscribe() || !Subscribe() || !QueueConfiguration())
      return false;
    state.phase = Phase::FirstEvent;
    spdlog::info("[LayoutProbe] subscribed; direct delegate call passed; queued same-layout notification");
    return true;
  }
}

void Tick()
{
  static const bool enabled = [] {
    const auto* value = std::getenv("STFC_LAYOUT_NOTIFICATION_PROBE");
    return value && std::strcmp(value, "selftest") == 0;
  }();
  if (!enabled)
    return;
  auto& state = Get();
  if (state.phase == Phase::Done)
    return;
  if (std::chrono::steady_clock::now() - state.started > std::chrono::seconds(15)) {
    Stop("timeout");
    return;
  }
  if (state.phase == Phase::Cold) {
    if (!Start()) Stop("setup_or_direct_invocation_failed");
    return;
  }
  const unsigned configs = state.configs.load();
  if (state.phase == Phase::Detached) {
    uint32_t now = 0;
    if (!UpdateCount(now)) { Stop("update_counter_unavailable"); return; }
    if (configs != state.baseline) { Stop("callback_after_remove"); return; }
    if (now - state.detached_at < 3)
      return;
    if (!Subscribe() || !Subscribe() || !QueueConfiguration()) { Stop("reattach_failed"); return; }
    state.phase = Phase::Reattached;
    return;
  }
  if (configs == state.baseline)
    return;
  if (configs != state.baseline + 1) { Stop("unexpected_extra_configuration_events"); return; }
  if (state.phase == Phase::FirstEvent) {
    state.baseline = configs;
    if (!Unsubscribe() || !QueueConfiguration() || !UpdateCount(state.detached_at)) {
      Stop("detach_failed");
      return;
    }
    state.phase = Phase::Detached;
    spdlog::info("[LayoutProbe] first event received; removed listener for three input updates");
  } else {
    Stop("same_layout_delivery_detach_and_reattach", true);
  }
}

void Cancel()
{
  if (live_state && live_state->phase != Phase::Done)
    Stop("layout_resolver_failed");
}
}
#endif
