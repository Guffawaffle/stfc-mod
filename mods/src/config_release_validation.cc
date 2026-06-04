#include "config_release_validation.h"

#include "config_metadata.h"
#include "patches/input_binding/input_binding.h"
#include "patches/input_binding/input_config_bridge.h"

#include <sstream>
#include <string>
#include <string_view>

namespace config_release_validation
{
namespace
{
  constexpr std::string_view kGeneratedStartMarker = "<!-- GENERATED INPUT BINDING COMPATIBILITY START -->";
  constexpr std::string_view kGeneratedEndMarker   = "<!-- GENERATED INPUT BINDING COMPATIBILITY END -->";

  constexpr std::string_view kRequiredScalarPaths[] = {
      "battle_log_decoder.enabled",
      "battle_log_decoder.emit_segments",
      "battle_log_decoder.emit_feed",
      "buffs.use_out_of_dock_power",
      "config.assets_url_override",
      "config.settings_url",
      "control.enable_experimental",
      "control.hotkeys_enabled",
      "control.hotkeys_extended",
      "control.queue_enabled",
      "control.select_timer",
      "control.use_scopely_hotkeys",
      "advanced.diagnostics.battle_catalog",
      "advanced.diagnostics.battle_log_decoder",
      "advanced.diagnostics.debug",
      "advanced.diagnostics.files",
      "advanced.diagnostics.files.action_queue_probe_files",
      "advanced.diagnostics.files.action_queue_probe_max_kb",
      "advanced.diagnostics.files.navhook_trace_files",
      "advanced.diagnostics.files.navhook_trace_max_kb",
      "advanced.diagnostics.files.root",
      "advanced.diagnostics.live_query",
      "advanced.diagnostics.logging",
      "advanced.diagnostics.mod_impact_monitor",
      "advanced.diagnostics.refinery_diagnostics",
      "advanced.diagnostics.runtime_trace",
      "advanced.diagnostics.runtime_trace_report_interval_ms",
      "advanced.diagnostics.runtime_trace_track_overhead",
      "advanced.diagnostics.ship_identity",
      "advanced.queue",
      "advanced.queue.queue_repair_enabled",
      "advanced.queue.queue_add_direct_handler",
      "advanced.queue.queue_add_hide_viewers",
      "graphics.allow_cursor",
      "graphics.borderless_fullscreen",
      "graphics.default_system_zoom",
      "graphics.free_resize",
      "graphics.keyboard_zoom_speed",
      "graphics.loader_enabled",
      "graphics.loader_image",
      "graphics.loader_transition",
      "graphics.show_all_resolutions",
      "graphics.system_pan_momentum",
      "graphics.system_pan_momentum_falloff",
      "graphics.system_zoom_preset_1",
      "graphics.system_zoom_preset_2",
      "graphics.system_zoom_preset_3",
      "graphics.system_zoom_preset_4",
      "graphics.system_zoom_preset_5",
      "graphics.transition_time",
      "graphics.ui_scale",
      "graphics.ui_scale_adjust",
      "graphics.ui_scale_viewer",
      "graphics.use_presets_as_default",
      "graphics.zoom",
      "input.original_frame_policy",
      "input.scopely_shortcuts",
      "sidecar.logging.jsonl",
      "sidecar.logging.jsonl_recent_logs",
      "sidecar.logging.jsonl_replay_seconds",
      "sidecar.sync.allow_unsafe_tls_without_certificate_validation",
      "sidecar.sync.battlelogs_realtime",
      "sidecar.sync.enabled",
      "sidecar.sync.fleet_runtime",
      "sidecar.sync.proxy",
      "sidecar.sync.token",
      "sidecar.sync.url",
      "sidecar.sync.verify_ssl",
      "sync.allow_unsafe_tls_without_certificate_validation",
      "sync.battlelogs",
      "sync.battlelogs_realtime",
      "sync.buffs",
      "sync.buildings",
      "sync.debug",
      "sync.fleet_runtime",
      "sync.inventory",
      "sync.jobs",
      "sync.logging",
      "sync.missions",
      "sync.officer",
      "sync.proxy",
      "sync.research",
      "sync.resolver_cache_ttl",
      "sync.resources",
      "sync.ships",
      "sync.slots",
      "sync.tech",
      "sync.token",
      "sync.traits",
      "sync.url",
      "sync.verify_ssl",
      "ui.always_skip_reveal_sequence",
      "ui.disable_escape_exit",
      "ui.disable_first_popup",
      "ui.disable_galaxy_chat",
      "ui.disable_move_keys",
      "ui.disable_preview_locate",
      "ui.disable_preview_recall",
      "ui.disable_toast_banners",
      "ui.disable_veil_chat",
      "ui.disabled_banner_types",
      "ui.escape_exit_timer",
      "ui.extend_donation_max",
      "ui.extend_donation_slider",
      "ui.show_armada_cargo",
      "ui.show_cargo_default",
      "ui.show_hostile_cargo",
      "ui.show_player_cargo",
      "ui.show_station_cargo",
  };

  constexpr std::string_view kLegacyOnlyShortcutKeys[] = {
      "set_zoom_default", "set_zoom_preset1", "set_zoom_preset2", "set_zoom_preset3", "set_zoom_preset4",
      "set_zoom_preset5", "zoom_max",         "zoom_min",         "zoom_preset1",     "zoom_preset2",
      "zoom_preset3",     "zoom_preset4",     "zoom_preset5",     "zoom_reset",
  };

  constexpr std::string_view kExplicitlyInternalOrMigrationPaths[] = {
      "control.allow_key_fallthrough", "patches.*", "sync.file", "ui.auto_confirm_discovery", "ui.notify_banner_types",
      "ui.notify_on_banner_types",
  };

  bool path_exists(const toml::table& config, std::string_view dotted_path)
  {
    const toml::table* table = &config;
    size_t             start = 0;

    while (start < dotted_path.size()) {
      const size_t end = dotted_path.find('.', start);
      const auto   key =
          dotted_path.substr(start, end == std::string_view::npos ? dotted_path.size() - start : end - start);
      if (!table) {
        return false;
      }

      const auto* node = table->get(key);
      if (!node) {
        return false;
      }

      if (end == std::string_view::npos) {
        return true;
      }

      table = node->as_table();
      start = end + 1;
    }

    return false;
  }

  std::string format_path_message(std::string_view path, std::string_view detail)
  {
    std::string message(path);
    message.append(": ");
    message.append(detail);
    return message;
  }

  void require_path(const toml::table& config, std::string_view path, ExampleConfigValidationResult& result,
                    std::string_view detail = "missing from example_community_patch_settings.toml")
  {
    if (!path_exists(config, path)) {
      result.errors.push_back({std::string(path), std::string(detail)});
    }
  }

  std::string format_shortcut_aliases(const std::span<const input_binding::BindingConfigAlias> aliases)
  {
    std::ostringstream out;
    bool               first = true;
    for (const auto& alias : aliases) {
      if (!first) {
        out << ", ";
      }
      first = false;
      out << alias.key;
      if (alias.source_kind == input_binding::BindingConfigSourceKind::DeprecatedAlias) {
        out << " (deprecated)";
      }
    }
    return out.str();
  }

  bool has_non_identity_aliases(const input_binding::InputActionSpec& spec)
  {
    const auto aliases = input_binding::ShortcutConfigAliases(spec.id);
    if (aliases.empty()) {
      return false;
    }

    return aliases.size() != 1 || aliases.front().key != spec.canonical_key
           || aliases.front().source_kind != input_binding::BindingConfigSourceKind::LegacyAlias
           || !aliases.front().deprecation_warning.empty();
  }

  void validate_input_binding_coverage(const toml::table& config, ExampleConfigValidationResult& result)
  {
    for (const auto& spec : input_binding::ActionSpecs()) {
      if (spec.default_bind == std::string_view{"NONE"} && input_binding::ShortcutConfigAliases(spec.id).empty()) {
        continue;
      }

      const std::string canonical_path = std::string("input.bindings.") + std::string(spec.canonical_key);
      if (path_exists(config, canonical_path)) {
        continue;
      }

      bool covered = false;
      for (const auto& alias : input_binding::ShortcutConfigAliases(spec.id)) {
        const std::string alias_path = std::string("shortcuts.") + std::string(alias.key);
        if (path_exists(config, alias_path)) {
          covered = true;
          break;
        }
      }

      if (!covered) {
        result.errors.push_back(
            {canonical_path, "missing canonical [input.bindings] entry and all supported [shortcuts] aliases"});
      }
    }
  }

  void validate_sync_target_examples(const toml::table& config, ExampleConfigValidationResult& result)
  {
    const auto* sync    = config["sync"].as_table();
    const auto* targets = sync ? (*sync)["targets"].as_table() : nullptr;
    if (!targets || targets->empty()) {
      result.errors.push_back({"sync.targets", "missing example sync target section"});
    } else {
      for (const auto& [name, node] : *targets) {
        if (!node.is_table()) {
          result.errors.push_back(
              {std::string("sync.targets.") + std::string(name.str()), "target example must be a table"});
        }
      }
    }
  }
} // namespace

ExampleConfigValidationResult ValidateExampleConfig(const toml::table& config)
{
  ExampleConfigValidationResult result;

  for (const auto path : kRequiredScalarPaths) {
    require_path(config, path, result);
  }

  for (const auto& spec : config_metadata::notificationBoolConfigSpecs) {
    require_path(config, spec.canonical_path, result);
  }

  for (const auto& spec : config_metadata::notificationToggleSpecs) {
    require_path(config, std::string(spec.section) + "." + std::string(spec.key), result);
  }

  validate_input_binding_coverage(config, result);

  for (const auto key : kLegacyOnlyShortcutKeys) {
    require_path(config, std::string("shortcuts.") + std::string(key), result);
  }

  validate_sync_target_examples(config, result);

  (void)kExplicitlyInternalOrMigrationPaths;
  return result;
}

std::string RenderGeneratedInputBindingCompatibilitySection()
{
  std::ostringstream out;
  out << kGeneratedStartMarker << "\n";
  out << "| [input.bindings] key | Default | Legacy [shortcuts] keys |\n";
  out << "| --- | --- | --- |\n";

  for (const auto& spec : input_binding::ActionSpecs()) {
    if (!has_non_identity_aliases(spec)) {
      continue;
    }

    out << "| " << spec.canonical_key << " | " << spec.default_bind << " | "
        << format_shortcut_aliases(input_binding::ShortcutConfigAliases(spec.id)) << " |\n";
  }

  out << kGeneratedEndMarker;
  return out.str();
}

std::string ExtractGeneratedInputBindingCompatibilitySection(std::string_view markdown)
{
  const auto start = markdown.find(kGeneratedStartMarker);
  if (start == std::string_view::npos) {
    return {};
  }

  const auto end_marker = markdown.find(kGeneratedEndMarker, start);
  if (end_marker == std::string_view::npos) {
    return {};
  }

  const auto end = end_marker + kGeneratedEndMarker.size();
  return std::string(markdown.substr(start, end - start));
}

std::string NormalizeMarkdownNewlines(std::string_view text)
{
  std::string normalized;
  normalized.reserve(text.size());

  for (size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\r') {
      continue;
    }
    normalized.push_back(text[index]);
  }

  return normalized;
}

} // namespace config_release_validation
