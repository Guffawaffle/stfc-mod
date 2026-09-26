#pragma once
#include "page_catalog.h"
#include <cctype>
#include <sstream>

namespace mod_settings
{
// Presentation IDs predate search; several intentionally differ from the saved
// key. Keep that translation explicit rather than advertising invented TOML keys.
inline std::string SearchTomlKey(std::string_view id)
{
  constexpr std::pair<std::string_view, std::string_view> aliases[]{
      {"community_mod.warp_mode", "ui.auto_confirm_instant_warp"},
      {"community_mod.navigation.galactic_anomaly_timer", "graphics.galactic_anomaly_timer"},
      {"community_mod.labels.player.detail", "graphics.zoom_label_player_detail"},
      {"community_mod.labels.other.detail", "graphics.zoom_label_non_player_detail"},
      {"community_mod.labels.player.threshold", "graphics.zoom_label_player_threshold"},
      {"community_mod.labels.other.threshold", "graphics.zoom_label_non_player_threshold"},
      {"community_mod.labels.ship_hotkeys", "graphics.ship_hotkey_badges"},
      {"community_mod.galaxy.major.detail", "graphics.galaxy_label_major_detail"},
      {"community_mod.galaxy.minor.detail", "graphics.galaxy_label_minor_detail"},
      {"community_mod.galaxy.major.threshold", "graphics.galaxy_label_major_threshold"},
      {"community_mod.galaxy.minor.threshold", "graphics.galaxy_label_minor_threshold"},
      {"community_mod.galaxy.extended_selection", "graphics.galaxy_extended_selection"},
      {"community_mod.galaxy.multi_select", "graphics.galaxy_multi_select"},
  };
  for (const auto& [identity, key] : aliases)
    if (id == identity)
      return std::string(key);
  if (id.starts_with("community_mod.hud."))
    return "ui.hud_" + std::string(id.substr(18));
  if (id.starts_with("community_mod.galaxy.overlays."))
    return "graphics.galaxy_overlay_"
           + std::string(id.substr(std::string_view("community_mod.galaxy.overlays.").size()));
  for (auto prefix :
       {"community_mod.ui.", "community_mod.graphics.", "community_mod.shortcuts.", "community_mod.audio."})
    if (id.starts_with(prefix))
      return std::string(id.substr(14));
  return {};
}

inline std::string SearchWords(std::string_view text)
{
  std::string result;
  for (unsigned char c : text)
    // TOML underscores, dots and UI spaces should be interchangeable. Preserve
    // UTF-8 bytes; current catalog labels/keys use ASCII for searchable words.
    result += c == '_' || c == '.' || c == '-' ? ' ' : static_cast<char>(std::tolower(c));
  return result;
}
struct SettingsSearchEntry {
  std::string page, item, label, location, key, words;
};
class SettingsSearch
{
public:
  static constexpr std::size_t ResultLimit = 32;
  static constexpr int         QueryLimit  = 128;
  void                         Build(const std::vector<PageCatalog::Page>& pages)
  {
    entries_.clear();
    const auto path = [&](const PageCatalog::Page& page) {
      std::string text   = page.label;
      auto        parent = page.parent;
      for (std::size_t depth = 0; depth < pages.size() && parent != "community_mod.settings"; ++depth) {
        auto it = std::ranges::find(pages, parent, &PageCatalog::Page::id);
        if (it == pages.end())
          break;
        text   = it->label + " > " + text;
        parent = it->parent;
      }
      return text;
    };
    for (const auto& page : pages) {
      const auto location = path(page);
      // Shortcut/audio pages contain commands for one persisted setting, not
      // separate settings named Change, Remove, Test or Restore.
      if ((page.id.starts_with("community_mod.shortcuts.") || page.id.starts_with("community_mod.audio."))
          && !page.items.empty()
          && std::ranges::none_of(pages, [&](const auto& child) { return child.parent == page.id; })) {
        Add(page.id, {}, page.label, location, SearchTomlKey(page.id));
        continue;
      }
      std::string section;
      for (const auto& item : page.items)
        std::visit(
            [&](const auto& value) {
              using T = std::decay_t<decltype(value)>;
              if constexpr (std::is_same_v<T, PageCatalog::Heading>) {
                section = value.label;
              } else if constexpr (!std::is_same_v<T, ActionSetting*>) {
                const auto& setting = [&]() -> const auto& {
                  if constexpr (std::is_same_v<T, BooleanSetting*>)
                    return *value;
                  else
                    return value->state();
                }();
                Add(page.id, setting.id(), setting.label(), location + (section.empty() ? "" : " > " + section),
                    SearchTomlKey(setting.id()));
              }
            },
            item);
    }
  }
  std::vector<const SettingsSearchEntry*> Find(std::string_view query) const
  {
    std::vector<std::string> tokens;
    std::istringstream       input(SearchWords(query.substr(0, QueryLimit)));
    for (std::string token; input >> token;)
      tokens.push_back(std::move(token));
    std::vector<const SettingsSearchEntry*> result;
    if (tokens.empty())
      return result;
    for (const auto& entry : entries_)
      if (std::ranges::all_of(tokens, [&](const auto& token) { return entry.words.find(token) != std::string::npos; }))
        result.push_back(&entry);
    // Exact key/name matches precede broad matches while registration order
    // remains stable within each group.
    const auto exact = SearchWords(query);
    std::stable_partition(result.begin(), result.end(), [&](auto* entry) {
      const auto key = entry->key.substr(entry->key.find('.') + 1);
      return SearchWords(entry->key) == exact || SearchWords(key) == exact || SearchWords(entry->label) == exact;
    });
    return result;
  }

private:
  void Add(std::string page, std::string item, std::string label, std::string location, std::string key)
  {
    auto words = SearchWords(label + " " + key);
    entries_.push_back(
        {std::move(page), std::move(item), std::move(label), std::move(location), std::move(key), std::move(words)});
  }
  std::vector<SettingsSearchEntry> entries_;
};
} // namespace mod_settings
