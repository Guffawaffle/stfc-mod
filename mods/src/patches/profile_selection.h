#pragma once

#if _WIN32

#include <filesystem>
#include <string>

struct ProfileSelection {
  bool                  active = false;
  bool                  marked = false;
  bool                  enroll = false;
  std::wstring          id;
  std::filesystem::path config_path;
};

const ProfileSelection& ResolveProfileSelection();
void                    CompleteProfileEnrollment();
[[noreturn]] void       AbortProfileLaunch(const char* reason);

#endif
