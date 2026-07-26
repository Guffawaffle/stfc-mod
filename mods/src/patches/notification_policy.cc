#include "patches/notification_policy.h"

#include "config.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace
{
constexpr std::string_view kCatalogDefaultSource = "catalog-default";
constexpr std::string_view kEventCatalogSource   = "event-catalog";
constexpr std::string_view kLegacyDefaultSource  = "deprecated-default";
constexpr std::string_view kLegacyRemovalTarget  = "3.0.0";
constexpr size_t           kMaxIgnoredSources    = 8;
constexpr size_t           kMaxDiagnostics       = 8;
constexpr int              kMaxConflictWarnings  = 4;

struct NotificationPolicyResolution {
  NotificationPolicy       policy;
  std::string              source_kind = "catalog-default";
  std::string              system_source{kCatalogDefaultSource};
  std::string              audio_source{kCatalogDefaultSource};
  std::string              sound_source{kEventCatalogSource};
  bool                     deprecated_inputs    = false;
  bool                     conflict             = false;
  int                      diagnostic_count     = 0;
  int                      ignored_source_count = 0;
  std::vector<std::string> ignored_sources;
  std::vector<std::string> diagnostics;
};

struct LegacyMaster {
  bool                     value  = true;
  std::string              source = "hidden-default";
  std::string              ignored_source;
  bool                     present          = false;
  bool                     conflict         = false;
  int                      diagnostic_count = 0;
  std::vector<std::string> diagnostics;
};

std::array<NotificationPolicy, kNotificationKindCount>           s_notification_policy{};
std::array<NotificationPolicyResolution, kNotificationKindCount> s_notification_resolutions{};
std::vector<std::string>                                         s_unknown_root_keys;
int                                                              s_unknown_root_key_count = 0;

std::string normalize_sound_name(std::string_view value)
{
  std::string normalized;
  normalized.reserve(value.size());

  for (const auto ch : value) {
    const auto byte = static_cast<unsigned char>(ch);
    if (std::isspace(byte) || ch == '-') {
      normalized.push_back('_');
    } else {
      normalized.push_back(static_cast<char>(std::tolower(byte)));
    }
  }

  return normalized;
}

const toml::table* nested_table(const toml::table& table, std::initializer_list<std::string_view> path)
{
  const toml::table* current = &table;
  for (const auto segment : path) {
    const auto* node = current->get(segment);
    if (!node) {
      return nullptr;
    }

    current = node->as_table();
    if (!current) {
      return nullptr;
    }
  }

  return current;
}

toml::table* ensure_table(toml::table& table, std::initializer_list<std::string_view> path)
{
  auto* current = &table;
  for (const auto segment_view : path) {
    const auto segment = std::string(segment_view);
    auto*      node    = current->get(segment);
    if (!node || !node->is_table()) {
      current->insert_or_assign(segment, toml::table{});
      node = current->get(segment);
    }

    current = node ? node->as_table() : nullptr;
    if (!current) {
      return nullptr;
    }
  }

  return current;
}

std::string canonical_path(const NotificationEventCatalogEntry& spec)
{ return "notifications." + std::string(spec.canonical_key); }

std::string legacy_event_path(const NotificationEventCatalogEntry& spec)
{ return "notifications.events." + std::string(spec.legacy_category) + "." + std::string(spec.legacy_key); }

std::string legacy_split_path(const NotificationEventCatalogEntry& spec, const std::string_view channel)
{
  return "notifications." + std::string(channel) + "." + std::string(spec.legacy_category) + "."
         + std::string(spec.legacy_key);
}

std::string legacy_flat_path(const NotificationEventCatalogEntry& spec)
{ return "notifications.notifications_" + std::string(spec.canonical_key); }

std::string legacy_flat_audio_path(const NotificationEventCatalogEntry& spec)
{ return "notifications.notifications_audio_" + std::string(spec.canonical_key); }

const toml::node* root_notification_node(const toml::table& config, const std::string_view key)
{
  const auto* notifications = config["notifications"].as_table();
  return notifications ? notifications->get(key) : nullptr;
}

bool is_supported_root_notification_key(const std::string_view key)
{
  if (key == "events" || key == "system" || key == "audio" || key == "provenance" || key == "resolution"
      || key == "notifications_enabled" || key == "notifications_audio_enabled"
      || key == "notifications_incoming_attack" || key == "notifications_incoming_attack_faction") {
    return true;
  }

  return std::ranges::any_of(kNotificationEventCatalog, [key](const auto& spec) {
    return key == spec.canonical_key || key == "notifications_" + std::string(spec.canonical_key)
           || key == "notifications_audio_" + std::string(spec.canonical_key);
  });
}

const toml::node* legacy_event_node(const toml::table& config, const NotificationEventCatalogEntry& spec)
{
  const auto* category = nested_table(config, {"notifications", "events", spec.legacy_category});
  return category ? category->get(spec.legacy_key) : nullptr;
}

const toml::node* legacy_split_node(const toml::table& config, const NotificationEventCatalogEntry& spec,
                                    const std::string_view channel)
{
  const auto* category = nested_table(config, {"notifications", channel, spec.legacy_category});
  return category ? category->get(spec.legacy_key) : nullptr;
}

void add_diagnostic(NotificationPolicyResolution& result, std::string fact)
{
  ++result.diagnostic_count;
  if (result.diagnostics.size() < kMaxDiagnostics
      && std::ranges::find(result.diagnostics, fact) == result.diagnostics.end()) {
    result.diagnostics.push_back(std::move(fact));
  }
}

void add_ignored_source(NotificationPolicyResolution& result, const std::string& source)
{
  ++result.ignored_source_count;
  if (result.ignored_sources.size() < kMaxIgnoredSources
      && std::ranges::find(result.ignored_sources, source) == result.ignored_sources.end()) {
    result.ignored_sources.push_back(source);
  }
}

bool is_default_source(const std::string& source)
{ return source == kCatalogDefaultSource || source == kEventCatalogSource || source == kLegacyDefaultSource; }

void apply_legacy_bool(bool& field, std::string& field_source, const bool value, const std::string& source,
                       NotificationPolicyResolution& result)
{
  if (!is_default_source(field_source)) {
    if (field != value) {
      result.conflict = true;
      add_diagnostic(result, "deprecated-value-conflict:" + field_source + " overridden-by " + source);
    }
    add_ignored_source(result, field_source);
  }

  field        = value;
  field_source = source;
}

void apply_legacy_sound(NotificationSound& field, std::string& field_source, const NotificationSound value,
                        const std::string& source, NotificationPolicyResolution& result)
{
  if (!is_default_source(field_source)) {
    if (field != value) {
      result.conflict = true;
      add_diagnostic(result, "deprecated-value-conflict:" + field_source + " overridden-by " + source);
    }
    add_ignored_source(result, field_source);
  }

  field        = value;
  field_source = source;
}

std::optional<bool> read_bool_node(const toml::node* node, const std::string& path,
                                   NotificationPolicyResolution& result)
{
  if (!node) {
    return std::nullopt;
  }

  const auto value = node->value<bool>();
  if (!value.has_value()) {
    add_diagnostic(result, "invalid-boolean:" + path);
    spdlog::warn("[NotifyPolicy] Invalid boolean {}; ignoring deprecated value", path);
  }
  return value;
}

std::optional<NotificationSound> read_sound_node(const toml::node* node, const std::string& path,
                                                 NotificationPolicyResolution& result)
{
  if (!node) {
    return std::nullopt;
  }

  const auto value = node->value<std::string>();
  if (!value.has_value()) {
    add_diagnostic(result, "invalid-sound-type:" + path);
    spdlog::warn("[NotifyPolicy] Invalid string {}; keeping event catalog sound", path);
    return std::nullopt;
  }

  const auto sound = notification_sound_from_name(*value);
  if (!sound.has_value()) {
    add_diagnostic(result, "unknown-sound:" + path + "=" + *value);
    spdlog::warn("[NotifyPolicy] Unknown notification sound '{}' at {}; keeping event catalog sound", *value, path);
  }
  return sound;
}

bool legacy_system_default(const NotificationConfig& legacy, const NotificationEventCatalogEntry& spec)
{
  switch (spec.kind) {
    case NotificationKind::FleetArrivedInSystem:
      return legacy.fleet_arrived_in_system;
    case NotificationKind::FleetArrivedAtDestination:
      return legacy.fleet_arrived_at_destination;
    case NotificationKind::FleetStartedMining:
      return legacy.fleet_started_mining;
    case NotificationKind::FleetNodeDepleted:
      return legacy.fleet_node_depleted;
    case NotificationKind::FleetDocked:
      return legacy.fleet_docked;
    case NotificationKind::FleetRepairComplete:
      return legacy.fleet_repair_complete;
    default:
      return spec.toast_state >= 0 ? legacy.EnabledForToastState(spec.toast_state) : false;
  }
}

bool legacy_audio_default(const NotificationConfig& legacy, const NotificationEventCatalogEntry& spec)
{ return spec.kind == NotificationKind::FleetArrivedInSystem ? legacy.audio_fleet_arrived_in_system : false; }

LegacyMaster resolve_legacy_master(const toml::table& config, const std::string_view channel,
                                   const std::string_view flat_key)
{
  LegacyMaster                 result;
  NotificationPolicyResolution scratch;

  const auto flat_path = "notifications." + std::string(flat_key);
  if (const auto* node = root_notification_node(config, flat_key); node) {
    result.present = true;
    result.source  = flat_path;
    if (const auto value = read_bool_node(node, flat_path, scratch); value.has_value()) {
      result.value = *value;
    }
  }

  const auto split_path = "notifications." + std::string(channel) + ".enabled";
  if (const auto* channel_table = nested_table(config, {"notifications", channel}); channel_table) {
    if (const auto* node = channel_table->get("enabled"); node) {
      result.present = true;
      if (const auto value = read_bool_node(node, split_path, scratch); value.has_value()) {
        if (result.source != "hidden-default") {
          result.ignored_source = result.source;
        }
        if (!result.ignored_source.empty() && result.value != *value) {
          result.conflict = true;
          add_diagnostic(scratch, "deprecated-master-conflict:" + split_path + " overrides " + result.source);
          spdlog::warn("[NotifyPolicy] Deprecated master conflict: {} overrides {}; removal target {}", split_path,
                       result.source, kLegacyRemovalTarget);
        }
        result.value  = *value;
        result.source = split_path;
      } else {
        result.source = split_path;
      }
    }
  }

  result.diagnostic_count = scratch.diagnostic_count;
  result.diagnostics      = std::move(scratch.diagnostics);
  return result;
}

void apply_legacy_master(bool& field, std::string& field_source, const LegacyMaster& master,
                         NotificationPolicyResolution& result)
{
  if (!master.present) {
    return;
  }

  result.deprecated_inputs = true;
  result.conflict          = result.conflict || master.conflict;
  for (const auto& diagnostic : master.diagnostics) {
    add_diagnostic(result, diagnostic);
  }
  result.diagnostic_count += std::max(0, master.diagnostic_count - static_cast<int>(master.diagnostics.size()));
  if (!master.ignored_source.empty()) {
    add_ignored_source(result, master.ignored_source);
  }

  if (!master.value) {
    field = false;
  }
  field_source.append(" gated-by ");
  field_source.append(master.source);
  field_source.append(master.value ? "=true" : "=false");
}

std::vector<std::string> legacy_sources_for_event(const toml::table& config, const NotificationEventCatalogEntry& spec)
{
  std::vector<std::string> sources;
  auto                     add_if = [&sources](const toml::node* node, std::string path) {
    if (node) {
      sources.push_back(std::move(path));
    }
  };

  add_if(legacy_event_node(config, spec), legacy_event_path(spec));
  add_if(legacy_split_node(config, spec, "system"), legacy_split_path(spec, "system"));
  add_if(legacy_split_node(config, spec, "audio"), legacy_split_path(spec, "audio"));
  add_if(root_notification_node(config, "notifications_" + std::string(spec.canonical_key)), legacy_flat_path(spec));
  add_if(root_notification_node(config, "notifications_audio_" + std::string(spec.canonical_key)),
         legacy_flat_audio_path(spec));

  if (spec.kind == NotificationKind::BattleIncomingAttackPlayer) {
    add_if(root_notification_node(config, "notifications_incoming_attack"),
           "notifications.notifications_incoming_attack");
  } else if (spec.kind == NotificationKind::BattleIncomingAttackHostile) {
    add_if(root_notification_node(config, "notifications_incoming_attack_faction"),
           "notifications.notifications_incoming_attack_faction");
  }

  if (spec.toast_state >= 0) {
    if (const auto* ui = config["ui"].as_table(); ui) {
      add_if(ui->get("notify_on_banner_types"), "ui.notify_on_banner_types");
      add_if(ui->get("notify_banner_types"), "ui.notify_banner_types");
    }
  }

  add_if(root_notification_node(config, "notifications_enabled"), "notifications.notifications_enabled");
  add_if(root_notification_node(config, "notifications_audio_enabled"), "notifications.notifications_audio_enabled");
  if (const auto* system = nested_table(config, {"notifications", "system"}); system) {
    add_if(system->get("enabled"), "notifications.system.enabled");
  }
  if (const auto* audio = nested_table(config, {"notifications", "audio"}); audio) {
    add_if(audio->get("enabled"), "notifications.audio.enabled");
    if (spec.kind == NotificationKind::ExperimentalStandard) {
      add_if(audio->get("default_sound"), "notifications.audio.default_sound");
    }
  }

  return sources;
}

NotificationPolicyResolution resolve_legacy_policy(const toml::table& config, const NotificationConfig& legacy,
                                                   const NotificationEventCatalogEntry& spec,
                                                   const bool has_any_legacy_input, const LegacyMaster& system_master,
                                                   const LegacyMaster& audio_master)
{
  NotificationPolicyResolution result;
  result.policy.sound = spec.catalog_sound;
  if (!has_any_legacy_input) {
    return result;
  }

  result.policy.system     = legacy_system_default(legacy, spec);
  result.policy.audio      = legacy_audio_default(legacy, spec);
  result.system_source     = std::string(kLegacyDefaultSource);
  result.audio_source      = std::string(kLegacyDefaultSource);
  result.sound_source      = std::string(kEventCatalogSource);
  result.source_kind       = "deprecated";
  result.deprecated_inputs = true;

  if (spec.kind == NotificationKind::ExperimentalStandard) {
    if (const auto* audio = nested_table(config, {"notifications", "audio"}); audio) {
      if (const auto* node = audio->get("default_sound"); node) {
        if (const auto sound = read_sound_node(node, "notifications.audio.default_sound", result); sound.has_value()) {
          apply_legacy_sound(result.policy.sound, result.sound_source, *sound, "notifications.audio.default_sound",
                             result);
        }
      }
    }
  }

  if (const auto* ui = config["ui"].as_table(); ui && spec.toast_state >= 0) {
    const auto* allowlist = ui->get("notify_on_banner_types");
    auto        source    = std::string("ui.notify_on_banner_types");
    if (!allowlist) {
      allowlist = ui->get("notify_banner_types");
      source    = "ui.notify_banner_types";
    }
    if (allowlist) {
      apply_legacy_bool(result.policy.system, result.system_source, legacy.EnabledForToastState(spec.toast_state),
                        source, result);
    }
  }

  if (spec.kind == NotificationKind::BattleIncomingAttackPlayer) {
    const auto path = std::string("notifications.notifications_incoming_attack");
    if (const auto* node = root_notification_node(config, "notifications_incoming_attack"); node) {
      if (const auto value = read_bool_node(node, path, result); value.has_value()) {
        apply_legacy_bool(result.policy.system, result.system_source, *value, path, result);
      }
    }
  } else if (spec.kind == NotificationKind::BattleIncomingAttackHostile) {
    const auto path = std::string("notifications.notifications_incoming_attack_faction");
    if (const auto* node = root_notification_node(config, "notifications_incoming_attack_faction"); node) {
      if (const auto value = read_bool_node(node, path, result); value.has_value()) {
        apply_legacy_bool(result.policy.system, result.system_source, *value, path, result);
      }
    }
  }

  const auto flat_path = legacy_flat_path(spec);
  if (const auto* node = root_notification_node(config, "notifications_" + std::string(spec.canonical_key)); node) {
    if (const auto value = read_bool_node(node, flat_path, result); value.has_value()) {
      apply_legacy_bool(result.policy.system, result.system_source, *value, flat_path, result);
    }
  }

  const auto flat_audio_path = legacy_flat_audio_path(spec);
  if (const auto* node = root_notification_node(config, "notifications_audio_" + std::string(spec.canonical_key));
      node) {
    if (const auto value = read_bool_node(node, flat_audio_path, result); value.has_value()) {
      apply_legacy_bool(result.policy.audio, result.audio_source, *value, flat_audio_path, result);
    }
  }

  const auto split_system_path = legacy_split_path(spec, "system");
  if (const auto* node = legacy_split_node(config, spec, "system"); node) {
    if (const auto value = read_bool_node(node, split_system_path, result); value.has_value()) {
      apply_legacy_bool(result.policy.system, result.system_source, *value, split_system_path, result);
    }
  }

  const auto split_audio_path = legacy_split_path(spec, "audio");
  if (const auto* node = legacy_split_node(config, spec, "audio"); node) {
    if (const auto value = read_bool_node(node, split_audio_path, result); value.has_value()) {
      apply_legacy_bool(result.policy.audio, result.audio_source, *value, split_audio_path, result);
    }
  }

  const auto event_path = legacy_event_path(spec);
  if (const auto* node = legacy_event_node(config, spec); node) {
    const auto* table = node->as_table();
    if (!table) {
      add_diagnostic(result, "invalid-deprecated-event-policy:" + event_path);
      spdlog::warn("[NotifyPolicy] Expected inline table at {}; keeping lower-precedence deprecated policy",
                   event_path);
    } else {
      if (const auto* field = table->get("system"); field) {
        if (const auto value = read_bool_node(field, event_path + ".system", result); value.has_value()) {
          apply_legacy_bool(result.policy.system, result.system_source, *value, event_path + ".system", result);
        }
      }
      if (const auto* field = table->get("audio"); field) {
        if (const auto value = read_bool_node(field, event_path + ".audio", result); value.has_value()) {
          apply_legacy_bool(result.policy.audio, result.audio_source, *value, event_path + ".audio", result);
        }
      }
      if (const auto* field = table->get("sound"); field) {
        if (const auto value = read_sound_node(field, event_path + ".sound", result); value.has_value()) {
          apply_legacy_sound(result.policy.sound, result.sound_source, *value, event_path + ".sound", result);
        }
      }
    }
  }

  apply_legacy_master(result.policy.system, result.system_source, system_master, result);
  apply_legacy_master(result.policy.audio, result.audio_source, audio_master, result);
  result.source_kind = result.conflict ? "deprecated-conflict" : "deprecated";
  return result;
}

NotificationPolicyResolution resolve_canonical_policy(const toml::node& node, const NotificationEventCatalogEntry& spec)
{
  NotificationPolicyResolution result;
  result.policy.sound = spec.catalog_sound;
  result.source_kind  = "canonical";

  const auto path = canonical_path(spec);
  if (const auto value = node.value<bool>(); value.has_value()) {
    result.policy.system = *value;
    result.policy.audio  = false;
    result.system_source = path;
    result.audio_source  = path;
    result.sound_source  = std::string(kEventCatalogSource);
    return result;
  }

  const auto* table = node.as_table();
  if (!table) {
    result.source_kind = "invalid-canonical-fallback";
    add_diagnostic(result, "invalid-canonical-policy:" + path);
    spdlog::warn("[NotifyPolicy] Invalid canonical value at {}; expected bool or inline table. Using event catalog "
                 "default.",
                 path);
    return result;
  }

  bool invalid_field = false;
  for (const auto& [field_key, field_node] : *table) {
    const auto field = field_key.str();
    if (field != "system" && field != "audio" && field != "sound") {
      invalid_field = true;
      add_diagnostic(result, "unknown-canonical-field:" + path + "." + std::string(field));
      spdlog::warn("[NotifyPolicy] Unknown canonical field {}.{}; ignoring it", path, field);
    }
    (void)field_node;
  }

  if (const auto* field = table->get("system"); field) {
    if (const auto value = field->value<bool>(); value.has_value()) {
      result.policy.system = *value;
      result.system_source = path + ".system";
    } else {
      invalid_field = true;
      add_diagnostic(result, "invalid-canonical-boolean:" + path + ".system");
      spdlog::warn("[NotifyPolicy] Invalid canonical boolean {}.system; using event catalog default", path);
    }
  }

  if (const auto* field = table->get("audio"); field) {
    if (const auto value = field->value<bool>(); value.has_value()) {
      result.policy.audio = *value;
      result.audio_source = path + ".audio";
    } else {
      invalid_field = true;
      add_diagnostic(result, "invalid-canonical-boolean:" + path + ".audio");
      spdlog::warn("[NotifyPolicy] Invalid canonical boolean {}.audio; using event catalog default", path);
    }
  }

  if (const auto* field = table->get("sound"); field) {
    const auto value = field->value<std::string>();
    const auto sound = value.has_value() ? notification_sound_from_name(*value) : std::nullopt;
    if (sound.has_value()) {
      result.policy.sound = *sound;
      result.sound_source = path + ".sound";
    } else {
      invalid_field = true;
      if (value.has_value()) {
        add_diagnostic(result, "unknown-canonical-sound:" + path + ".sound=" + *value);
        spdlog::warn("[NotifyPolicy] Unknown canonical sound '{}' at {}.sound; using event catalog sound", *value,
                     path);
      } else {
        add_diagnostic(result, "invalid-canonical-sound-type:" + path + ".sound");
        spdlog::warn("[NotifyPolicy] Invalid canonical string {}.sound; using event catalog sound", path);
      }
    }
  } else if (result.policy.audio) {
    invalid_field = true;
    add_diagnostic(result, "missing-canonical-sound:" + path + ".sound");
    spdlog::warn("[NotifyPolicy] Canonical {} enables audio without a sound; using event catalog sound '{}'", path,
                 notification_sound_name(spec.catalog_sound));
  }

  result.source_kind = invalid_field ? "invalid-canonical-fallback" : "canonical";
  return result;
}

bool policies_equal(const NotificationPolicy& left, const NotificationPolicy& right)
{ return left.system == right.system && left.audio == right.audio && left.sound == right.sound; }

void write_runtime_event(toml::table& runtime_config, const NotificationEventCatalogEntry& spec,
                         const NotificationPolicyResolution& result)
{
  auto* notifications = ensure_table(runtime_config, {"notifications"});
  auto* provenance    = ensure_table(runtime_config, {"notifications", "provenance"});
  if (!notifications || !provenance) {
    return;
  }

  toml::table policy;
  policy.insert_or_assign("system", result.policy.system);
  policy.insert_or_assign("audio", result.policy.audio);
  policy.insert_or_assign("sound", notification_sound_name(result.policy.sound));
  policy.is_inline(true);
  notifications->insert_or_assign(std::string(spec.canonical_key), std::move(policy));

  toml::array ignored;
  for (const auto& source : result.ignored_sources) {
    ignored.push_back(source);
  }

  toml::array diagnostics;
  for (const auto& diagnostic : result.diagnostics) {
    diagnostics.push_back(diagnostic);
  }

  toml::table source;
  source.insert_or_assign("source_kind", result.source_kind);
  source.insert_or_assign("system_source", result.system_source);
  source.insert_or_assign("audio_source", result.audio_source);
  source.insert_or_assign("sound_source", result.sound_source);
  source.insert_or_assign("deprecated_inputs", result.deprecated_inputs);
  source.insert_or_assign("conflict", result.conflict);
  source.insert_or_assign("removal_target",
                          result.deprecated_inputs ? std::string(kLegacyRemovalTarget) : std::string{});
  source.insert_or_assign("diagnostic_count", result.diagnostic_count);
  source.insert_or_assign("diagnostics", std::move(diagnostics));
  source.insert_or_assign("diagnostics_truncated",
                          result.diagnostic_count > static_cast<int>(result.diagnostics.size()));
  source.insert_or_assign("ignored_sources", std::move(ignored));
  source.insert_or_assign("ignored_sources_truncated",
                          result.ignored_source_count > static_cast<int>(result.ignored_sources.size()));
  source.is_inline(true);
  provenance->insert_or_assign(std::string(spec.canonical_key), std::move(source));
}

void write_runtime_resolution(toml::table& runtime_config)
{
  auto* notifications = ensure_table(runtime_config, {"notifications"});
  if (!notifications) {
    return;
  }

  toml::array unknown;
  for (const auto& key : s_unknown_root_keys) {
    unknown.push_back(key);
  }

  toml::table resolution;
  resolution.insert_or_assign("unknown_root_key_count", s_unknown_root_key_count);
  resolution.insert_or_assign("unknown_root_keys", std::move(unknown));
  resolution.insert_or_assign("unknown_root_keys_truncated",
                              s_unknown_root_key_count > static_cast<int>(s_unknown_root_keys.size()));
  resolution.is_inline(true);
  notifications->insert_or_assign("resolution", std::move(resolution));
}
} // namespace

void notification_policy_load(const toml::table& config, toml::table& runtime_config,
                              const NotificationConfig& legacy_notifications)
{
  // runtime.vars is resolved truth, not a second copy of every compatibility alias.
  // Replace the notification subtree after all deprecated values have been read.
  runtime_config.insert_or_assign("notifications", toml::table{});

  s_unknown_root_keys.clear();
  s_unknown_root_key_count = 0;
  if (const auto* notifications = config["notifications"].as_table()) {
    for (const auto& [key, node] : *notifications) {
      const auto name = key.str();
      if (!is_supported_root_notification_key(name)) {
        ++s_unknown_root_key_count;
        if (s_unknown_root_keys.size() < kMaxDiagnostics) {
          s_unknown_root_keys.emplace_back(name);
        }
      }
      (void)node;
    }
  }
  if (s_unknown_root_key_count > 0) {
    spdlog::warn("[NotifyPolicy] Ignoring {} unknown root notification key(s); see "
                 "notifications.resolution in community_patch_runtime.vars",
                 s_unknown_root_key_count);
  }

  const auto system_master              = resolve_legacy_master(config, "system", "notifications_enabled");
  const auto audio_master               = resolve_legacy_master(config, "audio", "notifications_audio_enabled");
  int        deprecated_event_count     = 0;
  int        canonical_deprecated_count = 0;
  int        canonical_conflict_count   = 0;

  for (const auto& spec : kNotificationEventCatalog) {
    const auto legacy_sources = legacy_sources_for_event(config, spec);
    auto       legacy =
        resolve_legacy_policy(config, legacy_notifications, spec, !legacy_sources.empty(), system_master, audio_master);
    auto result = legacy;

    if (const auto* node = root_notification_node(config, spec.canonical_key); node) {
      result                   = resolve_canonical_policy(*node, spec);
      result.deprecated_inputs = !legacy_sources.empty();
      if (!legacy_sources.empty()) {
        ++canonical_deprecated_count;
      }
      for (const auto& source : legacy_sources) {
        add_ignored_source(result, source);
      }

      if (!legacy_sources.empty() && !policies_equal(result.policy, legacy.policy)) {
        result.conflict = true;
        add_diagnostic(result, "canonical-overrides-conflicting-deprecated-policy");
        ++canonical_conflict_count;
        if (canonical_conflict_count <= kMaxConflictWarnings) {
          spdlog::warn("[NotifyPolicy] Canonical {} overrides conflicting deprecated notification inputs; deprecated "
                       "values are ignored and removed in {}",
                       canonical_path(spec), kLegacyRemovalTarget);
        }
      }
    } else if (legacy.deprecated_inputs) {
      ++deprecated_event_count;
    }

    const auto index                  = static_cast<size_t>(spec.kind);
    s_notification_policy[index]      = result.policy;
    s_notification_resolutions[index] = result;
    write_runtime_event(runtime_config, spec, result);
  }

  write_runtime_resolution(runtime_config);

  if (deprecated_event_count > 0) {
    spdlog::warn("[NotifyPolicy] Deprecated notification configuration resolved {} event(s); migrate to canonical "
                 "[notifications] keys before removal in {}",
                 deprecated_event_count, kLegacyRemovalTarget);
  }
  if (canonical_deprecated_count > 0) {
    spdlog::warn("[NotifyPolicy] {} canonical event(s) ignored deprecated notification inputs ({} conflicting); "
                 "compatibility removal target {}",
                 canonical_deprecated_count, canonical_conflict_count, kLegacyRemovalTarget);
  }
}

void notification_policy_prepare_generated_config(toml::table& user_config)
{
  toml::table notifications;
  for (const auto& spec : kNotificationEventCatalog) {
    notifications.insert_or_assign(std::string(spec.canonical_key), false);
  }
  user_config.insert_or_assign("notifications", std::move(notifications));
}

void notification_policy_write_runtime_snapshot(toml::table& runtime_config)
{
  for (const auto& spec : kNotificationEventCatalog) {
    write_runtime_event(runtime_config, spec, s_notification_resolutions[static_cast<size_t>(spec.kind)]);
  }
  write_runtime_resolution(runtime_config);
}

const NotificationPolicy& notification_policy_for(NotificationKind kind)
{
  const auto index = static_cast<size_t>(kind);
  if (index >= s_notification_policy.size()) {
    static constexpr NotificationPolicy disabled{};
    return disabled;
  }

  return s_notification_policy[index];
}

bool notification_policy_has_delivery(NotificationKind kind)
{
  const auto& policy = notification_policy_for(kind);
  return policy.system || (policy.audio && policy.sound != NotificationSound::None);
}

bool notification_policy_system_enabled(NotificationKind kind)
{ return notification_policy_for(kind).system; }

bool notification_policy_audio_enabled(NotificationKind kind)
{
  const auto& policy = notification_policy_for(kind);
  return policy.audio && policy.sound != NotificationSound::None;
}

bool notification_policy_any_system_enabled()
{
  return std::ranges::any_of(s_notification_policy, [](const auto& policy) { return policy.system; });
}

bool notification_policy_any_audio_enabled()
{
  return std::ranges::any_of(s_notification_policy, [](const auto& policy) {
    return policy.audio && policy.sound != NotificationSound::None;
  });
}

bool notification_policy_delivery_equivalent(NotificationKind left, NotificationKind right)
{
  const auto& left_policy  = notification_policy_for(left);
  const auto& right_policy = notification_policy_for(right);
  const auto  left_audio   = left_policy.audio && left_policy.sound != NotificationSound::None;
  const auto  right_audio  = right_policy.audio && right_policy.sound != NotificationSound::None;

  return left_policy.system == right_policy.system && left_audio == right_audio
         && (!left_audio || left_policy.sound == right_policy.sound);
}

std::optional<NotificationKind> notification_kind_from_toast_state(int state)
{
  const auto it =
      std::ranges::find_if(kNotificationEventCatalog, [state](const auto& spec) { return spec.toast_state == state; });
  return it == kNotificationEventCatalog.end() ? std::nullopt : std::optional{it->kind};
}

const char* notification_kind_name(NotificationKind kind)
{
  const auto* spec = notification_catalog_entry(kind);
  return spec ? spec->runtime_name.data() : "unknown";
}

const char* notification_canonical_key(NotificationKind kind)
{
  const auto* spec = notification_catalog_entry(kind);
  return spec ? spec->canonical_key.data() : "unknown";
}

const char* notification_sound_name(NotificationSound sound)
{
  switch (sound) {
    case NotificationSound::None:
      return "none";
    case NotificationSound::Default:
      return "default";
    case NotificationSound::Info:
      return "info";
    case NotificationSound::Success:
      return "success";
    case NotificationSound::Warning:
      return "warning";
    case NotificationSound::Alarm:
      return "alarm";
    case NotificationSound::Arrival:
      return "arrival";
    case NotificationSound::Soft:
      return "soft";
    case NotificationSound::Ping:
      return "ping";
    case NotificationSound::Repair:
      return "repair";
    default:
      return "default";
  }
}

std::optional<NotificationSound> notification_sound_from_name(std::string_view name)
{
  const auto normalized = normalize_sound_name(name);
  if (normalized == "none" || normalized == "off" || normalized == "silent") {
    return NotificationSound::None;
  }
  if (normalized == "default") {
    return NotificationSound::Default;
  }
  if (normalized == "info") {
    return NotificationSound::Info;
  }
  if (normalized == "success" || normalized == "victory") {
    return NotificationSound::Success;
  }
  if (normalized == "warning" || normalized == "warn") {
    return NotificationSound::Warning;
  }
  if (normalized == "alarm" || normalized == "attack") {
    return NotificationSound::Alarm;
  }
  if (normalized == "arrival" || normalized == "arrive") {
    return NotificationSound::Arrival;
  }
  if (normalized == "soft" || normalized == "quiet") {
    return NotificationSound::Soft;
  }
  if (normalized == "ping") {
    return NotificationSound::Ping;
  }
  if (normalized == "repair" || normalized == "repaired") {
    return NotificationSound::Repair;
  }

  return std::nullopt;
}
