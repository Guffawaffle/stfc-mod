#include "maintenance_pages.h"
#include "file.h"
#include "maintenance.h"
#include "native_controls.h"
#include "runtime_config.h"
#include <algorithm>
#include <memory>
#include <set>

namespace mod_settings
{
namespace
{
  constexpr auto results_id = "community_mod.search.results";
  constexpr auto notice_id  = "community_mod.save_notice";
  struct MaintenanceState {
    SettingsSearch                              index;
    std::vector<SettingsSearchEntry>            matches;
    std::vector<PageCatalog::Page>              pages;
    std::vector<std::shared_ptr<ActionSetting>> commands;
    std::string                                 root, query, navigation_error;
    std::uint64_t                               revision   = 0;
    bool                                        registered = false;
    ActionSetting*                              notice     = nullptr;
  };
  MaintenanceState& Tools()
  {
    static auto* state = new MaintenanceState;
    return *state;
  }
  ActionSetting& KeepCommand(PageCatalog& catalog, std::string_view page, std::shared_ptr<ActionSetting> command)
  {
    auto& result = *command;
    catalog.AddAction(page, result);
    Tools().commands.push_back(std::move(command));
    return result;
  }
  std::string ConfigLocation()
  {
    const auto path = File::Path(File::Config(), false).u8string();
    return std::string(reinterpret_cast<const char*>(path.data()), path.size());
  }
  void RegisterConfigurationHelp(PageCatalog& catalog)
  {
    const auto& pages = catalog.RegisteredPages();
    const auto  page  = std::ranges::find(pages, "community_mod.advanced", &PageCatalog::Page::id);
    if (page == pages.end())
      return;
    auto location      = std::make_shared<ActionSetting>();
    location->identity = "community_mod.configuration.location";
    location->label    = "Settings file";
    location->read     = [](std::size_t) {
      try {
        return ActionSetting::Presentation{"Settings file", "Details", ConfigLocation()};
      } catch (...) {
        return ActionSetting::Presentation{"Settings file", "", "Path unavailable", false};
      }
    };
    location->invoke = [](std::size_t) {
      const auto path = ConfigLocation();
      OpenTextEntry(
          {.title = "Settings file",
           .value = path,
           .help = "This is the settings file selected by the current launch/profile. Saving a setting keeps unrelated "
                   "comments and values. Settings reload after external edits; pending or session-only changes retain "
                   "their "
                   "existing conflict rules. This dialog does not move files or change the active profile.",
           .commit      = [](std::string_view) { return true; },
           .read_only   = true,
           .apply_label = "Close"});
    };
    KeepCommand(catalog, page->id, std::move(location));
    auto help      = std::make_shared<ActionSetting>();
    help->identity = "community_mod.configuration.help";
    help->label    = "Configuration help";
    help->read     = [](std::size_t) { return ActionSetting::Presentation{"Configuration help", "Read", ""}; };
    help->invoke   = [](std::size_t) {
      OpenTextEntry(
          {.title = "Configuration help",
           .value = "",
           .help =
               "Live settings apply immediately. Some settings require reopening the affected screen or restarting "
               "the client; their controls and Defaults & Help report the timing. The emergency hook switches under "
               "[patches] require a restart. Advanced settings not listed here remain in TOML.\n\n"
               "A setting can be applied for this session while its save is pending, failed, conflicted, or "
               "unavailable. "
               "See its Defaults & Help entry for the actual save status and an explicit retry. Credentials and "
               "account preference contents are not displayed here.",
           .commit      = [](std::string_view) { return true; },
           .read_only   = true,
           .apply_label = "Close"});
    };
    KeepCommand(catalog, page->id, std::move(help));
  }
  void QualifyMaintenanceDialogs(PageCatalog& catalog, std::size_t first)
  {
    const auto& pages = catalog.RegisteredPages();
    for (std::size_t i = first; i < pages.size(); ++i)
      for (auto* action : pages[i].Controls<ActionSetting>()) {
        if (action->id().ends_with(".retry"))
          continue;
        const auto invoke = action->invoke;
        const auto read   = action->read;
        auto       error  = std::make_shared<std::string>();
        action->read      = [read, error](std::size_t index) {
          auto result = read(index);
          if (!error->empty())
            result.value = *error;
          return result;
        };
        action->invoke = [invoke, error](std::size_t index) {
          error->clear();
          if (TextEntryActive()) {
            *error = "Close the current editor before opening another.";
            native::RequestPageRefresh();
            return;
          }
          try {
            invoke(index);
          } catch (...) {
            *error = "Details or reset are unavailable; no success was confirmed.";
          }
          if (error->empty() && !TextEntryActive())
            *error = "The native editor could not open. Reopen Settings and try again.";
          native::RequestPageRefresh();
        };
      }
  }
} // namespace

const std::string& SettingsSearchQuery()
{ return Tools().query; }
std::uint64_t SettingsSearchRevision()
{ return Tools().revision; }
void SetSettingsSearchError(std::string text)
{
  Tools().navigation_error = std::move(text);
  native::RequestPageRefresh();
}
void SetSettingsSearchQuery(std::string text)
{
  if (text.size() > SettingsSearch::QueryLimit)
    text.resize(SettingsSearch::QueryLimit);
  if (text == Tools().query)
    return;
  Tools().query = std::move(text);
  Tools().matches.clear();
  for (const auto* match : Tools().index.Find(Tools().query))
    Tools().matches.push_back(*match);
  Tools().navigation_error.clear();
  ++Tools().revision;
  native::RequestPageRefresh();
}
bool FilterMaintenanceSearch(const PageCatalog::Page& page, std::string_view item)
{ return page.id != Tools().root || Tools().query.empty() || item == results_id || item == notice_id; }
void RegisterMaintenanceTools(PageCatalog& catalog)
{
  auto& state = Tools();
  if (state.registered || catalog.RegisteredPages().empty())
    return;
  state.registered = true;
  state.root       = catalog.RegisteredPages().front().id;
  RegisterConfigurationHelp(catalog);
  const auto before = catalog.RegisteredPages().size();
  RegisterMaintenancePages(catalog); // Preserved prepare/current/apply/reset and per-key retry contracts.
  QualifyMaintenanceDialogs(catalog, before);

  // The native root-page header owns search input; only its results need rows.
  auto results      = std::make_shared<ActionSetting>();
  results->identity = results_id;
  results->label    = "Search results";
  results->count    = [] {
    if (Tools().query.empty())
      return std::size_t(!Tools().navigation_error.empty());
    return 1 + std::min(Tools().matches.size(), SettingsSearch::ResultLimit);
  };
  results->read = [](std::size_t index) {
    if (index == 0) {
      auto label = Tools().matches.empty() ? std::string("No matching settings")
                                           : std::to_string(Tools().matches.size()) + " results";
      if (Tools().matches.size() > SettingsSearch::ResultLimit)
        label += " (showing first 32; narrow the search)";
      const bool empty = Tools().query.empty();
      return ActionSetting::Presentation{empty ? "Search unavailable" : std::move(label), empty ? "Dismiss" : "Clear",
                                         Tools().navigation_error};
    }
    if (index > Tools().matches.size())
      return ActionSetting::Presentation{"", "", "", false, false};
    const auto& hit    = Tools().matches[index - 1];
    const auto  page   = std::ranges::find(Tools().pages, hit.page, &PageCatalog::Page::id);
    const bool  hidden = page != Tools().pages.end() && !hit.item.empty() && !page->IsVisible(hit.item);
    return ActionSetting::Presentation{hit.label + "\n<size=65%>" + hit.location
                                           + (hidden ? " (hidden by another setting)" : "") + "</size>",
                                       "Open", hit.key};
  };
  results->invoke = [](std::size_t index) {
    if (index == 0) {
      Tools().navigation_error.clear();
      SetSettingsSearchQuery({});
      native::RequestPageRefresh(); // Dismiss must refresh even when the query was already empty.
      return;
    }
    if (index > Tools().matches.size())
      return;
    const auto hit = Tools().matches[index - 1]; // Copy before a callback can change the query or result list.
    if (!native::QueueSettingsPage(hit.page, hit.item, true)) {
      Tools().navigation_error = "This setting could not be opened in the current Settings context.";
      native::RequestPageRefresh();
    }
  };
  KeepCommand(catalog, state.root, std::move(results));
  auto notice      = std::make_shared<ActionSetting>();
  notice->identity = notice_id;
  notice->label    = "Save notice";
  notice->read     = [](std::size_t) {
    return ActionSetting::Presentation{"Some changes were not saved. See Defaults & Help.", "", "", false,
                                       runtime_config::HasSaveFailures()};
  };
  notice->invoke = [](std::size_t) {};
  state.notice   = notice.get();
  state.commands.push_back(std::move(notice));
  state.pages = catalog.RegisteredPages();
  state.index.Build(state.pages); // Metadata only: never Observe/apply a setting or inspect preference contents.
}
void AppendMaintenanceNotices(std::vector<PageCatalog::Page>& pages)
{
  if (!Tools().notice)
    return;
  for (auto& page : pages)
    page.items.insert(page.items.begin(), Tools().notice); // After empty-page pruning: a notice creates no empty group.
}
} // namespace mod_settings
