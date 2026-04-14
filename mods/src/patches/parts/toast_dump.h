#pragma once

#include <il2cpp/il2cpp_helper.h>

// Call once at init — resolves LanguageManager::Localize internally.
void toast_dump_setup();

// Process a toast: resolve text, build battle summary, fire Windows notification.
void toast_handle_notification(Il2CppObject* toast);
