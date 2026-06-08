/**
 * @file notification_service.cc
 * @brief OS-native notification delivery for in-game toast events.
 *
 * Resolves IL2CPP LanguageManager::Localize at init time, maps toast states
 * to human-readable titles, strips Unity rich text tags from body text, and
 * delivers Windows toast notifications via WinRT for configured toast types.
 */
#include "patches/notification_service.h"
#include "patches/battle_notify_parser.h"

#include "bounded_ttl_cache.h"
#include "config.h"
#include "patches/async_work_queue.h"
#include "patches/notification_audio.h"
#include "patches/notification_platform.h"
#include "patches/notification_policy.h"
#include "patches/notification_queue.h"
#include "patches/notification_text.h"
#include "platform_config.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/LanguageManager.h>
#include <prime/Toast.h>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <exception>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// ─── IL2CPP Method Cache ──────────────────────────────────────────────────────────────

/** Cached LanguageManager::Localize(out string, LocaleTextContext) method pointer. */
static const MethodInfo* s_localize_ltc             = nullptr;
static bool              s_notification_initialized = false;

static FieldInfo* find_il2cpp_field(Il2CppClass* klass, const char* primary_name, const char* fallback_name = nullptr)
{
  if (!klass || !primary_name) {
    return nullptr;
  }

  auto* field = il2cpp_class_get_field_from_name(klass, primary_name);
  if (!field && fallback_name) {
    field = il2cpp_class_get_field_from_name(klass, fallback_name);
  }

  if (!field) {
    spdlog::warn("[Notify] Could not resolve {}.{} field",
                 il2cpp_class_get_name(klass) ? il2cpp_class_get_name(klass) : "<unknown>", primary_name);
  }

  return field;
}

template <typename T>
static T* read_il2cpp_reference_field(Il2CppObject* obj, const char* primary_name, const char* fallback_name = nullptr)
{
  if (!obj || !obj->klass) {
    return nullptr;
  }

  auto* field = find_il2cpp_field(obj->klass, primary_name, fallback_name);
  if (!field) {
    return nullptr;
  }

  const auto field_offset = il2cpp_field_get_offset(field);
  return *reinterpret_cast<T**>(reinterpret_cast<char*>(obj) + field_offset);
}

template <typename T> static T read_il2cpp_boxed_value(Il2CppObject* obj)
{ return *reinterpret_cast<T*>(reinterpret_cast<char*>(obj) + sizeof(Il2CppObject)); }

// ─── Toast State → Human-Readable Title ───────────────────────────────────────────────

/**
 * @brief Map a numeric toast state to a notification title string.
 * @param state The Toast::State enum value.
 * @return Static title string, or nullptr for unmapped states.
 */
static const char* toast_state_title(int state)
{
  switch (state) {
    case Victory:
      return "Victory!";
    case Defeat:
      return "Defeat";
    case PartialVictory:
      return "Partial Victory";
    case StationVictory:
      return "Station Victory!";
    case StationDefeat:
      return "Station Defeat";
    case StationBattle:
      return "Station Under Attack!";
    case IncomingAttack:
      return "Incoming Attack!";
    case IncomingAttackFaction:
      return "Incoming Faction Attack!";
    case FleetBattle:
      return "Fleet Battle";
    case ArmadaBattleWon:
      return "Armada Victory!";
    case ArmadaBattleLost:
      return "Armada Defeated";
    case ArmadaCreated:
      return "Armada Created";
    case ArmadaCanceled:
      return "Armada Canceled";
    case ArmadaIncomingAttack:
      return "Armada Under Attack!";
    case AssaultVictory:
      return "Assault Victory!";
    case AssaultDefeat:
      return "Assault Defeat";
    case Tournament:
      return "Event Progress";
    case ChainedEventScored:
      return "Event Progress";
    case Achievement:
      return "Achievement";
    case ChallengeComplete:
      return "Challenge Complete";
    case ChallengeFailed:
      return "Challenge Failed";
    case TakeoverVictory:
      return "Takeover Victory!";
    case TakeoverDefeat:
      return "Takeover Defeat";
    case TreasuryProgress:
      return "Treasury Progress";
    case TreasuryFull:
      return "Treasury Full";
    case WarchestProgress:
      return "Warchest Progress";
    case WarchestFull:
      return "Warchest Full";
    case FactionLevelUp:
      return "Faction Level Up";
    case FactionLevelDown:
      return "Faction Level Down";
    case FactionDiscovered:
      return "Faction Discovered";
    case FactionWarning:
      return "Faction Warning";
    case DiplomacyUpdated:
      return "Diplomacy Updated";
    case StrikeHit:
      return "Strike Hit";
    case StrikeDefeat:
      return "Strike Defeat";
    case SurgeWarmUpEnded:
      return "Surge Started";
    case SurgeHostileGroupDefeated:
      return "Surge Hostiles Defeated";
    case SurgeTimeLeft:
      return "Surge Time Warning";
    case ArenaTimeLeft:
      return "Arena Time Warning";
    case FleetPresetApplied:
      return "Fleet Preset Applied";
    default:
      return nullptr;
  }
}

const char* notification_toast_title(int state)
{ return toast_state_title(state); }

// ─── Platform Notification Delivery ──────────────────────────────────────────────────
#if STFCMOD_PLATFORM_WINDOWS
static constexpr size_t                         kNotificationQueueMaxDepth = 256;
static AsyncWorkQueue<NotificationQueueRequest> s_notification_queue(kNotificationQueueMaxDepth);
static std::mutex                               s_recent_toast_mutex;
static std::once_flag                           s_notification_worker_once;
static std::thread                              s_notification_worker_thread;
static constexpr auto                           kNotificationCoalesceWindow    = std::chrono::milliseconds(750);
static constexpr auto                           kRecentToastDedupWindow        = std::chrono::milliseconds(500);
static constexpr size_t                         kRecentToastDedupMaxEntries    = 256;
static constexpr size_t                         kNotificationSummaryLimit      = 4;
static constexpr auto                           kNotificationJoinWarnThreshold = std::chrono::seconds(5);
static BoundedTtlDeduper<uintptr_t>             s_recent_toasts(kRecentToastDedupMaxEntries);

bool notification_should_process_toast(Toast* toast)
{
  if (!toast) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  const auto key = reinterpret_cast<uintptr_t>(toast);

  std::lock_guard lock(s_recent_toast_mutex);
  const auto      dedupe_result = s_recent_toasts.should_emit(key, now, kRecentToastDedupWindow);
  if (!dedupe_result.emitted) {
    spdlog::debug("[Notify] Duplicate toast pointer {:p}, suppressing repeated notification pass", (const void*)toast);
    return false;
  }

  return true;
}

static void queue_system_notification(const char* title, const char* body, const char* source)
{
  NotificationQueueRequest request;
  if (source) {
    request.source = source;
  }
  if (title) {
    request.title = title;
  }
  if (body) {
    request.body = body;
  }
  request.queued_at = std::chrono::steady_clock::now();

  if (!s_notification_queue.enqueue(std::move(request))) {
    const auto diagnostics = s_notification_queue.diagnostics();
    spdlog::warn("[NotifyQueue] drop source={} title='{}' reason={} queue_size={} dropped={}",
                 source ? source : "unknown", title ? notification_flatten_text(title) : "",
                 diagnostics.shutdown_requested ? "shutdown" : "full", diagnostics.depth, diagnostics.dropped);
    return;
  }

  const auto diagnostics = s_notification_queue.diagnostics();

  spdlog::debug("[NotifyQueue] enqueue source={} title='{}' queue_size={}", source ? source : "unknown",
                title ? notification_flatten_text(title) : "", diagnostics.depth);
}

static void notification_worker_main()
{
  notification_platform_init();
  s_notification_queue.set_worker_active(true);
  spdlog::debug("[NotifyQueue] worker started");

  for (;;) {
    auto batch = s_notification_queue.wait_for_batch_after_quiet(kNotificationCoalesceWindow);

    if (batch.empty()) {
      if (s_notification_queue.shutdown_requested()) {
        break;
      }
      continue;
    }

    const auto batch_start   = batch.front().queued_at;
    const auto batch_end     = batch.back().queued_at;
    const auto batch_span    = std::chrono::duration_cast<std::chrono::milliseconds>(batch_end - batch_start).count();
    const auto batch_preview = notification_queue_batch_preview(batch, kNotificationSummaryLimit);
    const auto batch_count   = batch.size();

    auto collapsed = notification_queue_collapse_batch(std::move(batch), kNotificationSummaryLimit);
    if (!collapsed.title.empty()) {
      spdlog::debug("[NotifyQueue] flush count={} span_ms={} preview=[{}] collapsed_title='{}' collapsed_body='{}'",
                    batch_count, batch_span, batch_preview, notification_escape_text_for_log(collapsed.title),
                    notification_escape_text_for_log(collapsed.body));
      try {
        notification_platform_show(collapsed.title.c_str(), collapsed.body.c_str());
      } catch (const std::exception& exception) {
        s_notification_queue.record_worker_error();
        spdlog::error("[NotifyQueue] worker error: {}", exception.what());
      } catch (...) {
        s_notification_queue.record_worker_error();
        spdlog::error("[NotifyQueue] worker error: unknown exception");
      }
    }
  }

  s_notification_queue.set_worker_active(false);
  const auto diagnostics = s_notification_queue.diagnostics();
  spdlog::debug("[NotifyQueue] worker stopped dequeued={} errors={}", diagnostics.dequeued, diagnostics.worker_errors);
}
#else
bool notification_should_process_toast(Toast*)
{ return false; }
#endif

// ─── Toast Text Resolution ───────────────────────────────────────────────────────────

/**
 * @brief Resolve the localized body text from a Toast's TextLocaleTextContext.
 *
 * Invokes the cached LanguageManager::Localize method via il2cpp_runtime_invoke
 * to produce a localized string from the toast's LTC data.
 *
 * @param toast The Toast to extract text from.
 * @return Localized text string, or empty on failure.
 */
static std::string resolve_toast_text(Toast* toast)
{
  if (!s_localize_ltc)
    return {};

  auto* ltc = toast->get_TextLocaleTextContext();
  if (!ltc)
    return {};

  auto* langMgr = LanguageManager::Instance();
  if (!langMgr)
    return {};

  Il2CppString*    resolved  = nullptr;
  void*            params[2] = {&resolved, ltc};
  Il2CppException* exc       = nullptr;
  il2cpp_runtime_invoke(s_localize_ltc, langMgr, params, &exc);

  if (exc || !resolved)
    return {};
  return to_string(resolved);
}

static std::string resolve_ltc_param(Il2CppObject* obj)
{
  if (!obj) {
    return {};
  }

  auto* klass = obj->klass;
  if (!klass) {
    return "?";
  }

  auto name = std::string_view(il2cpp_class_get_name(klass));

  if (name == "LocaleTextContext") {
    if (s_localize_ltc) {
      auto* langMgr = LanguageManager::Instance();
      if (langMgr) {
        Il2CppString*    resolved  = nullptr;
        void*            params[2] = {&resolved, obj};
        Il2CppException* exc       = nullptr;
        il2cpp_runtime_invoke(s_localize_ltc, langMgr, params, &exc);
        if (!exc && resolved) {
          return to_string(resolved);
        }
      }
    }

    auto* identifier = read_il2cpp_reference_field<Il2CppString>(obj, "Identifier", "<Identifier>k__BackingField");
    return identifier ? to_string(identifier) : "?";
  }

  if (name == "String") {
    return to_string(reinterpret_cast<Il2CppString*>(obj));
  }

  if (name == "BoxedDouble" || name == "BoxedFloat" || name == "BoxedInt" || name == "BoxedLong") {
    void* iter = nullptr;
    while (auto* field = il2cpp_class_get_fields(klass, &iter)) {
      auto  field_offset = il2cpp_field_get_offset(field);
      auto* field_type   = il2cpp_field_get_type(field);
      if (!field_type) {
        continue;
      }
      auto field_type_id = il2cpp_type_get_type(field_type);

      if (field_type_id == 13) {
        auto value = *reinterpret_cast<double*>(reinterpret_cast<char*>(obj) + field_offset);
        if (value == static_cast<int64_t>(value)) {
          return fmt::format("{}", static_cast<int64_t>(value));
        }
        return fmt::format("{:.1f}", value);
      }

      if (field_type_id == 12) {
        auto value = *reinterpret_cast<float*>(reinterpret_cast<char*>(obj) + field_offset);
        return fmt::format("{:.1f}", value);
      }

      if (field_type_id == 8) {
        auto value = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(obj) + field_offset);
        return fmt::format("{}", value);
      }

      if (field_type_id == 10) {
        auto value = *reinterpret_cast<int64_t*>(reinterpret_cast<char*>(obj) + field_offset);
        return fmt::format("{}", value);
      }
    }
  }

  if (name == "Double") {
    auto value = read_il2cpp_boxed_value<double>(obj);
    if (value == static_cast<int64_t>(value)) {
      return fmt::format("{}", static_cast<int64_t>(value));
    }
    return fmt::format("{:.1f}", value);
  }

  if (name == "Int32") {
    auto value = read_il2cpp_boxed_value<int32_t>(obj);
    return fmt::format("{}", value);
  }

  if (name == "Int64") {
    auto value = read_il2cpp_boxed_value<int64_t>(obj);
    return fmt::format("{}", value);
  }

  if (name == "Single") {
    auto value = read_il2cpp_boxed_value<float>(obj);
    return fmt::format("{:.1f}", value);
  }

  return fmt::format("<{}>", name);
}

static std::string resolve_ltc_formatted(void* ltc, std::string_view template_text)
{
  if (!ltc) {
    return notification_strip_unity_rich_text(template_text);
  }

  auto* ltc_object = reinterpret_cast<Il2CppObject*>(ltc);
  auto* text_parameters =
      read_il2cpp_reference_field<Il2CppArray>(ltc_object, "TextParameters", "<TextParameters>k__BackingField");
  if (!text_parameters) {
    return notification_strip_unity_rich_text(template_text);
  }

  auto  length   = il2cpp_array_length(text_parameters);
  auto* elements = reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(text_parameters) + sizeof(Il2CppArray));

  auto result = std::string(template_text);
  for (il2cpp_array_size_t i = 0; i < length && i < 16; ++i) {
    auto placeholder = fmt::format("{{{}}}", i);
    auto replacement = resolve_ltc_param(elements[i]);

    size_t position = 0;
    while ((position = result.find(placeholder, position)) != std::string::npos) {
      result.replace(position, placeholder.size(), replacement);
      position += replacement.size();
    }
  }

  return notification_strip_unity_rich_text(result);
}

// ─── Public API ──────────────────────────────────────────────────────────────────────

void notification_init()
{
  if (s_notification_initialized) {
    return;
  }

  // Resolve LanguageManager::Localize(out string, LocaleTextContext) — the
  // 2-parameter overload that takes an LTC and returns a localized string.
  auto lm_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Localization", "LanguageManager");
  if (lm_helper.isValidHelper()) {
    auto* cls = lm_helper.get_cls();
    if (cls) {
      void* iter = nullptr;
      while (auto* method = il2cpp_class_get_methods(cls, &iter)) {
        auto name = std::string_view(il2cpp_method_get_name(method));
        auto pc   = il2cpp_method_get_param_count(method);
        if (name == "Localize" && pc == 2) {
          s_localize_ltc = method;
          spdlog::debug("[Notify] Resolved LanguageManager::Localize(out, LTC) at {:p}", (const void*)method);
          break;
        }
      }
    }
  }

  if (!s_localize_ltc) {
    spdlog::warn("[Notify] Could not resolve LanguageManager::Localize — notifications will show titles only");
  }

#if STFCMOD_PLATFORM_WINDOWS
  notification_platform_init();
  std::call_once(s_notification_worker_once,
                 []() { s_notification_worker_thread = std::thread(notification_worker_main); });
  spdlog::debug("[Notify] Windows notification service initialized");
#elif STFCMOD_PLATFORM_MACOS
  if (Config::Get().notifications.enabled) {
    spdlog::warn(
        "[Notify] macOS does not support OS notifications yet; [notifications].notifications_enabled will be ignored");
  } else {
    spdlog::debug("[Notify] Notification service: macOS does not support this feature yet (no-op)");
  }
#else
  spdlog::debug("[Notify] Notification service: platform not supported (no-op)");
#endif

  notification_audio_init();

  s_notification_initialized = true;
}

void notification_shutdown()
{
  notification_audio_shutdown();

#if STFCMOD_PLATFORM_WINDOWS
  s_notification_queue.request_shutdown();

  if (!s_notification_worker_thread.joinable()) {
    return;
  }

  const auto join_started_at = std::chrono::steady_clock::now();
  s_notification_worker_thread.join();
  const auto join_elapsed = std::chrono::steady_clock::now() - join_started_at;
  if (join_elapsed > kNotificationJoinWarnThreshold) {
    spdlog::warn("[NotifyQueue] worker join waited {} ms during shutdown",
                 std::chrono::duration_cast<std::chrono::milliseconds>(join_elapsed).count());
  }
#endif
}

void notification_show(const char* title, const char* body)
{
#if STFCMOD_PLATFORM_WINDOWS
  if (!Config::Get().notifications.enabled) {
    return;
  }

  queue_system_notification(title, body, "direct");
#endif
}

bool notification_delivery_enabled(NotificationKind kind)
{
  const auto& notifications = Config::Get().notifications;
  return (notifications.enabled && notification_policy_system_enabled(kind))
         || (notifications.audio_enabled && notification_policy_audio_enabled(kind));
}

void notification_emit(NotificationKind kind, const char* title, const char* body)
{
  const auto& notifications = Config::Get().notifications;
  const auto& policy        = notification_policy_for(kind);

#if STFCMOD_PLATFORM_WINDOWS
  if (notifications.enabled && policy.system) {
    queue_system_notification(title, body, notification_kind_name(kind));
  }
#endif

  if (notifications.audio_enabled && policy.audio && policy.sound != NotificationSound::None) {
    notification_audio_play(policy.sound, notification_kind_name(kind));
  }
}

void notification_handle_generic_toast(Toast* toast, int state, const char* title)
{
#if STFCMOD_PLATFORM_MACOS
  return; // No notification delivery on macOS yet
#elif STFCMOD_PLATFORM_OTHER
  return; // No notification delivery on unsupported non-Windows platforms yet
#else
  const auto kind = notification_kind_from_toast_state(state);
  if (!kind.has_value() || !notification_delivery_enabled(kind.value())) {
    return;
  }

  if (!title) {
    if (AdvancedDiagnosticsSettings().notification_skip_logging) {
      spdlog::debug("[Notify] No title mapping for toast state {}, skipping", state);
    }
    return;
  }

  auto* ltc = toast->get_TextLocaleTextContext();

  auto        parsed_body = battle_notify_parse(toast);
  std::string localized_body;
  std::string formatted_localized_body;
  if (parsed_body.empty()) {
    localized_body           = resolve_toast_text(toast);
    formatted_localized_body = resolve_ltc_formatted(ltc, localized_body);
  }
  auto body = notification_choose_body(parsed_body, formatted_localized_body, localized_body);

  spdlog::debug("[Notify] {} — {}", title, body);
  notification_emit(kind.value(), title, body.c_str());
#endif
}
