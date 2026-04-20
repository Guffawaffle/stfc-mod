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

#include "config.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/LanguageManager.h>
#include <prime/Toast.h>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if _WIN32
#include <windows.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#endif

// ─── IL2CPP Method Cache ──────────────────────────────────────────────────────────────

/** Cached LanguageManager::Localize(out string, LocaleTextContext) method pointer. */
static const MethodInfo* s_localize_ltc = nullptr;
static bool              s_notification_initialized = false;

// ─── Toast State → Human-Readable Title ───────────────────────────────────────────────

/**
 * @brief Map a numeric toast state to a notification title string.
 * @param state The Toast::State enum value.
 * @return Static title string, or nullptr for unmapped states.
 */
static const char* toast_state_title(int state)
{
  switch (state) {
    case Victory:                   return "Victory!";
    case Defeat:                    return "Defeat";
    case PartialVictory:            return "Partial Victory";
    case StationVictory:            return "Station Victory!";
    case StationDefeat:             return "Station Defeat";
    case StationBattle:             return "Station Under Attack!";
    case IncomingAttack:            return "Incoming Attack!";
    case IncomingAttackFaction:     return "Incoming Faction Attack!";
    case FleetBattle:               return "Fleet Battle";
    case ArmadaBattleWon:           return "Armada Victory!";
    case ArmadaBattleLost:          return "Armada Defeated";
    case ArmadaCreated:             return "Armada Created";
    case ArmadaCanceled:            return "Armada Canceled";
    case ArmadaIncomingAttack:      return "Armada Under Attack!";
    case AssaultVictory:            return "Assault Victory!";
    case AssaultDefeat:             return "Assault Defeat";
    case Tournament:                return "Event Progress";
    case ChainedEventScored:        return "Event Progress";
    case Achievement:               return "Achievement";
    case ChallengeComplete:         return "Challenge Complete";
    case ChallengeFailed:           return "Challenge Failed";
    case TakeoverVictory:           return "Takeover Victory!";
    case TakeoverDefeat:            return "Takeover Defeat";
    case TreasuryProgress:          return "Treasury Progress";
    case TreasuryFull:              return "Treasury Full";
    case WarchestProgress:          return "Warchest Progress";
    case WarchestFull:              return "Warchest Full";
    case FactionLevelUp:            return "Faction Level Up";
    case FactionLevelDown:          return "Faction Level Down";
    case FactionDiscovered:         return "Faction Discovered";
    case FactionWarning:            return "Faction Warning";
    case DiplomacyUpdated:          return "Diplomacy Updated";
    case StrikeHit:                 return "Strike Hit";
    case StrikeDefeat:              return "Strike Defeat";
    case SurgeWarmUpEnded:          return "Surge Started";
    case SurgeHostileGroupDefeated: return "Surge Hostiles Defeated";
    case SurgeTimeLeft:             return "Surge Time Warning";
    case ArenaTimeLeft:             return "Arena Time Warning";
    case FleetPresetApplied:        return "Fleet Preset Applied";
    default:                        return nullptr;
  }
}

// ─── Platform Notification Delivery ──────────────────────────────────────────────────
#if _WIN32
struct NotificationRequest {
  std::string source;
  std::string title;
  std::string body;
  std::chrono::steady_clock::time_point queued_at;
};

static std::mutex              s_notification_queue_mutex;
static std::condition_variable s_notification_queue_condition;
static std::deque<NotificationRequest> s_notification_queue;
static std::once_flag          s_notification_worker_once;
static constexpr auto          kNotificationCoalesceWindow   = std::chrono::milliseconds(750);
static constexpr size_t        kNotificationSummaryLimit     = 4;
static std::string normalize_notification_body(const char* body)
{
  if (!body || !*body) {
    return {};
  }

  std::string normalized;
  normalized.reserve(std::string_view(body).size());

  for (size_t i = 0; body[i] != '\0'; ++i) {
    if (body[i] == '\r') {
      normalized += '\r';
      if (body[i + 1] == '\n') {
        normalized += '\n';
        ++i;
      }
      continue;
    }

    if (body[i] == '\n') {
      normalized += "\r\n";
      continue;
    }

    normalized += body[i];
  }

  return normalized;
}

static std::string flatten_notification_text(std::string_view text)
{
  std::string flattened;
  flattened.reserve(text.size());

  bool last_was_space = false;
  for (char ch : text) {
    if (ch == '\r' || ch == '\n' || ch == '\t') {
      ch = ' ';
    }

    if (ch == ' ') {
      if (flattened.empty() || last_was_space) {
        continue;
      }

      last_was_space = true;
      flattened += ch;
      continue;
    }

    last_was_space = false;
    flattened += ch;
  }

  if (!flattened.empty() && flattened.back() == ' ') {
    flattened.pop_back();
  }

  return flattened;
}

static std::string escape_notification_text_for_log(std::string_view text)
{
  std::string escaped;
  escaped.reserve(text.size());

  for (char ch : text) {
    switch (ch) {
      case '\r':
        escaped += "\\r";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }

  return escaped;
}

static NotificationRequest collapse_notification_batch(std::vector<NotificationRequest>&& batch)
{
  if (batch.empty()) {
    return {};
  }

  if (batch.size() == 1) {
    return std::move(batch.front());
  }

  bool same_title = true;
  for (size_t i = 1; i < batch.size(); ++i) {
    if (batch[i].title != batch.front().title) {
      same_title = false;
      break;
    }
  }

  NotificationRequest collapsed;
  if (same_title) {
    collapsed.title = batch.front().title + " (" + std::to_string(batch.size()) + ")";
  } else {
    collapsed.title = std::to_string(batch.size()) + " Notifications";
  }

  size_t appended = 0;
  for (size_t i = 0; i < batch.size() && appended < kNotificationSummaryLimit; ++i) {
    auto title = flatten_notification_text(batch[i].title);
    auto body  = flatten_notification_text(batch[i].body);

    std::string line;
    if (same_title) {
      line = body.empty() ? title : body;
    } else if (body.empty()) {
      line = title;
    } else {
      line = title + ": " + body;
    }

    if (line.empty()) {
      continue;
    }

    if (!collapsed.body.empty()) {
      collapsed.body += "\n";
    }
    collapsed.body += line;
    ++appended;
  }

  if (batch.size() > appended) {
    if (!collapsed.body.empty()) {
      collapsed.body += "\n";
    }
    collapsed.body += "+" + std::to_string(batch.size() - appended) + " more";
  }

  return collapsed;
}

static std::string notification_batch_preview(const std::vector<NotificationRequest>& batch)
{
  std::string preview;
  size_t appended = 0;

  for (const auto& item : batch) {
    if (appended >= kNotificationSummaryLimit) {
      break;
    }

    auto title = flatten_notification_text(item.title);
    if (title.empty()) {
      title = "(untitled)";
    }

    if (!preview.empty()) {
      preview += ", ";
    }
    preview += item.source + ":" + title;
    ++appended;
  }

  if (batch.size() > appended) {
    preview += ", +" + std::to_string(batch.size() - appended) + " more";
  }

  return preview;
}
static void show_system_notification(const char* title, const char* body)
{
  try {
    using namespace winrt::Windows::UI::Notifications;
    using namespace winrt::Windows::Data::Xml::Dom;

    auto normalizedBody = normalize_notification_body(body);
    spdlog::debug("[NotifyQueue] show title='{}' body='{}'",
                  title ? escape_notification_text_for_log(title) : "",
                  escape_notification_text_for_log(normalizedBody));
    auto xml = ToastNotificationManager::GetTemplateContent(normalizedBody.empty() ? ToastTemplateType::ToastText01
                                                                                   : ToastTemplateType::ToastText02);
    auto nodes = xml.GetElementsByTagName(L"text");
    nodes.Item(0).InnerText(winrt::to_hstring(title));
    if (!normalizedBody.empty()) {
      nodes.Item(1).InnerText(winrt::to_hstring(normalizedBody));
    }

    auto notification = ToastNotification(xml);
    auto notifier     = ToastNotificationManager::CreateToastNotifier(L"Star Trek Fleet Command");
    notifier.Show(notification);
  } catch (const winrt::hresult_error& e) {
    spdlog::warn("[Notify] WinRT notification failed: {}", winrt::to_string(e.message()));
  } catch (...) {
    spdlog::warn("[Notify] WinRT notification failed (unknown error)");
  }
}

static void queue_system_notification(const char* title, const char* body, const char* source)
{
  NotificationRequest request;
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

  size_t queue_size = 0;
  {
    std::lock_guard lock(s_notification_queue_mutex);
    s_notification_queue.emplace_back(std::move(request));
    queue_size = s_notification_queue.size();
  }

  spdlog::debug("[NotifyQueue] enqueue source={} title='{}' queue_size={}",
                source ? source : "unknown",
                title ? flatten_notification_text(title) : "",
                queue_size);

  s_notification_queue_condition.notify_one();
}

static void notification_worker_main()
{
  try { winrt::init_apartment(); } catch (...) {}

  for (;;) {
    std::vector<NotificationRequest> batch;

    {
      std::unique_lock lock(s_notification_queue_mutex);
      s_notification_queue_condition.wait(lock, []() { return !s_notification_queue.empty(); });

      auto observed_size = s_notification_queue.size();
      while (s_notification_queue_condition.wait_for(lock, kNotificationCoalesceWindow, [&] {
        return s_notification_queue.size() != observed_size;
      })) {
        observed_size = s_notification_queue.size();
      }

      while (!s_notification_queue.empty()) {
        batch.emplace_back(std::move(s_notification_queue.front()));
        s_notification_queue.pop_front();
      }
    }

    if (batch.empty()) {
      continue;
    }

    const auto batch_start = batch.front().queued_at;
    const auto batch_end   = batch.back().queued_at;
    const auto batch_span  = std::chrono::duration_cast<std::chrono::milliseconds>(batch_end - batch_start).count();
    const auto batch_preview = notification_batch_preview(batch);
    const auto batch_count = batch.size();

    auto collapsed = collapse_notification_batch(std::move(batch));
    if (!collapsed.title.empty()) {
      spdlog::debug("[NotifyQueue] flush count={} span_ms={} preview=[{}] collapsed_title='{}' collapsed_body='{}'",
                    batch_count,
                    batch_span,
                    batch_preview,
                    escape_notification_text_for_log(collapsed.title),
                    escape_notification_text_for_log(collapsed.body));
      show_system_notification(collapsed.title.c_str(), collapsed.body.c_str());
    }
  }
}
#endif

// ─── Toast Text Resolution ───────────────────────────────────────────────────────────

static std::string strip_unity_rich_text(const std::string& s);

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
  if (!s_localize_ltc) return {};

  auto* ltc = toast->get_TextLocaleTextContext();
  if (!ltc) return {};

  auto* langMgr = LanguageManager::Instance();
  if (!langMgr) return {};

  Il2CppString*  resolved = nullptr;
  void*          params[2] = { &resolved, ltc };
  Il2CppException* exc = nullptr;
  il2cpp_runtime_invoke(s_localize_ltc, langMgr, params, &exc);

  if (exc || !resolved) return {};
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
        Il2CppString*  resolved = nullptr;
        void*          params[2] = { &resolved, obj };
        Il2CppException* exc = nullptr;
        il2cpp_runtime_invoke(s_localize_ltc, langMgr, params, &exc);
        if (!exc && resolved) {
          return to_string(resolved);
        }
      }
    }

    auto* identifier = *reinterpret_cast<Il2CppString**>(reinterpret_cast<char*>(obj) + 16);
    return identifier ? to_string(identifier) : "?";
  }

  if (name == "String") {
    return to_string(reinterpret_cast<Il2CppString*>(obj));
  }

  if (name == "BoxedDouble" || name == "BoxedFloat" || name == "BoxedInt" || name == "BoxedLong") {
    void* iter = nullptr;
    while (auto* field = il2cpp_class_get_fields(klass, &iter)) {
      auto field_offset = il2cpp_field_get_offset(field);
      auto* field_type  = il2cpp_field_get_type(field);
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
    auto value = *reinterpret_cast<double*>(reinterpret_cast<char*>(obj) + 0x10);
    if (value == static_cast<int64_t>(value)) {
      return fmt::format("{}", static_cast<int64_t>(value));
    }
    return fmt::format("{:.1f}", value);
  }

  if (name == "Int32") {
    auto value = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(obj) + 0x10);
    return fmt::format("{}", value);
  }

  if (name == "Int64") {
    auto value = *reinterpret_cast<int64_t*>(reinterpret_cast<char*>(obj) + 0x10);
    return fmt::format("{}", value);
  }

  if (name == "Single") {
    auto value = *reinterpret_cast<float*>(reinterpret_cast<char*>(obj) + 0x10);
    return fmt::format("{:.1f}", value);
  }

  return fmt::format("<{}>", name);
}

static std::string resolve_ltc_formatted(void* ltc, std::string_view template_text)
{
  if (!ltc) {
    return strip_unity_rich_text(std::string(template_text));
  }

  auto* ltc_object = reinterpret_cast<Il2CppObject*>(ltc);
  auto* text_parameters = *reinterpret_cast<Il2CppArray**>(reinterpret_cast<char*>(ltc_object) + 64);
  if (!text_parameters) {
    return strip_unity_rich_text(std::string(template_text));
  }

  auto length = il2cpp_array_length(text_parameters);
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

  return strip_unity_rich_text(result);
}

/**
 * @brief Strip Unity rich text tags (e.g. \<color=#FF0000\>, \<b\>, \</size\>).
 * @param s Input string potentially containing Unity markup.
 * @return Clean string with all angle-bracket tags removed.
 */
static std::string strip_unity_rich_text(const std::string& s)
{
  std::string result;
  result.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '<') {
      auto end = s.find('>', i);
      if (end != std::string::npos) { i = end + 1; continue; }
    }
    result += s[i++];
  }
  return result;
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

#if _WIN32
  try { winrt::init_apartment(); } catch (...) {}
  std::call_once(s_notification_worker_once, []() {
    std::thread(notification_worker_main).detach();
  });
  spdlog::debug("[Notify] Windows notification service initialized");
#else
  spdlog::debug("[Notify] Notification service: platform not supported (no-op)");
#endif

  s_notification_initialized = true;
}

void notification_show(const char* title, const char* body)
{
#if _WIN32
  if (!Config::Get().notifications.enabled) {
    return;
  }

  queue_system_notification(title, body, "direct");
#endif
}

void notification_handle_toast(Toast* toast)
{
#if !_WIN32
  return; // No notification delivery on non-Windows platforms yet
#else
  const auto& notifications = Config::Get().notifications;
  if (!notifications.enabled) {
    return;
  }

  auto state = toast->get_State();

  if (!notifications.EnabledForToastState(state)) {
    return;
  }

  auto* title = toast_state_title(state);
  if (!title) {
    spdlog::debug("[Notify] No title mapping for toast state {}, skipping", state);
    return;
  }

  auto* ltc = toast->get_TextLocaleTextContext();

  auto parsed_body = battle_notify_parse(toast);
  std::string localized_body;
  auto body = parsed_body;
  if (body.empty()) {
    localized_body = resolve_toast_text(toast);
    body = resolve_ltc_formatted(ltc, localized_body);
  }
  if (body.empty()) {
    body = strip_unity_rich_text(localized_body);
  }
  if (body.empty()) {
    body = "(no details available)";
  }

  spdlog::debug("[Notify] {} — {}", title, body);
  queue_system_notification(title, body.c_str(), "toast");
#endif
}
