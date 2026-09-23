// Temporary play-dev science probe: selected cargo widget discovery, client263 Windows only.
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/rotating_file_sink.h>
#include <unordered_map>
namespace
{
using Json  = nlohmann::json;
using Clock = std::chrono::steady_clock;
#include "cargo_probe_targets.h"
std::shared_ptr<spdlog::logger> trace;
std::atomic_uint64_t            timingCalls{}, simpleCalls{};
std::mutex                      mutex;
size_t                          rows{}, burst{};
Clock::time_point               window{};
bool                            ready{};

FieldInfo* Field(void* obj, const char* name, const char* type, size_t size)
{
  if (!obj)
    return nullptr;
  auto* cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(obj));
  for (auto* p = cls; p; p = il2cpp_class_get_parent(p)) {
    auto* f = il2cpp_class_get_field_from_name(p, name);
    if (f && !(il2cpp_field_get_flags(f) & FIELD_ATTRIBUTE_STATIC) && !f->type->byref
        && f->offset >= sizeof(Il2CppObject) && f->offset + size <= il2cpp_class_instance_size(cls)
        && (!type || method_contract::Type(f->type, type)))
      return f;
  }
  return nullptr;
}
void* Ref(void* obj, const char* name)
{
  auto* f = Field(obj, name, nullptr, sizeof(void*));
  if (!f)
    return nullptr;
  const auto type = il2cpp_type_get_type(f->type);
  auto*      cls  = il2cpp_class_from_type(f->type);
  if (type != IL2CPP_TYPE_CLASS && type != IL2CPP_TYPE_OBJECT
      && !(type == IL2CPP_TYPE_GENERICINST && cls && !il2cpp_class_is_valuetype(cls)))
    return nullptr;
  void* result{};
  il2cpp_field_get_value(static_cast<Il2CppObject*>(obj), f, &result);
  return result;
}
std::string Class(void* obj)
{
  if (!obj)
    return "null";
  auto* cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(obj));
  return std::string(il2cpp_class_get_namespace(cls)) + "." + il2cpp_class_get_name(cls);
}
template <typename T> void Scalar(Json& j, void* obj, const char* name, const char* type)
{
  auto* f = Field(obj, name, type, sizeof(T));
  if (!f) {
    j[name] = nullptr;
    return;
  }
  T value{};
  il2cpp_field_get_value(static_cast<Il2CppObject*>(obj), f, &value);
  j[name] = value;
}
Json Data(void* obj)
{
  Json j = {{"class", Class(obj)}};
  if (Class(obj) == "Digit.PrimeServer.Models.CargoProgressData")
    obj = Ref(obj, "_data");
  for (auto* field : {"_currentValue", "_minValue", "_maxValue", "_normalizedValue"})
    Scalar<double>(j, obj, field, "System.Double");
  return j;
}
// Copies only numeric model state; never retain managed objects beyond a callback.
std::unordered_map<uintptr_t, Json> cargoModels;
void                                CaptureCargoModel(void* owner)
{
  if (Class(owner) == "Digit.Prime.FleetManagement.FleetInfoWidget")
    owner = Ref(owner, "_cargoHoldWidget");
  if (Class(owner) != "Digit.Prime.FleetManagement.CargoHoldWidget")
    return;
  auto* label = Ref(owner, "_colouredAmountLocalizer");
  if (!label)
    return;
  if (cargoModels.size() >= 32 && !cargoModels.contains(reinterpret_cast<uintptr_t>(label)))
    cargoModels.clear();
  auto j                                          = Data(Ref(Ref(owner, "m_context"), "_currentCargo"));
  j["sample_ms"]                                  = GetTickCount64();
  cargoModels[reinterpret_cast<uintptr_t>(label)] = std::move(j);
}
Json Snapshot(void* bar)
{
  auto* provider = Ref(bar, "m_provider");
  CaptureCargoModel(provider);
  auto* lerper = Ref(bar, "_lerper");
  Json  j      = {{"class", Class(bar)},
                  {"instance", reinterpret_cast<uintptr_t>(bar)},
                  {"provider", Class(provider)},
                  {"fleet_fill", Ref(provider, "_cargoFillBar") == bar && bar},
                  {"context", Data(Ref(bar, "m_context"))},
                  {"lerper", Class(lerper)},
                  {"result", Data(Ref(lerper, "_result"))},
                  {"target", Data(Ref(lerper, "_target"))}};
  Scalar<float>(j, lerper, "_duration", "System.Single");
  Scalar<float>(j, lerper, "_currentTime", "System.Single");
  Scalar<bool>(j, bar, "_snapToValue", "System.Boolean");
  Scalar<float>(j, bar, "_refreshPeriod", "System.Single");
  return j;
}
// Bound both output and snapshot work; no game state or retained object references.
bool Permit(bool display = false) noexcept
{
  try {
    if (!ready)
      return false;
    std::lock_guard lock(mutex);
    auto            now = Clock::now();
    if (now - window >= std::chrono::seconds(1)) {
      window = now;
      burst  = 0;
    }
    if (burst >= (display ? 60 : 4))
      return false;
    ++rows;
    ++burst;
    return true;
  } catch (...) {
    return false;
  }
}
void Observe(const char* event, void* bar) noexcept
{
  if (!Permit())
    return;
  try {
    auto j                       = Snapshot(bar);
    j["ms"]                      = GetTickCount64();
    j["event"]                   = event;
    j["simple_calls"]            = simpleCalls.load();
    j["duration_override_calls"] = timingCalls.load();
    trace->info("{}", j.dump());
  } catch (...) {
  }
}
void Fleet_Hook(auto original, void* self)
{
  original(self);
  try {
    CaptureCargoModel(self);
  } catch (...) {
  }
  Observe("fleet_set_after", Ref(self, "_cargoFillBar"));
}
void Display_Hook(auto original, void* self, void* data)
{
  original(self, data);
  if (!Permit(true))
    return;
  try {
    auto ownerClass = Class(Ref(self, "m_provider"));
    if (ownerClass != "Digit.Prime.FleetManagement.FleetInfoWidget"
        && ownerClass != "Digit.Prime.FleetManagement.CargoHoldWidget")
      return;
    auto j            = Snapshot(self);
    j["display_data"] = Data(data);
    j["text_class"]   = Class(Ref(self, "_currentDynamicValue"));
    static std::unordered_map<uintptr_t, std::string> previous;
    auto                                              key       = reinterpret_cast<uintptr_t>(self);
    auto                                              signature = j.dump();
    // Bounded deduplication; only store local pointer values, never managed references.
    if (previous.size() >= 128 && !previous.contains(key))
      previous.clear();
    if (previous[key] == signature)
      return;
    previous[key]                = std::move(signature);
    j["event"]                   = "display_after";
    j["ms"]                      = GetTickCount64();
    j["duration_override_calls"] = timingCalls.load();
    trace->info("{}", j.dump());
  } catch (...) {
  }
}
template <size_t N> bool Pinned(const MethodInfo* method, uintptr_t rva, const unsigned char (&bytes)[N])
{
  auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
  return base && method && reinterpret_cast<uintptr_t>(method->methodPointer) == base + rva
         && std::memcmp(reinterpret_cast<const void*>(method->methodPointer), bytes, N) == 0;
}
} // namespace
#endif
void CargoProbeSimple(void* self)
{
#if defined(_WIN32) && defined(_M_X64)
  ++simpleCalls;
  Observe("simple_start_after", self);
#endif
}
void CargoProbeDuration()
{
#if defined(_WIN32) && defined(_M_X64)
  ++timingCalls;
#endif
}
// Called only from the existing cargo-identifier-filtered formatting hook, after original.
void CargoProbeText(void* self, void* args) noexcept
{
#if defined(_WIN32) && defined(_M_X64)
  if (!ready || !args)
    return;
  try {
    static std::unordered_map<uintptr_t, ULONGLONG> last;
    auto                                            key = reinterpret_cast<uintptr_t>(self);
    auto                                            now = GetTickCount64();
    if (last.size() >= 32 && !last.contains(key))
      last.clear();
    if (now - last[key] < 50)
      return;
    last[key]   = now;
    auto* array = static_cast<Il2CppArray*>(args);
    if (il2cpp_array_length(array) > 8 || il2cpp_array_element_size(array->klass) != sizeof(void*))
      return;
    auto* values  = reinterpret_cast<Il2CppArraySize*>(array);
    Json  numbers = Json::array();
    for (uintptr_t i = 0; i < il2cpp_array_length(array); ++i) {
      auto* value = static_cast<Il2CppObject*>(values->vector[i]);
      auto  name  = Class(value);
      Json  item  = {{"class", name}};
      if (name == "System.Double")
        item["value"] = *static_cast<double*>(il2cpp_object_unbox(value));
      else if (name == "System.Single")
        item["value"] = *static_cast<float*>(il2cpp_object_unbox(value));
      else if (name == "System.Int32")
        item["value"] = *static_cast<int32_t*>(il2cpp_object_unbox(value));
      else if (name == "Digit.Client.UI.BoxedDouble")
        Scalar<double>(item, value, "DoubleValue", "System.Double");
      numbers.push_back(std::move(item));
    }
    void* stack[24]{};
    auto  count  = CaptureStackBackTrace(0, 24, stack, nullptr);
    auto  base   = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
    Json  frames = Json::array();
    for (USHORT i = 0; i < count; ++i) {
      auto address = reinterpret_cast<uintptr_t>(stack[i]);
      if (address >= base && address - base < 0x8000000)
        frames.push_back(address - base);
    }
    Json j = {{"event", "cargo_text_parameters"},
              {"ms", now},
              {"label", key},
              {"numbers", numbers},
              {"game_stack_rvas", frames}};
    if (auto it = cargoModels.find(key); it != cargoModels.end())
      j["model"] = it->second;
    trace->info("{}", j.dump());
  } catch (...) {
  }
#endif
}
void InstallCargoProbe()
{
#if defined(_WIN32) && defined(_M_X64)
  auto  fleet  = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.FleetManagement", "FleetInfoWidget");
  auto  simple = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.UI", "SimpleProgressBarWidget");
  auto* f      = fleet.GetMethodInfo("SetWidgetData");
  auto* l      = simple.GetMethodInfo("SetCurrentAndMaxValues", 1);
  if (!Pinned(f, kProbeRva0, kProbeWindow0) || !Pinned(l, kProbeRva2, kProbeWindow2)) {
    spdlog::warn("[CargoProbe] client fingerprint mismatch; not installed");
    return;
  }
  try {
    auto file = "community_cargo_probe_" + std::to_string(GetCurrentProcessId()) + "_"
                + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".jsonl";
    trace     = spdlog::rotating_logger_mt("cargo_probe", file, 2 * 1024 * 1024, 2);
    trace->set_pattern("%v");
    trace->flush_on(spdlog::level::info);
    trace->info("{}", Json({{"event", "session"},
                            {"client", 263},
                            {"limit", "60 samples/sec; 2MiB plus 2 rotations; read-only; display path"}})
                          .dump());
    ready = SPUD_STATIC_DETOUR(f->methodPointer, Fleet_Hook) != nullptr;
    ready = ready && SPUD_STATIC_DETOUR(l->methodPointer, Display_Hook) != nullptr;
    spdlog::info("[CargoProbe] installed={} file={}", ready, file);
  } catch (...) {
    spdlog::warn("[CargoProbe] installation unavailable");
  }
#endif
}
