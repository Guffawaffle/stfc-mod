#pragma once

struct SettingsSectionDirector;
struct SettingsContext;
namespace mod_settings::native
{
// The native AddGeneralSettings hook has one terminal owner; page restoration appends through this seam.
using PageAppender = void (*)(SettingsSectionDirector*, SettingsContext*);
bool RegisterPageAppender(PageAppender appender);
} // namespace mod_settings::native
