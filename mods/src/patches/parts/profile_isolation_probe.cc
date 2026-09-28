#if _WIN32

#include "il2cpp/method_contract.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace spud::detail::x64 {
uintptr_t maybe_resolve_jump(uintptr_t);
}

namespace {

std::wstring   profile_id;
std::u16string preference_prefix;

[[noreturn]] void FailClosed(const char* reason)
{
  spdlog::critical("[ProfileIsolationProbe] {}", reason);
  spdlog::default_logger()->flush();
  ExitProcess(190);
  std::abort();
}

std::wstring Environment(const wchar_t* name)
{
  const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
  if (!size)
    return {};
  std::vector<wchar_t> buffer(size);
  const DWORD length = GetEnvironmentVariableW(name, buffer.data(), size);
  if (!length || length >= size)
    FailClosed("Could not read profile environment");
  return {buffer.data(), length};
}

std::wstring KnownFolder(REFKNOWNFOLDERID folder_id)
{
  PWSTR path = nullptr;
  if (FAILED(SHGetKnownFolderPath(folder_id, 0, nullptr, &path)) || !path) {
    CoTaskMemFree(path);
    return {};
  }
  std::wstring result(path);
  CoTaskMemFree(path);
  return result;
}

bool HasForcedEdgeUserDataDir()
{
  for (const auto root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
    DWORD size = 0;
    const auto status = RegGetValueW(root, L"SOFTWARE\\Policies\\Microsoft\\Edge", L"UserDataDir", RRF_RT_ANY,
                                     nullptr, nullptr, &size);
    if (status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND)
      return true;
  }
  return false;
}

bool ReservedWindowsName(std::wstring_view name)
{
  if (name == L"con" || name == L"prn" || name == L"aux" || name == L"nul")
    return true;
  return name.size() == 4 && name[3] >= L'1' && name[3] <= L'9'
         && (name.substr(0, 3) == L"com" || name.substr(0, 3) == L"lpt");
}

Il2CppString* ProfileKey(Il2CppString* key)
{
  if (!key)
    return nullptr;
  std::u16string value = preference_prefix;
  value.append(reinterpret_cast<const char16_t*>(key->chars), key->length);
  auto* mapped = il2cpp_string_new_utf16(reinterpret_cast<const Il2CppChar*>(value.data()),
                                         static_cast<int32_t>(value.size()));
  if (!mapped)
    FailClosed("Could not allocate a profile preference key");
  return mapped;
}

bool TrySetInt_Hook(auto original, Il2CppString* key, int value)
{
  return original(ProfileKey(key), value);
}

bool TrySetFloat_Hook(auto original, Il2CppString* key, float value)
{
  return original(ProfileKey(key), value);
}

bool TrySetString_Hook(auto original, Il2CppString* key, Il2CppString* value)
{
  return original(ProfileKey(key), value);
}

int GetInt_Hook(auto original, Il2CppString* key, int fallback)
{
  return original(ProfileKey(key), fallback);
}

float GetFloat_Hook(auto original, Il2CppString* key, float fallback)
{
  return original(ProfileKey(key), fallback);
}

Il2CppString* GetString_Hook(auto original, Il2CppString* key, Il2CppString* fallback)
{
  return original(ProfileKey(key), fallback);
}

bool HasKey_Hook(auto original, Il2CppString* key)
{
  return original(ProfileKey(key));
}

void DeleteKey_Hook(auto original, Il2CppString* key)
{
  original(ProfileKey(key));
}

void DeleteAll_Hook(auto)
{
  FailClosed("The client attempted an unscoped preference reset");
}

bool LaunchProfileBrowser(Il2CppString* url)
{
  if (!url)
    return false;
  std::wstring address(reinterpret_cast<const wchar_t*>(url->chars), url->length);
  if (!address.starts_with(L"https://") || address.find_first_of(L"\"\r\n\t ") != std::wstring::npos)
    return false;

  if (HasForcedEdgeUserDataDir())
    return false;
  const auto local_app_data = KnownFolder(FOLDERID_LocalAppData);
  if (local_app_data.empty())
    return false;
  std::filesystem::path browser;
  for (const auto* folder_id : {&FOLDERID_ProgramFilesX86, &FOLDERID_ProgramFiles, &FOLDERID_LocalAppData}) {
    const auto base = KnownFolder(*folder_id);
    if (base.empty())
      continue;
    const auto candidate = std::filesystem::path(base) / L"Microsoft" / L"Edge" / L"Application" / L"msedge.exe";
    if (std::filesystem::is_regular_file(candidate)) {
      browser = candidate;
      break;
    }
  }
  if (browser.empty())
    return false;
  const auto data_dir = std::filesystem::path(local_app_data) / L"STFC Community Mod" / L"BrowserProfiles" / profile_id;
  std::filesystem::create_directories(data_dir);

  std::wstring command = L"\"" + browser.wstring() + L"\" --user-data-dir=\"" + data_dir.wstring()
                         + L"\" --new-window \"" + address + L"\"";
  STARTUPINFOW startup{sizeof(startup)};
  PROCESS_INFORMATION process{};
  const bool started = CreateProcessW(browser.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                                      &startup, &process) != 0;
  if (started) {
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
  }
  return started;
}

bool PresentUrl_Hook(auto, void*, Il2CppString* url)
{
  try {
    if (LaunchProfileBrowser(url))
      return true;
  } catch (...) {
  }
  FailClosed("Could not open the isolated sign-in browser");
}

void* Resolve(Il2CppClass* cls, const char* name, const char* result, std::initializer_list<const char*> args)
{
  return method_contract::Pointer(method_contract::Resolve(cls, name, true, result, args));
}

} // namespace

bool IsolatedProfileRequested()
{
  SetLastError(ERROR_SUCCESS);
  const DWORD size = GetEnvironmentVariableW(L"STFC_MOD_ISOLATED_PROFILE", nullptr, 0);
  return size != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
}

[[noreturn]] void AbortIsolatedProfileLaunch()
{
  TerminateProcess(GetCurrentProcess(), 190);
  std::abort();
}

void InstallProfileIsolationProbe()
{
  if (!IsolatedProfileRequested())
    return;
  profile_id = Environment(L"STFC_MOD_ISOLATED_PROFILE");
  if (profile_id.empty())
    FailClosed("Empty profile ID");
  if (profile_id.size() > 32)
    FailClosed("Invalid profile ID");
  for (const wchar_t ch : profile_id)
    if (!((ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_'))
      FailClosed("Invalid profile ID");
  if (ReservedWindowsName(profile_id))
    FailClosed("Reserved profile ID");

  preference_prefix = u"stfc-mod/profile/";
  for (const wchar_t ch : profile_id)
    preference_prefix.push_back(static_cast<char16_t>(ch));
  preference_prefix.push_back(u'/');

  auto prefs = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "PlayerPrefs");
  auto oidc = il2cpp_get_class_helper("Playgami.Sdk.Identity.Runtime", "Playgami.Identity.Api.Internal",
                                     "OidcAuthorizer");
  if (!prefs.get_cls() || !oidc.get_cls())
    FailClosed("Required profile classes are unavailable");

  const std::array<void*, 9> pref_methods = {
      Resolve(prefs.get_cls(), "TrySetInt", "System.Boolean", {"System.String", "System.Int32"}),
      Resolve(prefs.get_cls(), "TrySetFloat", "System.Boolean", {"System.String", "System.Single"}),
      Resolve(prefs.get_cls(), "TrySetSetString", "System.Boolean", {"System.String", "System.String"}),
      Resolve(prefs.get_cls(), "GetInt", "System.Int32", {"System.String", "System.Int32"}),
      Resolve(prefs.get_cls(), "GetFloat", "System.Single", {"System.String", "System.Single"}),
      Resolve(prefs.get_cls(), "GetString", "System.String", {"System.String", "System.String"}),
      Resolve(prefs.get_cls(), "HasKey", "System.Boolean", {"System.String"}),
      Resolve(prefs.get_cls(), "DeleteKey", "System.Void", {"System.String"}),
      Resolve(prefs.get_cls(), "DeleteAll", "System.Void", {}),
  };
  const std::array<void*, 3> browser_methods = {
      method_contract::Pointer(method_contract::Resolve(oidc.get_cls(), "PresentLoginUrlToUser", false,
                                                        "System.Boolean", {"System.String"})),
      method_contract::Pointer(method_contract::Resolve(oidc.get_cls(), "PresentLogoutUrlToUser", false,
                                                        "System.Boolean", {"System.String"})),
      method_contract::Pointer(method_contract::Resolve(oidc.get_cls(), "PresentLinkUrlToUser", false,
                                                        "System.Boolean", {"System.String"})),
  };
  for (auto* method : pref_methods)
    if (!method)
      FailClosed("A preference method is unavailable");
  for (auto* method : browser_methods)
    if (!method)
      FailClosed("A browser handoff method is unavailable");

  // The pinned SPUD installer resolves x64 jump thunks before patching.
  static_assert(sizeof(void*) == 8);
  const auto canonical_target = [](void* method) {
    return spud::detail::x64::maybe_resolve_jump(reinterpret_cast<uintptr_t>(method));
  };
  std::array<uintptr_t, pref_methods.size() + browser_methods.size()> targets{};
  std::transform(pref_methods.begin(), pref_methods.end(), targets.begin(), canonical_target);
  std::transform(browser_methods.begin(), browser_methods.end(), targets.begin() + pref_methods.size(),
                 canonical_target);
  for (size_t i = 0; i < targets.size(); ++i)
    for (size_t j = i + 1; j < targets.size(); ++j)
      if (targets[i] == targets[j])
        FailClosed("Profile hook methods share a native target");

  try {
    if (!SPUD_STATIC_DETOUR(pref_methods[0], TrySetInt_Hook)
        || !SPUD_STATIC_DETOUR(pref_methods[1], TrySetFloat_Hook)
        || !SPUD_STATIC_DETOUR(pref_methods[2], TrySetString_Hook)
        || !SPUD_STATIC_DETOUR(pref_methods[3], GetInt_Hook)
        || !SPUD_STATIC_DETOUR(pref_methods[4], GetFloat_Hook)
        || !SPUD_STATIC_DETOUR(pref_methods[5], GetString_Hook)
        || !SPUD_STATIC_DETOUR(pref_methods[6], HasKey_Hook)
        || !SPUD_STATIC_DETOUR(pref_methods[7], DeleteKey_Hook)
        || !SPUD_STATIC_DETOUR(pref_methods[8], DeleteAll_Hook)
        || !SPUD_STATIC_DETOUR(browser_methods[0], PresentUrl_Hook)
        || !SPUD_STATIC_DETOUR(browser_methods[1], PresentUrl_Hook)
        || !SPUD_STATIC_DETOUR(browser_methods[2], PresentUrl_Hook))
      FailClosed("A profile hook could not be installed");
  } catch (...) {
    FailClosed("A profile hook failed to install");
  }
  spdlog::info("[ProfileIsolationProbe] Active for a dedicated launch profile");
}

#endif
