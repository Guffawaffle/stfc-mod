#include "settings/maintenance_pages.h"
#include "settings/native_controls.h"
#include "settings/text_entry.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace
{
bool                                          save_failure = false, editor_available = true, search_available = true;
unsigned                                      refreshes = 0, focus_calls = 0, path_reads = 0;
std::string                                   opened_page, opened_item;
std::optional<mod_settings::TextEntryRequest> editor;
void                                          Check(bool value, const char* message)
{
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
mod_settings::ActionSetting& Action(const std::vector<mod_settings::PageCatalog::Page>& pages, std::string_view id)
{
  for (const auto& page : pages)
    for (auto* action : page.Controls<mod_settings::ActionSetting>())
      if (action->id() == id)
        return *action;
  std::cerr << "Missing action: " << id << '\n';
  std::exit(1);
}
} // namespace
namespace runtime_config
{
bool HasSaveFailures() noexcept
{ return save_failure; }
} // namespace runtime_config
namespace File
{
const std::string& Config()
{
  static const std::string path = "/fixture/profiles/catalog/settings.toml";
  return path;
}
std::filesystem::path Path(std::string_view name, bool create)
{
  Check(!create, "configuration help never creates or moves a settings location");
  ++path_reads;
  return name;
}
} // namespace File
namespace mod_settings
{
bool OpenTextEntry(TextEntryRequest request)
{
  if (!editor_available)
    return false;
  editor = std::move(request);
  return true;
}
void CloseTextEntry()
{ editor.reset(); }
bool TextEntryActive()
{ return bool(editor); }
namespace native
{
  void RequestPageRefresh()
  { ++refreshes; }
  bool SettingsSearchAvailable()
  { return search_available; }
  bool FocusSettingsSearch()
  {
    ++focus_calls;
    return search_available;
  }
  bool QueueSettingsPage(std::string_view page, std::string_view item, bool returning)
  {
    Check(returning, "search result requests the same-context Back return");
    opened_page = page;
    opened_item = item;
    return true;
  }
} // namespace native
} // namespace mod_settings

int main()
{
  using namespace mod_settings;
  unsigned              reads = 0, writes = 0, saves = 0;
  bool                  live = true;
  float                 zoom = 15.f;
  SaveStatus            boolean_save, number_save;
  ValueDefinition<bool> toggle_definition{"toggle", "Camera toggle",
                                          [&] {
                                            ++reads;
                                            return ReadResult::Known(live, 1);
                                          },
                                          [&](bool value, uint64_t) {
                                            ++writes;
                                            live = value;
                                            return ApplyResult::Applied;
                                          }};
  toggle_definition.metadata      = {"ui", "toggle", "Camera controls"};
  toggle_definition.default_value = false;
  toggle_definition.persist       = [&](bool) {
    ++saves;
    boolean_save.state = SaveState::Pending;
  };
  toggle_definition.save_status = [&](bool) { return boolean_save; };
  BooleanSetting         toggle(std::move(toggle_definition));
  ValueDefinition<float> number_definition{"zoom", "Zoom speed",
                                           [&] {
                                             ++reads;
                                             return ValueReadResult<float>::Known(zoom, 1);
                                           },
                                           [&](float value, uint64_t) {
                                             ++writes;
                                             zoom = value;
                                             return ApplyResult::Applied;
                                           }};
  number_definition.metadata      = {"graphics", "zoom_speed", "Camera zoom rate", "ratio"};
  number_definition.default_value = 2.f;
  number_definition.persist       = [&](float) {
    ++saves;
    number_save.state = SaveState::Pending;
  };
  number_definition.save_status = [&](float) { return number_save; };
  ValueSetting<float> canonical(number_definition);
  SliderSetting       slider(std::move(number_definition), 0, 10, .1f, [] { return true; });
  ActionSetting       exact{"zoom.exact", "Zoom speed",
                            [](std::size_t) { return ActionSetting::Presentation{"Zoom speed", "Edit", ""}; },
                            [](std::size_t) {}};
  exact.metadata      = &canonical.metadata();
  exact.prepare_reset = [&] { return PrepareReset(canonical); };
  exact.retry         = [&] { canonical.RetrySave(canonical.Observe()); };
  exact.status        = [&] { return canonical.status_text(); };
  exact.default_label = [] { return "2"; };
  exact.save_status   = [&] { return canonical.save_status(); };
  PageCatalog catalog("community_mod.settings", "Mod Settings");
  catalog.AddPage("camera", "Camera", "community_mod.settings");
  catalog.AddPage("empty", "Empty", "community_mod.settings");
  catalog.AddPage("community_mod.advanced", "Advanced & Configuration", "community_mod.settings");
  catalog.AddBoolean("camera", toggle);
  catalog.AddSlider("camera", slider);
  catalog.AddAction("camera", exact);
  RegisterMaintenanceTools(catalog);
  RegisterMaintenanceTools(catalog);
  Check(reads == 0 && writes == 0 && saves == 0 && path_reads == 0,
        "registration indexes metadata without reading settings, preferences, files or routes");
  auto pages = catalog.Build();
  Check(std::ranges::find(pages, "empty", &PageCatalog::Page::id) == pages.end(),
        "failure notices do not resurrect empty pages");
  AppendMaintenanceNotices(pages);
  const auto defaults = std::ranges::find(pages, "camera.defaults", &PageCatalog::Page::id);
  Check(defaults != pages.end() && defaults->items.size() == 6,
        "maintenance deduplicates slider/exact metadata and adds one real save notice after pruning");
  auto& reset = Action(pages, "camera.defaults.reset");
  reset.invoke(0);
  Check(editor && editor->read_only && writes == 0 && saves == 0,
        "page Reset prepares a confirmation without applying");
  zoom = 8.f;
  Check(!editor->commit("") && writes == 0 && saves == 0,
        "all reset plans are revalidated before any mutation when one captured setting changed");
  CloseTextEntry();
  reset.invoke(0);
  Check(editor && editor->commit("") && !live && zoom == 2 && writes == 2 && saves == 2,
        "available page reset applies the canonical full-range models and their existing save policies");
  CloseTextEntry();
  number_save.state = SaveState::Failed;
  auto& retry       = Action(pages, "camera.defaults.graphics.zoom_speed.retry");
  Check(retry.Read(0).visible, "failed persistence exposes an explicit real retry command");
  retry.invoke(0);
  Check(writes == 2 && saves == 3 && number_save.state == SaveState::Pending,
        "Retry resubmits the current canonical value without an extra feature mutation");
  editor_available = false;
  reset.invoke(0);
  Check(!editor && reset.Read(0).value.find("could not open") != std::string::npos && writes == 2,
        "unavailable native dialogs expose failure instead of claiming reset success");

  auto& search = Action(pages, "community_mod.search.focus");
  search.invoke(0);
  Check(focus_calls == 1, "Search focuses the native field instead of being a no-op menu item");
  SetSettingsSearchQuery("graphics.zoom_speed");
  auto& results = Action(pages, "community_mod.search.results");
  Check(results.Count() == 2 && results.Read(1).value == "graphics.zoom_speed",
        "search indexes exact TOML names and deduplicates one setting's two representations");
  Check(!FilterMaintenanceSearch(pages.front(), "camera")
            && FilterMaintenanceSearch(pages.front(), "community_mod.search.results"),
        "active root search shows result commands while preserving the native category ownership tree");
  results.invoke(1);
  Check(opened_page == "camera" && opened_item == "zoom", "the actual result queues its canonical page/item target");
  results.invoke(0);
  Check(SettingsSearchQuery().empty() && results.Count() == 0 && FilterMaintenanceSearch(pages.front(), "camera"),
        "Clear restores normal navigation without changing settings");
  save_failure      = true;
  const auto notice = Action(pages, "community_mod.save_notice").Read(0);
  Check(notice.visible && !notice.actionable() && notice.label.find("not saved") != std::string::npos,
        "save notice reports actual owner failure and offers no fake action");
  const auto location = Action(pages, "community_mod.configuration.location").Read(0);
  Check(location.value.find("/fixture/profiles/catalog/settings.toml") != std::string::npos && path_reads == 1,
        "configuration help reads only the launch's existing scoped path on explicit presentation");
  Check(writes == 2 && saves == 3 && refreshes != 0, "search/help/status presentation never silently writes settings");
  std::cout << "PASS actual maintenance/search catalog, canonical reset/retry, routing and failure presentation\n";
}
