#pragma once

#include "page_catalog.h"
#include <cstddef>
#include <optional>
#include <span>
#include <string>

struct SettingsSectionDirector;
struct SettingsContext;
struct SettingsContainerContext;
struct SettingsOptionContext;
struct SettingsPageController;
namespace mod_settings::native
{
// The native AddGeneralSettings hook has one terminal owner; page restoration appends through this seam.
using PageAppender = void (*)(SettingsSectionDirector*, SettingsContext*);
bool RegisterPageAppender(PageAppender appender);
using BooleanResolver = BooleanSetting* (*)(SettingsOptionContext*);
using PageInvalidator = void (*)();
bool RegisterBooleanResolver(BooleanResolver resolver);
bool RegisterPageInvalidator(PageInvalidator invalidator);
bool AppendBooleanRow(SettingsSectionDirector* director, SettingsContext* context, SettingsContainerContext* parent,
                      BooleanSetting& setting);
BooleanSetting* OwnedBooleanSetting(SettingsOptionContext* context);

enum class PageItemKind : std::size_t { Choice, Slider, Action, Count };
struct PageItemIdentity {
  std::string id; // Canonical catalog setting ID, not a native label or a choice's individual row ID.
  std::size_t index   = 0;
  bool        visible = true;
};
using PageItemAppender = bool (*)(SettingsSectionDirector*, SettingsContext*, SettingsContainerContext*,
                                  const PageCatalog::Item&);
using PageItemResolver = std::optional<PageItemIdentity> (*)(SettingsOptionContext*);
// Each control family owns its native callback/widget hooks. Register before NativeSettingsPages installation.
// Appending runs off-screen; false aborts publication of that tree. Resolvers must prove callback ownership.
bool RegisterPageItemAdapter(PageItemKind kind, PageItemAppender append, PageItemResolver resolve,
                             PageInvalidator invalidate = nullptr);
// Change observers request a later refresh; never rebind/release the requesting row within its own setter.
void RequestPageRefresh();
using PageVisitToken = uint64_t;
PageVisitToken CurrentPageVisit();
bool           IsPageVisitCurrent(PageVisitToken token);
bool           PageRefreshInProgress();
using PageVisitObserver = void (*)(SettingsPageController*, SettingsContainerContext*, std::string_view, PageVisitToken,
                                   bool entering);
bool                               RegisterPageVisitObserver(PageVisitObserver observer);
std::span<const PageCatalog::Page> NativeSettingsPages();
bool CurrentSettingsPage(SettingsPageController*& controller, SettingsContainerContext*& context,
                         std::string_view& page);
// Deferred until the native row callback has returned; targets remain within the same published tree.
bool QueueSettingsPage(std::string_view page, std::string_view item, bool return_to_current = false);
bool SettingsSearchAvailable();
bool FocusSettingsSearch();
bool AllowsSettingsSearchAction(std::string_view id);
enum class PageBindResult { Bound, Unavailable, Stale };
PageBindResult BindPageRows(SettingsPageController* controller, SettingsContainerContext* expected,
                            std::span<SettingsOptionContext* const> rows);
} // namespace mod_settings::native
