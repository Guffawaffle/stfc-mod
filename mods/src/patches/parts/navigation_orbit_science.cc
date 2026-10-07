#include "patches/navigation_orbit_science.h"

#ifdef _MODDBG
#include "errormsg.h"
#include "version.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <prime/Hub.h>
#include <prime/Vector3.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace navigation_orbit_science
{
namespace
{
  using Clock                                                = std::chrono::steady_clock;
  using Json                                                 = nlohmann::json;
  constexpr unsigned                            limit        = 4096;
  unsigned                                      emitted      = 0;
  bool                                          orbit_active = false;
  float                                         orbit_yaw = 0.0f, orbit_pitch = 0.0f;
  bool                                          orbit_dragging = false;
  int                                           sampled_frame  = -1;
  Clock::time_point                             next_sample{};
  std::shared_ptr<spdlog::details::thread_pool> pool;
  std::shared_ptr<spdlog::async_logger>         logger;

  bool Enabled()
  {
    static const bool enabled = [] {
      const auto *value = std::getenv("STFC_MOD_SCIENCE_ORBIT_PROBE");
      const bool  on    = value && std::strcmp(value, "1") == 0;
      if (!on)
        return false;
      try {
        const auto stamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        const auto filename = "community_navigation_orbit_" + std::to_string(stamp) + ".jsonl";
        auto       sink     = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename, 2 * 1024 * 1024, 2);
        pool                = std::make_shared<spdlog::details::thread_pool>(128, 1);
        logger              = std::make_shared<spdlog::async_logger>("navigation-orbit-science", sink, pool,
                                                                     spdlog::async_overflow_policy::overrun_oldest);
        logger->set_pattern("%v");
        logger->flush_on(spdlog::level::info); // Worker thread only; never flush the game thread.
        logger->info("{}", Json{{"event", "start"},
                                {"source", STFC_SOURCE_STATE_ID},
                                {"invocation", STFC_BUILD_INVOCATION_ID},
                                {"schema", 1},
                                {"snapshot_hz", 5},
                                {"event_limit", limit},
                                {"scope", "Sampled camera and existing pan hook; not every native pan/clamp call"}}
                               .dump());
        spdlog::info("[NavigationOrbitScience] enabled; read-only 5 Hz snapshots, max {} events: {}", limit, filename);
        return true;
      } catch (...) {
        logger.reset();
        return false;
      }
    }();
    return enabled && emitted < limit;
  }

  int Frame()
  {
    static auto get = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
    return get ? get() : -1;
  }

  bool Window(bool force)
  {
    if (!Enabled())
      return false;
    if (force)
      return true;
    auto *sections = Hub::get_SectionManager();
    if (!orbit_active || !sections || sections->CurrentSection != SectionID::Navigation_System)
      return false;
    const int frame = Frame();
    if (frame < 0)
      return false;
    const auto now = Clock::now();
    if (frame != sampled_frame && now >= next_sample) {
      sampled_frame = frame;
      next_sample   = now + std::chrono::milliseconds(200);
    }
    return frame == sampled_frame;
  }

  FieldInfo *Field(Il2CppObject *object, const char *name, const char *type)
  {
    auto *field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
    return field && !(il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC)
                   && field->offset >= static_cast<int>(sizeof(Il2CppObject))
                   && method_contract::Type(field->type, type)
               ? field
               : nullptr;
  }

  Json Vector(Vector3 v)
  { return Json::array({v.x, v.y, v.z}); }

  template <typename T> void Scalar(Json &data, Il2CppObject *object, const char *name, const char *type)
  {
    if (auto *field = Field(object, name, type)) {
      T value{};
      il2cpp_field_get_value(object, field, &value);
      data[name] = value;
    } else {
      data[name] = nullptr;
    }
  }

  void VectorField(Json &data, Il2CppObject *object, const char *name)
  {
    if (auto *field = Field(object, name, "UnityEngine.Vector3")) {
      Vector3 value{};
      il2cpp_field_get_value(object, field, &value);
      data[name] = Vector(value);
    } else {
      data[name] = nullptr;
    }
  }

  void PairField(Json &data, Il2CppObject *object, const char *name)
  {
    if (auto *field = Field(object, name, "UnityEngine.Vector2")) {
      float values[2]{};
      il2cpp_field_get_value(object, field, values);
      data[name] = Json::array({values[0], values[1]});
    } else {
      data[name] = nullptr;
    }
  }

  const MethodInfo *TransformMethod(const char *name, const char *type)
  {
    static auto cls = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Transform");
    return method_contract::Resolve(cls.get_cls(), name, false, type, {});
  }

  Json TransformVector(Il2CppObject *transform, const MethodInfo *method)
  {
    Il2CppObject *result = nullptr;
    if (!transform || !Il2CppRuntime::TryInvoke(method, transform, nullptr, &result) || !result
        || !method_contract::Type(il2cpp_class_get_type(result->klass), "UnityEngine.Vector3"))
      return nullptr;
    const auto *value = static_cast<Vector3 *>(il2cpp_object_unbox(result));
    return value ? Vector(*value) : Json(nullptr);
  }

  Json Transform(Il2CppObject *component)
  {
    static auto   cls = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
    static auto   get = method_contract::Resolve(cls.get_cls(), "get_transform", false, "UnityEngine.Transform", {});
    static auto   position  = TransformMethod("get_position", "UnityEngine.Vector3");
    static auto   local     = TransformMethod("get_localPosition", "UnityEngine.Vector3");
    static auto   angles    = TransformMethod("get_localEulerAngles", "UnityEngine.Vector3");
    Il2CppObject *transform = nullptr;
    if (!component || !Il2CppRuntime::TryInvoke(get, component, nullptr, &transform) || !transform)
      return nullptr;
    return {{"position", TransformVector(transform, position)},
            {"local_position", TransformVector(transform, local)},
            {"local_angles", TransformVector(transform, angles)}};
  }

  Json Camera(Il2CppObject *component)
  {
    auto         *field  = Field(component, "_sceneCamera", "UnityEngine.Camera");
    Il2CppObject *camera = nullptr;
    if (field)
      il2cpp_field_get_value(component, field, &camera);
    return Transform(camera);
  }

  void Emit(Json data, const char *phase, Il2CppObject *object)
  {
    data["sequence"] = ++emitted;
    data["frame"]    = Frame();
    data["time_ms"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    data["phase"]  = phase;
    data["object"] = reinterpret_cast<uintptr_t>(object);
    data["orbit"]  = {
        {"active", orbit_active}, {"yaw", orbit_yaw}, {"pitch", orbit_pitch}, {"dragging", orbit_dragging}};
    data["queue_overruns"] = pool->overrun_counter();
    // Diagnostic failures must never change camera behavior or escape a game hook.
    logger->info("{}", data.dump());
    if (emitted == limit)
      logger->info("{}", R"({"event":"limit","message":"Restart with opt-in for another capture"})");
  }
} // namespace

void Orbit(void *raw, const char *phase, float yaw, float pitch, bool overridden, bool dragging, bool force)
{
  try {
    if (!Enabled())
      return;
    orbit_active   = overridden || dragging;
    orbit_yaw      = yaw;
    orbit_pitch    = pitch;
    orbit_dragging = dragging;
    if (!Window(force))
      return;
    auto *zoom = static_cast<Il2CppObject *>(raw);
    Json  data;
    data["transform"]  = Transform(zoom);
    data["camera"]     = Camera(zoom);
    data["overridden"] = overridden;
    Scalar<int>(data, zoom, "_depth", "Digit.PrimeServer.Models.NodeDepth");
    for (auto *name : {"_actualDistance", "_minimum", "_maximum", "_viewRadius", "<NormalizedZoom>k__BackingField"})
      Scalar<float>(data, zoom, name, "System.Single");
    VectorField(data, zoom, "_worldPoint");
    PairField(data, zoom, "_zoomLocation");
    Emit(std::move(data), phase, zoom);
    if (force)
      orbit_active = false; // Clear/failure/reset events end the current observation window.
  } catch (...) {
    orbit_active = false;
  }
}

void Pan(void *raw, const char *phase)
{
  try {
    if (!Window(false))
      return;
    auto *pan = static_cast<Il2CppObject *>(raw);
    Json  data;
    data["transform"] = Transform(pan);
    data["camera"]    = Camera(pan);
    Scalar<int>(data, pan, "_nodeDepth", "Digit.PrimeServer.Models.NodeDepth");
    for (auto *name : {"_viewRadius", "_softClampRange", "_outOfBoundsReturnCoeff", "_wasTrackingCoolDown",
                       "_farMagRadiusRatioSystemNormal", "_farMagRadiusRatioSystemExtended",
                       "_nearMagRadiusRatioSystemNormal", "_nearMagRadiusRatioSystemExtended"})
      Scalar<float>(data, pan, name, "System.Single");
    Scalar<bool>(data, pan, "_panningStarted", "System.Boolean");
    PairField(data, pan, "_lastDelta");
    VectorField(data, pan, "_systemPosition");
    if (auto *field = Field(pan, "_trackingPOI", "Digit.Prime.Navigation.POI")) {
      Il2CppObject *poi = nullptr;
      il2cpp_field_get_value(pan, field, &poi);
      data["tracking"] = poi != nullptr;
    }
    Emit(std::move(data), phase, pan);
  } catch (...) {
    orbit_active = false;
  }
}
} // namespace navigation_orbit_science
#endif
