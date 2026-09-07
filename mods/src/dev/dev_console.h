#pragma once

struct ScreenManager;

#ifdef _MODDBG
void InstallDevConsole();
bool dev_console_update(ScreenManager* screen_manager);
#else
inline void InstallDevConsole() {}
inline bool dev_console_update(ScreenManager*) { return false; }
#endif
