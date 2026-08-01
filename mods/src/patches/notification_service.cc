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
#include "patches/game_localization.h"

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
#include <prime/EventModel.h>
#include <prime/Toast.h>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <exception>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// ─── IL2CPP Method Cache ──────────────────────────────────────────────────────────────

/** Cached LanguageManager::Localize(out string, LocaleTextContext) method pointer. */
static const MethodInfo* s_localize_ltc             = nullptr;
static bool              s_notification_initialized = false;

static FieldInfo* find_il2cpp_field(Il2CppClass* klass, std::initializer_list<const char*> candidate_names,
                                    bool warn_on_missing = true)
{
  if (!klass) {
    return nullptr;
  }

  const char* first_name = nullptr;
  for (const auto* name : candidate_names) {
    if (!name || !*name) {
      continue;
    }

    if (!first_name) {
      first_name = name;
    }

    if (auto* field = il2cpp_class_get_field_from_name(klass, name)) {
      return field;
    }
  }

  if (warn_on_missing) {
    spdlog::warn("[Notify] Could not resolve {}.{} field",
                 il2cpp_class_get_name(klass) ? il2cpp_class_get_name(klass) : "<unknown>",
                 first_name ? first_name : "<unknown>");
  }

  return nullptr;
}

template <typename T>
static T* read_il2cpp_reference_field(Il2CppObject* obj, std::initializer_list<const char*> candidate_names,
                                      bool warn_on_missing = true)
{
  if (!obj || !obj->klass) {
    return nullptr;
  }

  auto* field = find_il2cpp_field(obj->klass, candidate_names, warn_on_missing);
  if (!field) {
    return nullptr;
  }

  return *reinterpret_cast<T**>(reinterpret_cast<char*>(obj) + il2cpp_field_get_offset(field));
}

template <typename T> static T read_il2cpp_boxed_value(Il2CppObject* obj)
{ return *reinterpret_cast<T*>(reinterpret_cast<char*>(obj) + sizeof(Il2CppObject)); }

static std::string read_il2cpp_string_field(Il2CppObject* obj, std::initializer_list<const char*> candidate_names,
                                            bool warn_on_missing = true)
{
  if (auto* value = read_il2cpp_reference_field<Il2CppString>(obj, candidate_names, warn_on_missing)) {
    return to_string(value);
  }
  return {};
}

static std::optional<int32_t> read_il2cpp_i32_field(Il2CppObject*                      obj,
                                                    std::initializer_list<const char*> candidate_names)
{
  if (!obj || !obj->klass) {
    return std::nullopt;
  }

  auto* field = find_il2cpp_field(obj->klass, candidate_names, false);
  if (!field) {
    return std::nullopt;
  }

  return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(obj) + il2cpp_field_get_offset(field));
}

static bool ascii_starts_with(std::string_view text, std::string_view prefix)
{ return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix; }

static std::optional<int32_t> parse_trailing_level_token(std::string_view text)
{
  const auto separator = text.rfind('_');
  if (separator == std::string_view::npos || separator + 1 >= text.size()) {
    return std::nullopt;
  }

  int32_t level = 0;
  for (size_t i = separator + 1; i < text.size(); ++i) {
    const auto ch = text[i];
    if (ch < '0' || ch > '9') {
      return std::nullopt;
    }
    level = (level * 10) + static_cast<int32_t>(ch - '0');
  }

  return level;
}

static std::string int_to_string_or_empty(std::optional<int32_t> value)
{ return value ? fmt::format("{}", *value) : std::string{}; }

static std::string armada_target_label(Il2CppObject* target_user, Il2CppObject* target_hull,
                                       Il2CppObject* target_alliance, std::string_view target_user_id)
{
  if (auto name = read_il2cpp_string_field(target_user, {"name_"}, false); !name.empty()) {
    return name;
  }

  if (auto name = read_il2cpp_string_field(target_hull, {"name_"}, false); !name.empty()) {
    return name;
  }

  if (auto tag = read_il2cpp_string_field(target_alliance, {"tag_"}, false); !tag.empty()) {
    return tag;
  }

  if (auto name = read_il2cpp_string_field(target_alliance, {"name_"}, false); !name.empty()) {
    return name;
  }

  if (ascii_starts_with(target_user_id, "mar_")) {
    return "Armada Target";
  }

  if (!target_user_id.empty()) {
    return std::string(target_user_id);
  }

  return "Armada Target";
}

static std::string resolve_armada_created_formatted(Toast* toast, int state, std::string_view template_text)
{
  if (state != ArmadaCreated || !toast || template_text.empty()) {
    return {};
  }

  auto* data = toast->get_Data();
  if (!data) {
    return {};
  }

  auto* armada_attack = read_il2cpp_reference_field<Il2CppObject>(data, {"ArmadaAttack"});
  auto* alliance      = read_il2cpp_reference_field<Il2CppObject>(data, {"Alliance"});
  if (!armada_attack) {
    return {};
  }

  auto* owner       = read_il2cpp_reference_field<Il2CppObject>(armada_attack, {"<Owner>k__BackingField", "Owner"});
  auto* target_user = read_il2cpp_reference_field<Il2CppObject>(armada_attack, {"<TargetUserProfile>k__BackingField"});
  auto* target_alliance =
      read_il2cpp_reference_field<Il2CppObject>(armada_attack, {"<TargetAllianceProfile>k__BackingField"});
  auto* target_hull = read_il2cpp_reference_field<Il2CppObject>(armada_attack, {"<TargetHullSpec>k__BackingField"});
  auto* target_info = read_il2cpp_reference_field<Il2CppObject>(armada_attack, {"target_"});

  auto alliance_tag = read_il2cpp_string_field(alliance, {"tag_"});
  if (alliance_tag.empty()) {
    alliance_tag = read_il2cpp_string_field(alliance, {"name_"});
  }

  auto owner_name = read_il2cpp_string_field(owner, {"name_"});
  if (owner_name.empty()) {
    owner_name = read_il2cpp_string_field(armada_attack, {"ownerId_"});
  }

  auto target_user_id = read_il2cpp_string_field(target_info, {"userId_"});
  auto target_level   = read_il2cpp_i32_field(target_user, {"level_"});
  if (!target_level || *target_level <= 0) {
    target_level = parse_trailing_level_token(target_user_id);
  }

  auto target_label = armada_target_label(target_user, target_hull, target_alliance, target_user_id);
  auto fleet_count  = read_il2cpp_i32_field(armada_attack, {"fleetCapacity_"});

  if (alliance_tag.empty() && owner_name.empty() && target_label.empty()) {
    return {};
  }

  return notification_format_armada_created_body(template_text, "#ffffff", alliance_tag, owner_name,
                                                 int_to_string_or_empty(fleet_count), "#ffffff",
                                                 int_to_string_or_empty(target_level), target_label);
}

static const char* event_category_name(const int32_t category)
{
  switch (category) {
    case 0: return "Standard";
    case 1: return "Daily Goals";
    case 2: return "Daily Milestone";
    case 3: return "Leaderboard";
    case 4: return "Stat";
    case 5: return "Battle Pass Season";
    case 6: return "Battle Pass Event";
    case 7: return "Treasury Progress";
    case 8: return "Treasury Reward";
    case 9: return "Server Clash";
    case 10: return "Webstore Event";
    case 11: return "Player Lifecycle";
    case 12: return "Field Training";
    case 13: return "FT Category";
    case 14: return "Cutscenes";
    case 15: return "Minigame";
    case 16: return "Minigame Stage";
    case 17: return "Warchest";
    case 18: return "Alliance Game";
    case 19: return "Alliance Game Task";
    case 20: return "Meta Event";
    case 21: return "Meta Event Objective";
    case 22: return "Invasion";
    case 23: return "Loop Museum";
    case 24: return "Loop Museum Task";
    case 25: return "PLC BP Season";
    case 26: return "PLC BP Event";
    case 27: return "Progression Reward";
    case 28: return "Faction Weekly Events";
    default: return nullptr;
  }
}

static std::string resolve_event_category(Toast* toast)
{
  auto* event = toast ? reinterpret_cast<EventModel*>(toast->get_Data()) : nullptr;
  if (!event) {
    return {};
  }

  const auto category = event->CategoryValue();
  const auto* name    = category ? event_category_name(*category) : nullptr;
  return name ? std::string{name} : std::string{};
}

static bool toast_state_uses_event_category(const int state)
{
  return state == Achievement || state == Tournament || state == ChainedEventScored || state == TreasuryProgress
         || state == TreasuryFull || state == WarchestProgress || state == WarchestFull
         || state == FactionWeeklyEventsProgress || state == FactionWeeklyEventsComplete;
}

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
    case QueueForLeaseActivated:
      return "Queue Activated";
    case QueueForLeaseExpired:
      return "Queue Expired";
    case PermanentQueuePurchased:
      return "Permanent Queue Purchased";
    case OutpostStartedOrEnded:
      return "Outpost Update";
    case CrossAllianceArmadaVictory:
      return "Cross-Armada Victory!";
    case CrossAllianceArmadaDefeat:
      return "Cross-Armada Defeated";
    case CrossAllianceArmadaPartialVictory:
      return "Cross-Armada Partial Victory";
    case FactionWeeklyEventsProgress:
      return "Faction Weekly Event Progress";
    case FactionWeeklyEventsComplete:
      return "Faction Weekly Event Complete";
    case ArmadaPlayerBlocked:
      return "Armada Player Blocked";
    case ArmadaPlayerUnblocked:
      return "Armada Player Unblocked";
    case DynamicCrisisUpdate:
      return "Dynamic Crisis Update";
    case DynamicCrisisFailed:
      return "Dynamic Crisis Failed";
    case DynamicCrisisCompleted:
      return "Dynamic Crisis Completed";
    case GalacticAnomalySystemEntered:
      return "Galactic Anomaly Entered";
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

static Il2CppArray* resolve_ltc_text_parameters(Il2CppObject* ltc_object)
{
  if (!ltc_object) {
    return nullptr;
  }

  if (auto* text_parameters = read_il2cpp_reference_field<Il2CppArray>(
          ltc_object, {"_textParameters", "TextParameters", "<TextParameters>k__BackingField"}, false)) {
    return text_parameters;
  }

  return read_il2cpp_reference_field<Il2CppArray>(ltc_object, {"_identifierParameters"}, false);
}

static Il2CppArray* resolve_toast_text_parameters(Toast* toast, void* ltc)
{
  if (toast) {
    auto* toast_object = reinterpret_cast<Il2CppObject*>(toast);
    if (auto* text_parameters = read_il2cpp_reference_field<Il2CppArray>(
            toast_object, {"<TextParameters>k__BackingField", "TextParameters", "_textParameters"}, false)) {
      return text_parameters;
    }

    auto* secondary_ltc = read_il2cpp_reference_field<Il2CppObject>(
        toast_object, {"<SecondaryTextLocaleTextContext>k__BackingField", "SecondaryTextLocaleTextContext"}, false);
    if (auto* text_parameters = resolve_ltc_text_parameters(secondary_ltc)) {
      return text_parameters;
    }
  }

  if (ltc) {
    return resolve_ltc_text_parameters(reinterpret_cast<Il2CppObject*>(ltc));
  }

  return nullptr;
}

static std::string resolve_toast_text(void* ltc)
{
  if (auto localized = game_localization::LocalizeContext(ltc); !localized.empty()) {
    return localized;
  }

  if (!ltc) {
    return {};
  }

  auto* langMgr = LanguageManager::Instance();
  if (!langMgr) {
    return {};
  }

  if (!s_localize_ltc) {
    return {};
  }

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

    auto* identifier =
        read_il2cpp_reference_field<Il2CppString>(obj, {"_identifier", "Identifier", "<Identifier>k__BackingField"});
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

static std::vector<std::string> resolve_text_parameter_values(Il2CppArray* text_parameters)
{
  if (!text_parameters) {
    return {};
  }

  auto  length   = il2cpp_array_length(text_parameters);
  auto* elements = reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(text_parameters) + sizeof(Il2CppArray));

  std::vector<std::string> parameters;
  parameters.reserve(length < 32 ? length : 32);
  for (il2cpp_array_size_t i = 0; i < length && i < 32; ++i) {
    parameters.push_back(resolve_ltc_param(elements[i]));
  }

  return parameters;
}

static std::string resolve_toast_formatted(Toast* toast, void* ltc, std::string_view template_text, int state)
{
  if (!ltc) {
    return notification_strip_unity_rich_text(template_text);
  }

  auto parameters = resolve_text_parameter_values(resolve_toast_text_parameters(toast, ltc));
  auto formatted  = notification_strip_unity_rich_text(notification_format_placeholders(template_text, parameters));
  if (notification_contains_placeholders(formatted)) {
    if (auto armada_formatted = resolve_armada_created_formatted(toast, state, template_text);
        !armada_formatted.empty()) {
      formatted = std::move(armada_formatted);
    }
  }
  return formatted;
}

static void resolve_language_manager_localize_methods()
{
  auto lm_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Localization", "LanguageManager");
  if (!lm_helper.isValidHelper()) {
    spdlog::warn("[Notify] LanguageManager class helper is invalid");
    return;
  }

  auto* cls = lm_helper.get_cls();
  if (!cls) {
    spdlog::warn("[Notify] LanguageManager class not found");
    return;
  }

  void* iter = nullptr;
  while (auto* method = il2cpp_class_get_methods(cls, &iter)) {
    auto name = std::string_view(il2cpp_method_get_name(method));
    if (name != "Localize") {
      continue;
    }

    const auto pc = il2cpp_method_get_param_count(method);
    if (pc == 2) {
      s_localize_ltc = method;
      break;
    }
  }
}

// ─── Public API ──────────────────────────────────────────────────────────────────────

void notification_init()
{
  if (s_notification_initialized) {
    return;
  }

  game_localization::Initialize();
  resolve_language_manager_localize_methods();

  if (!s_localize_ltc) {
    spdlog::warn("[Notify] Could not resolve LanguageManager::Localize — notifications will show titles only");
  }

#if STFCMOD_PLATFORM_WINDOWS
  notification_platform_init();
  std::call_once(s_notification_worker_once,
                 []() { s_notification_worker_thread = std::thread(notification_worker_main); });
  spdlog::debug("[Notify] Windows notification service initialized");
#elif STFCMOD_PLATFORM_MACOS
  if (notification_policy_any_system_enabled()) {
    spdlog::warn("[Notify] macOS does not support OS notifications yet; configured system deliveries are unavailable");
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
  queue_system_notification(title, body, "direct");
#endif
}

bool notification_delivery_enabled(NotificationKind kind)
{
#if !STFCMOD_PLATFORM_WINDOWS
  (void)kind;
  return false;
#else
  return notification_policy_has_delivery(kind);
#endif
}

void notification_emit(NotificationKind kind, const char* title, const char* body)
{
  const auto& policy = notification_policy_for(kind);

#if STFCMOD_PLATFORM_WINDOWS
  if (policy.system) {
    queue_system_notification(title, body, notification_kind_name(kind));
  }
#endif

  if (policy.audio && policy.sound != NotificationSound::None) {
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
  const auto armada_kind           = state == Victory  ? std::optional{NotificationKind::BattleArmadaBattleWon}
                                     : state == Defeat ? std::optional{NotificationKind::BattleArmadaBattleLost}
                                                       : std::nullopt;
  const auto armada_policy_enabled = armada_kind.has_value() && notification_delivery_enabled(armada_kind.value());
  const auto is_armada_battle      = armada_policy_enabled && battle_notify_is_armada(toast);
  const auto kind = notification_kind_for_battle_context(state, is_armada_battle, armada_policy_enabled);
  if (!kind.has_value() || !notification_delivery_enabled(kind.value())) {
    return;
  }

  const auto* selected_spec  = notification_catalog_entry(kind.value());
  const auto* selected_title = is_armada_battle && selected_spec
                                   ? notification_toast_title(selected_spec->toast_state)
                                   : title;
  if (!selected_title) {
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
    localized_body           = resolve_toast_text(ltc);
    formatted_localized_body = resolve_toast_formatted(toast, ltc, localized_body, state);
  }
  auto body = notification_choose_body(parsed_body, formatted_localized_body, localized_body);
  if (toast_state_uses_event_category(state)) {
    if (auto category = resolve_event_category(toast); !category.empty()) {
      body = category + " - " + body;
    }
  }

  spdlog::debug("[Notify] {} — {}", selected_title, body);
  notification_emit(kind.value(), selected_title, body.c_str());
#endif
}
