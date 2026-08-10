#include "config_sidecar.h"

#include "config_redaction.h"
#include "defaultconfig.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

namespace DCSidecar  = DefaultConfig::Sidecar;
namespace DCAdvanced = DefaultConfig::Advanced;

namespace
{
std::vector<std::string_view> split_path(std::string_view path)
{
  std::vector<std::string_view> segments;
  size_t                        start = 0;
  while (start <= path.size()) {
    const auto end = path.find('.', start);
    if (end == std::string_view::npos) {
      const auto segment = path.substr(start);
      if (!segment.empty()) {
        segments.push_back(segment);
      }
      break;
    }

    const auto segment = path.substr(start, end - start);
    if (!segment.empty()) {
      segments.push_back(segment);
    }
    start = end + 1;
  }

  return segments;
}

const toml::node* node_at_path(const toml::table& config, std::string_view path)
{
  const toml::node* current = &config;
  for (const auto segment : split_path(path)) {
    const auto* table = current ? current->as_table() : nullptr;
    if (!table) {
      return nullptr;
    }

    current = table->get(segment);
    if (!current) {
      return nullptr;
    }
  }

  return current;
}

const char* toml_type_name(toml::node_type type)
{
  switch (type) {
    case toml::node_type::none:
      return "none";
    case toml::node_type::table:
      return "table";
    case toml::node_type::array:
      return "array";
    case toml::node_type::string:
      return "string";
    case toml::node_type::integer:
      return "integer";
    case toml::node_type::floating_point:
      return "float";
    case toml::node_type::boolean:
      return "boolean";
    case toml::node_type::date:
      return "date";
    case toml::node_type::time:
      return "time";
    case toml::node_type::date_time:
      return "date_time";
  }

  return "unknown";
}

config_schema::Diagnostic make_diagnostic(config_schema::DiagnosticSeverity severity, std::string_view path,
                                          std::string_view source_path, std::string message)
{ return {severity, std::string(path), std::string(source_path), std::move(message)}; }

std::string make_invalid_scalar_message(std::string_view path, std::string_view expected_type,
                                        std::string_view actual_type, std::string_view description)
{
  std::ostringstream message;
  message << "Invalid config " << path;
  if (!description.empty()) {
    message << " (" << description << ")";
  }
  message << ". Expected " << expected_type << ", found " << actual_type << "; using default.";
  return message.str();
}

struct StringReadResult {
  std::string                            value;
  std::vector<config_schema::Diagnostic> diagnostics;
};

StringReadResult read_string(const toml::table& config, std::string_view path, std::string_view default_value,
                             std::string_view description)
{
  StringReadResult result;
  result.value = std::string(default_value);

  const auto* node = node_at_path(config, path);
  if (!node) {
    return result;
  }

  if (auto value = node->value<std::string>(); value.has_value()) {
    result.value = value.value();
    return result;
  }

  result.diagnostics.push_back(
      make_diagnostic(config_schema::DiagnosticSeverity::Warning, path, path,
                      make_invalid_scalar_message(path, "string", toml_type_name(node->type()), description)));
  return result;
}

struct IntReadResult {
  int                                    value = 0;
  std::vector<config_schema::Diagnostic> diagnostics;
};

IntReadResult read_int(const toml::table& config, std::string_view path, int default_value,
                       std::string_view description)
{
  IntReadResult result;
  result.value = default_value;

  const auto* node = node_at_path(config, path);
  if (!node) {
    return result;
  }

  if (auto value = node->value<int64_t>(); value.has_value()) {
    if (value.value() < std::numeric_limits<int>::min() || value.value() > std::numeric_limits<int>::max()) {
      result.diagnostics.push_back(
          make_diagnostic(config_schema::DiagnosticSeverity::Warning, path, path,
                          make_invalid_scalar_message(path, "integer", "out-of-range integer", description)));
      return result;
    }

    result.value = static_cast<int>(value.value());
    return result;
  }

  result.diagnostics.push_back(
      make_diagnostic(config_schema::DiagnosticSeverity::Warning, path, path,
                      make_invalid_scalar_message(path, "integer", toml_type_name(node->type()), description)));
  return result;
}

template <typename T> void append_diagnostics(std::vector<config_schema::Diagnostic>& target, const T& source)
{ target.insert(target.end(), source.diagnostics.begin(), source.diagnostics.end()); }

template <typename T> void write_scalar(toml::table& config, std::string_view path, T value)
{
  const auto segments = split_path(path);
  if (segments.empty()) {
    return;
  }

  auto* table = &config;
  for (size_t index = 0; index + 1 < segments.size(); ++index) {
    const auto segment = std::string(segments[index]);
    auto*      node    = table->get(segment);
    if (!node || !node->is_table()) {
      table->insert_or_assign(segment, toml::table{});
      node = table->get(segment);
    }

    table = node->as_table();
    if (!table) {
      return;
    }
  }

  table->insert_or_assign(std::string(segments.back()), value);
}

void ensure_table(toml::table& config, std::string_view path)
{
  const auto segments = split_path(path);
  if (segments.empty()) {
    return;
  }

  auto* table = &config;
  for (const auto segment_view : segments) {
    const auto segment = std::string(segment_view);
    auto*      node    = table->get(segment);
    if (!node || !node->is_table()) {
      table->insert_or_assign(segment, toml::table{});
      node = table->get(segment);
    }

    table = node->as_table();
    if (!table) {
      return;
    }
  }
}

std::string ascii_lower(std::string_view value)
{
  std::string lowered(value);
  std::ranges::transform(lowered, lowered.begin(),
                         [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lowered;
}

bool contains_any(std::string_view value, std::span<const std::string_view> needles)
{
  return std::ranges::any_of(needles, [value](const auto needle) { return value.find(needle) != std::string::npos; });
}

bool is_valid_fleet_runtime_mode(std::string_view mode)
{
  constexpr std::array<std::string_view, 4> kModes{
      "normal",
      "request_only",
      "snapshot_only",
      "enqueue_no_transport",
  };
  return std::ranges::find(kModes, mode) != kModes.end();
}

bool rejected_target_name_seen(std::set<std::string>& rejected_targets, const std::string& target_name,
                               std::vector<SidecarRejectedSyncTarget>& output)
{
  if (!rejected_targets.emplace(target_name).second) {
    return false;
  }

  output.push_back({target_name});
  return true;
}

std::string make_invalid_table_message(std::string_view path, std::string_view actual_type,
                                       std::string_view description)
{
  std::ostringstream message;
  message << "Invalid config " << path;
  if (!description.empty()) {
    message << " (" << description << ")";
  }
  message << ". Expected table, found " << actual_type << "; ignoring contents.";
  return message.str();
}
} // namespace

bool IsLoopbackSidecarSyncUrl(std::string_view url)
{
  const auto                                lowered = ascii_lower(url);
  constexpr std::array<std::string_view, 3> kLoopbackHosts{
      "://127.0.0.1",
      "://localhost",
      "://[::1]",
  };
  constexpr std::array<std::string_view, 4> kSidecarPaths{
      "/api/events",
      "/api/sidecar/ingest",
      "/api/majel/ingest",
      "/api/fleet/",
  };

  return contains_any(lowered, kLoopbackHosts) && contains_any(lowered, kSidecarPaths);
}

SidecarConfigParseResult ParseSidecarConfig(const toml::table& config)
{
  SidecarConfigParseResult result;

  const auto read_bool_value = [&config, &result](bool& destination, const config_schema::BoolSetting& setting) {
    const auto read = config_schema::read_bool(config, setting);
    destination     = read.value;
    append_diagnostics(result.diagnostics, read);
  };

  const auto read_string_value = [&config, &result](std::string& destination, std::string_view path,
                                                    std::string_view default_value, std::string_view description) {
    const auto read = read_string(config, path, default_value, description);
    destination     = read.value;
    append_diagnostics(result.diagnostics, read);
  };

  const auto read_int_value = [&config, &result](int& destination, std::string_view path, int default_value,
                                                 std::string_view description) {
    const auto read = read_int(config, path, default_value, description);
    destination     = read.value;
    append_diagnostics(result.diagnostics, read);
  };

  read_bool_value(result.config.sync.enabled,
                  {"sidecar.sync.enabled", DCSidecar::Sync::enabled, {}, "enable local sidecar delivery"});
  const auto* transport_node = node_at_path(config, "sidecar.sync.transport");
  read_string_value(result.config.sync.transport, "sidecar.sync.transport", DCSidecar::Sync::transport,
                    "local sidecar transport: legacy_http or named_pipe");
  if (transport_node && !transport_node->is_string()) {
    result.config.sync.transport = "invalid";
  }
  if (result.config.sync.transport != "legacy_http" && result.config.sync.transport != "named_pipe") {
    result.diagnostics.push_back(make_diagnostic(
        config_schema::DiagnosticSeverity::Error, "sidecar.sync.transport", "sidecar.sync.transport",
        "Invalid sidecar.sync.transport. Expected legacy_http or named_pipe; local delivery is disabled."));
    result.config.sync.transport = "invalid";
  }
  read_string_value(result.config.sync.pipe_name, "sidecar.sync.pipe_name", DCSidecar::Sync::pipe_name,
                    "Battle Bridge named-pipe name");
  read_string_value(result.config.sync.url, "sidecar.sync.url", DCSidecar::Sync::url,
                    "loopback or local sidecar ingest URL");
  read_string_value(result.config.sync.token, "sidecar.sync.token", DCSidecar::Sync::token, "sidecar ingest token");
  read_string_value(result.config.sync.proxy, "sidecar.sync.proxy", DCSidecar::Sync::proxy,
                    "HTTP proxy for local sidecar delivery");
  read_bool_value(result.config.sync.verify_ssl,
                  {"sidecar.sync.verify_ssl", DCSidecar::Sync::verify_ssl, {}, "verify TLS certificates"});
  read_bool_value(result.config.sync.allow_unsafe_tls_without_certificate_validation,
                  {"sidecar.sync.allow_unsafe_tls_without_certificate_validation",
                   DCSidecar::Sync::allow_unsafe_tls_without_certificate_validation,
                   {},
                   "unsafe TLS override for sidecar delivery"});
  read_bool_value(
      result.config.sync.battlelogs_realtime,
      {"sidecar.sync.battlelogs_realtime", DCSidecar::Sync::battlelogs_realtime, {}, "sidecar battle-log delivery"});
  constexpr std::array<std::string_view, 1> kBattlelogEnrichmentAlias{"battle_log_decoder.enabled"};
  read_bool_value(result.config.sync.battlelog_enrichment,
                  {"sidecar.sync.battlelog_enrichment", DCSidecar::Sync::battlelog_enrichment,
                   kBattlelogEnrichmentAlias, "decode local sidecar battle reports, catalog snapshots, and analytics"});
  read_bool_value(result.config.sync.fleet_runtime,
                  {"sidecar.sync.fleet_runtime", DCSidecar::Sync::fleet_runtime, {}, "sidecar fleet-runtime delivery"});
  read_string_value(result.config.sync.fleet_runtime_mode, "sidecar.sync.fleet_runtime_mode",
                    DCSidecar::Sync::fleet_runtime_mode, "sidecar fleet-runtime diagnostic mode");
  result.config.sync.fleet_runtime_mode = ascii_lower(result.config.sync.fleet_runtime_mode);
  if (!is_valid_fleet_runtime_mode(result.config.sync.fleet_runtime_mode)) {
    result.diagnostics.push_back(
        make_diagnostic(config_schema::DiagnosticSeverity::Warning, "sidecar.sync.fleet_runtime_mode",
                        "sidecar.sync.fleet_runtime_mode",
                        "Invalid sidecar.sync.fleet_runtime_mode. Expected normal, request_only, snapshot_only, or "
                        "enqueue_no_transport; using normal."));
    result.config.sync.fleet_runtime_mode = DCSidecar::Sync::fleet_runtime_mode;
  }

  read_bool_value(result.config.logging.jsonl,
                  {"sidecar.logging.jsonl", DCSidecar::Logging::jsonl, {}, "local sidecar JSONL capture"});
  read_int_value(result.config.logging.jsonl_replay_seconds, "sidecar.logging.jsonl_replay_seconds",
                 DCSidecar::Logging::jsonl_replay_seconds, "sidecar JSONL replay window seconds");
  result.config.logging.jsonl_replay_seconds = std::max(0, result.config.logging.jsonl_replay_seconds);
  read_int_value(result.config.logging.jsonl_recent_logs, "sidecar.logging.jsonl_recent_logs",
                 DCSidecar::Logging::jsonl_recent_logs, "sidecar JSONL retained recent battle logs");
  result.config.logging.jsonl_recent_logs = std::max(0, result.config.logging.jsonl_recent_logs);

  if (const auto* diagnostics_node = node_at_path(config, "advanced.diagnostics");
      diagnostics_node && !diagnostics_node->is_table()) {
    result.diagnostics.push_back(
        make_diagnostic(config_schema::DiagnosticSeverity::Warning, "advanced.diagnostics", "advanced.diagnostics",
                        make_invalid_table_message("advanced.diagnostics", toml_type_name(diagnostics_node->type()),
                                                   "reserved native diagnostics namespace")));
  }

  if (const auto* diagnostics_files_node = node_at_path(config, "advanced.diagnostics.files");
      diagnostics_files_node && !diagnostics_files_node->is_table()) {
    result.diagnostics.push_back(make_diagnostic(
        config_schema::DiagnosticSeverity::Warning, "advanced.diagnostics.files", "advanced.diagnostics.files",
        make_invalid_table_message("advanced.diagnostics.files", toml_type_name(diagnostics_files_node->type()),
                                   "native diagnostics file policy namespace")));
  }

  if (const auto* queue_node = node_at_path(config, "advanced.queue"); queue_node && !queue_node->is_table()) {
    result.diagnostics.push_back(
        make_diagnostic(config_schema::DiagnosticSeverity::Warning, "advanced.queue", "advanced.queue",
                        make_invalid_table_message("advanced.queue", toml_type_name(queue_node->type()),
                                                   "queue experiment/dev-test namespace")));
  }

  constexpr std::array<std::string_view, 1> kShipIdentityAlias{"sidecar.probes.ship_identity"};
  constexpr std::array<std::string_view, 1> kBattleLogDecoderAlias{"sidecar.probes.battle_log_decoder"};
  constexpr std::array<std::string_view, 1> kBattleCatalogAlias{"sidecar.probes.battle_catalog"};
  constexpr std::array<std::string_view, 1> kDebugAlias{"sidecar.diagnostics.debug"};
  constexpr std::array<std::string_view, 1> kLoggingAlias{"sidecar.diagnostics.logging"};

  read_bool_value(result.advanced.diagnostics.ship_identity,
                  {"advanced.diagnostics.ship_identity", DCAdvanced::Diagnostics::ship_identity, kShipIdentityAlias,
                   "reserved ship identity observability probes"});
  read_bool_value(result.advanced.diagnostics.battle_log_decoder,
                  {"advanced.diagnostics.battle_log_decoder", DCAdvanced::Diagnostics::battle_log_decoder,
                   kBattleLogDecoderAlias, "reserved battle log decoder observability probes"});
  read_bool_value(result.advanced.diagnostics.battle_catalog,
                  {"advanced.diagnostics.battle_catalog", DCAdvanced::Diagnostics::battle_catalog, kBattleCatalogAlias,
                   "reserved battle catalog observability probes"});
  read_bool_value(result.advanced.diagnostics.debug, {"advanced.diagnostics.debug", DCAdvanced::Diagnostics::debug,
                                                      kDebugAlias, "reserved native debug diagnostics"});
  read_bool_value(result.advanced.diagnostics.logging,
                  {"advanced.diagnostics.logging", DCAdvanced::Diagnostics::logging, kLoggingAlias,
                   "reserved native payload logging diagnostics"});
  read_bool_value(result.advanced.diagnostics.hotkey_suppression_logging,
                  {"advanced.diagnostics.hotkey_suppression_logging",
                   DCAdvanced::Diagnostics::hotkey_suppression_logging,
                   {},
                   "log suppressed native shortcut update breadcrumbs"});
  read_bool_value(result.advanced.diagnostics.notification_skip_logging,
                  {"advanced.diagnostics.notification_skip_logging",
                   DCAdvanced::Diagnostics::notification_skip_logging,
                   {},
                   "log skipped notification toast breadcrumbs"});
  read_bool_value(result.advanced.diagnostics.fleet_selection_timing_logging,
                  {"advanced.diagnostics.fleet_selection_timing_logging",
                   DCAdvanced::Diagnostics::fleet_selection_timing_logging,
                   {},
                   "log fleet selection timing breadcrumbs"});
  read_bool_value(result.advanced.diagnostics.live_query, {"advanced.diagnostics.live_query",
                                                           DCAdvanced::Diagnostics::live_query,
                                                           {},
                                                           "enable the live debug query channel"});
  read_string_value(result.advanced.diagnostics.runtime_trace, "advanced.diagnostics.runtime_trace",
                    DCAdvanced::Diagnostics::runtime_trace, "runtime trace level");
  read_bool_value(result.advanced.diagnostics.runtime_trace_track_overhead,
                  {"advanced.diagnostics.runtime_trace_track_overhead",
                   DCAdvanced::Diagnostics::runtime_trace_track_overhead,
                   {},
                   "record runtime trace overhead separately"});
  read_bool_value(result.advanced.diagnostics.action_queue_guard_logging,
                  {"advanced.diagnostics.action_queue_guard_logging",
                   DCAdvanced::Diagnostics::action_queue_guard_logging,
                   {},
                   "emit detailed action queue guard breadcrumbs"});
  read_bool_value(result.advanced.diagnostics.mod_impact_monitor, {"advanced.diagnostics.mod_impact_monitor",
                                                                   DCAdvanced::Diagnostics::mod_impact_monitor,
                                                                   {},
                                                                   "emit periodic runtime impact summaries"});
  read_int_value(result.advanced.diagnostics.runtime_trace_report_interval_ms,
                 "advanced.diagnostics.runtime_trace_report_interval_ms",
                 DCAdvanced::Diagnostics::runtime_trace_report_interval_ms, "runtime trace summary report interval");
  read_bool_value(result.advanced.diagnostics.refinery_diagnostics, {"advanced.diagnostics.refinery_diagnostics",
                                                                     DCAdvanced::Diagnostics::refinery_diagnostics,
                                                                     {},
                                                                     "emit focused refinery lifecycle diagnostics"});
  read_string_value(result.advanced.diagnostics.files.root, "advanced.diagnostics.files.root",
                    DCAdvanced::Diagnostics::Files::root, "optional native diagnostics root");
  read_int_value(result.advanced.diagnostics.files.navhook_trace_max_kb,
                 "advanced.diagnostics.files.navhook_trace_max_kb",
                 DCAdvanced::Diagnostics::Files::navhook_trace_max_kb, "navhook trace max size in KB");
  read_int_value(result.advanced.diagnostics.files.navhook_trace_files,
                 "advanced.diagnostics.files.navhook_trace_files", DCAdvanced::Diagnostics::Files::navhook_trace_files,
                 "navhook trace total file count");
  read_int_value(result.advanced.diagnostics.files.action_queue_probe_max_kb,
                 "advanced.diagnostics.files.action_queue_probe_max_kb",
                 DCAdvanced::Diagnostics::Files::action_queue_probe_max_kb, "action queue probe max size in KB");
  read_int_value(result.advanced.diagnostics.files.action_queue_probe_files,
                 "advanced.diagnostics.files.action_queue_probe_files",
                 DCAdvanced::Diagnostics::Files::action_queue_probe_files, "action queue probe total file count");
  result.advanced.diagnostics.files.navhook_trace_max_kb =
      std::max(1, result.advanced.diagnostics.files.navhook_trace_max_kb);
  result.advanced.diagnostics.files.navhook_trace_files =
      std::max(1, result.advanced.diagnostics.files.navhook_trace_files);
  result.advanced.diagnostics.files.action_queue_probe_max_kb =
      std::max(1, result.advanced.diagnostics.files.action_queue_probe_max_kb);
  result.advanced.diagnostics.files.action_queue_probe_files =
      std::max(1, result.advanced.diagnostics.files.action_queue_probe_files);
  read_bool_value(result.advanced.queue.queue_repair_enabled, {"advanced.queue.queue_repair_enabled",
                                                               DefaultConfig::Advanced::Queue::queue_repair_enabled,
                                                               {},
                                                               "enable queue repair and probe experiments"});
  read_bool_value(result.advanced.queue.thin_queue_protection,
                  {"advanced.queue.thin_queue_protection",
                   DefaultConfig::Advanced::Queue::thin_queue_protection,
                   {},
                   "recover an authoritative externally destroyed target only at the exact idle queue head"});
  read_bool_value(result.advanced.queue.queue_add_direct_handler,
                  {"advanced.queue.queue_add_direct_handler",
                   DefaultConfig::Advanced::Queue::queue_add_direct_handler,
                   {},
                   "use the direct queue-add handler during local diagnostics"});
  constexpr std::string_view kLegacyQueueAddHideViewersPath = "advanced.queue.queue_add_hide_viewers";
  if (const auto* legacy_hide_viewers = node_at_path(config, kLegacyQueueAddHideViewersPath); legacy_hide_viewers) {
    const auto configured_value = legacy_hide_viewers->value<bool>();
    if (!configured_value.has_value()) {
      std::ostringstream message;
      message << "Invalid config " << kLegacyQueueAddHideViewersPath << ". Expected boolean, found "
              << toml_type_name(legacy_hide_viewers->type())
              << "; deprecated key is ignored and successful queue-add actions always dismiss the target viewer.";
      result.diagnostics.push_back(make_diagnostic(config_schema::DiagnosticSeverity::Warning,
                                                   kLegacyQueueAddHideViewersPath, kLegacyQueueAddHideViewersPath,
                                                   message.str()));
    } else {
      const auto severity = configured_value.value() ? config_schema::DiagnosticSeverity::Info
                                                     : config_schema::DiagnosticSeverity::Warning;
      result.diagnostics.push_back(make_diagnostic(
          severity, kLegacyQueueAddHideViewersPath, kLegacyQueueAddHideViewersPath,
          "Deprecated config key advanced.queue.queue_add_hide_viewers is ignored; successful queue-add actions "
          "always dismiss the target viewer."));
    }
  }

  // Keep the deprecated sidecar-scoped members mirrored for low-risk
  // compatibility, but treat advanced.diagnostics as canonical.
  result.config.probes.ship_identity      = result.advanced.diagnostics.ship_identity;
  result.config.probes.battle_log_decoder = result.advanced.diagnostics.battle_log_decoder;
  result.config.probes.battle_catalog     = result.advanced.diagnostics.battle_catalog;
  result.config.diagnostics.debug         = result.advanced.diagnostics.debug;
  result.config.diagnostics.logging       = result.advanced.diagnostics.logging;

  constexpr std::array<std::pair<std::string_view, std::string_view>, 3> kLegacySidecarSyncPaths{{
      {"sync.sidecar_jsonl", "sidecar.logging.jsonl"},
      {"sync.sidecar_jsonl_replay_seconds", "sidecar.logging.jsonl_replay_seconds"},
      {"sync.sidecar_jsonl_recent_logs", "sidecar.logging.jsonl_recent_logs"},
  }};

  for (const auto& [legacy_path, canonical_path] : kLegacySidecarSyncPaths) {
    if (!config_schema::path_exists(config, legacy_path)) {
      continue;
    }

    std::ostringstream message;
    message << "Legacy sidecar config " << legacy_path << " is invalid. Use " << canonical_path << " instead.";
    result.diagnostics.push_back(
        make_diagnostic(config_schema::DiagnosticSeverity::Error, legacy_path, legacy_path, message.str()));
  }

  std::set<std::string> rejected_targets;
  if (const auto* sync = config["sync"].as_table()) {
    if (const auto* targets = (*sync)["targets"].as_table()) {
      for (const auto& [target_key, node] : *targets) {
        if (!node.is_table()) {
          continue;
        }

        const std::string target_name = std::string(target_key.str());
        const std::string base_path   = "sync.targets." + target_name;
        const auto&       table       = *node.as_table();

        if (ascii_lower(target_name) == "sidecar") {
          result.diagnostics.push_back(make_diagnostic(
              config_schema::DiagnosticSeverity::Error, base_path, base_path,
              "[sync.targets.sidecar] is invalid. Configure local sidecar delivery under [sidecar.sync] instead."));
          rejected_target_name_seen(rejected_targets, target_name, result.rejected_sync_targets);
        }

        if (auto mode = table["mode"].value<std::string>();
            mode.has_value() && ascii_lower(mode.value()) == "sidecar_broker") {
          result.diagnostics.push_back(make_diagnostic(
              config_schema::DiagnosticSeverity::Error, base_path + ".mode", base_path + ".mode",
              "mode = \"sidecar_broker\" is invalid. Local sidecar delivery belongs under [sidecar.sync]."));
          rejected_target_name_seen(rejected_targets, target_name, result.rejected_sync_targets);
        }

        if (auto url = table["url"].value<std::string>(); url.has_value() && IsLoopbackSidecarSyncUrl(url.value())) {
          result.diagnostics.push_back(make_diagnostic(
              config_schema::DiagnosticSeverity::Error, base_path + ".url", base_path + ".url",
              "Loopback sidecar ingest URLs are invalid under [sync.targets.*]. Configure [sidecar.sync] instead."));
          rejected_target_name_seen(rejected_targets, target_name, result.rejected_sync_targets);
        }
      }
    }

    if (auto sync_url = (*sync)["url"].value<std::string>();
        sync_url.has_value() && IsLoopbackSidecarSyncUrl(sync_url.value())) {
      result.reject_legacy_sync_url = true;
      result.diagnostics.push_back(
          make_diagnostic(config_schema::DiagnosticSeverity::Error, "sync.url", "sync.url",
                          "Loopback sidecar ingest URLs are invalid under [sync]. Configure [sidecar.sync] instead."));
    }
  }

  return result;
}

void WriteSidecarConfigRuntimeSnapshot(toml::table& runtime_config, const SidecarConfig& config)
{
  config_schema::write_bool(runtime_config, "sidecar.sync.enabled", config.sync.enabled);
  write_scalar(runtime_config, "sidecar.sync.transport", config.sync.transport);
  write_scalar(runtime_config, "sidecar.sync.pipe_name", config.sync.pipe_name);
  write_scalar(runtime_config, "sidecar.sync.url", config.sync.url);
  write_scalar(runtime_config, "sidecar.sync.token",
               config_redaction::redact_secret_for_runtime_snapshot(config.sync.token));
  write_scalar(runtime_config, "sidecar.sync.proxy", config_redaction::mask_proxy_userinfo(config.sync.proxy));
  config_schema::write_bool(runtime_config, "sidecar.sync.verify_ssl", config.sync.verify_ssl);
  config_schema::write_bool(runtime_config, "sidecar.sync.allow_unsafe_tls_without_certificate_validation",
                            config.sync.allow_unsafe_tls_without_certificate_validation);
  config_schema::write_bool(runtime_config, "sidecar.sync.battlelogs_realtime", config.sync.battlelogs_realtime);
  config_schema::write_bool(runtime_config, "sidecar.sync.battlelog_enrichment", config.sync.battlelog_enrichment);
  config_schema::write_bool(runtime_config, "sidecar.sync.fleet_runtime", config.sync.fleet_runtime);
  write_scalar(runtime_config, "sidecar.sync.fleet_runtime_mode", config.sync.fleet_runtime_mode);

  config_schema::write_bool(runtime_config, "sidecar.logging.jsonl", config.logging.jsonl);
  write_scalar(runtime_config, "sidecar.logging.jsonl_replay_seconds", config.logging.jsonl_replay_seconds);
  write_scalar(runtime_config, "sidecar.logging.jsonl_recent_logs", config.logging.jsonl_recent_logs);
}

void WriteAdvancedConfigRuntimeSnapshot(toml::table& runtime_config, const AdvancedConfig& config)
{
  config_schema::write_bool(runtime_config, "advanced.diagnostics.ship_identity", config.diagnostics.ship_identity);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.battle_log_decoder",
                            config.diagnostics.battle_log_decoder);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.battle_catalog", config.diagnostics.battle_catalog);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.debug", config.diagnostics.debug);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.logging", config.diagnostics.logging);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.hotkey_suppression_logging",
                            config.diagnostics.hotkey_suppression_logging);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.notification_skip_logging",
                            config.diagnostics.notification_skip_logging);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.fleet_selection_timing_logging",
                            config.diagnostics.fleet_selection_timing_logging);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.live_query", config.diagnostics.live_query);
  write_scalar(runtime_config, "advanced.diagnostics.runtime_trace", config.diagnostics.runtime_trace);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.runtime_trace_track_overhead",
                            config.diagnostics.runtime_trace_track_overhead);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.action_queue_guard_logging",
                            config.diagnostics.action_queue_guard_logging);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.mod_impact_monitor",
                            config.diagnostics.mod_impact_monitor);
  write_scalar(runtime_config, "advanced.diagnostics.runtime_trace_report_interval_ms",
               config.diagnostics.runtime_trace_report_interval_ms);
  config_schema::write_bool(runtime_config, "advanced.diagnostics.refinery_diagnostics",
                            config.diagnostics.refinery_diagnostics);
  write_scalar(runtime_config, "advanced.diagnostics.files.root", config.diagnostics.files.root);
  write_scalar(runtime_config, "advanced.diagnostics.files.navhook_trace_max_kb",
               config.diagnostics.files.navhook_trace_max_kb);
  write_scalar(runtime_config, "advanced.diagnostics.files.navhook_trace_files",
               config.diagnostics.files.navhook_trace_files);
  write_scalar(runtime_config, "advanced.diagnostics.files.action_queue_probe_max_kb",
               config.diagnostics.files.action_queue_probe_max_kb);
  write_scalar(runtime_config, "advanced.diagnostics.files.action_queue_probe_files",
               config.diagnostics.files.action_queue_probe_files);
  config_schema::write_bool(runtime_config, "advanced.queue.queue_repair_enabled", config.queue.queue_repair_enabled);
  config_schema::write_bool(runtime_config, "advanced.queue.thin_queue_protection", config.queue.thin_queue_protection);
  config_schema::write_bool(runtime_config, "advanced.queue.queue_add_direct_handler",
                            config.queue.queue_add_direct_handler);
}

void OmitOptInRuntimeDiagnosticsFromGeneratedUserConfig(toml::table& user_config)
{
  auto* advanced = user_config["advanced"].as_table();
  if (!advanced) {
    return;
  }

  auto* diagnostics = (*advanced)["diagnostics"].as_table();
  if (diagnostics) {
    diagnostics->erase("runtime_trace");
    diagnostics->erase("runtime_trace_track_overhead");
    diagnostics->erase("runtime_trace_report_interval_ms");
    diagnostics->erase("mod_impact_monitor");
    diagnostics->erase("action_queue_guard_logging");
  }
}
