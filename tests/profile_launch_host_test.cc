// Windows host-only fixture: real public Profiles headers and the actual host source,
// with fake component implementations and a fresh process/disposable directory per case.
// From a VS developer shell (JSON_INCLUDE contains nlohmann/json.hpp):
// clang++ --driver-mode=cl /std:c++latest /EHsc /MT /DNOMINMAX /DSTFC_PROFILES_ENABLED=1
//   /I D:/dev/stfc-profiles/include /I JSON_INCLUDE tests/profile_launch_host_test.cc
//   /Fe:profile-launch-host-tests.exe /link shell32.lib user32.lib
// Run profile-launch-host-tests.exe; repeat with STFC_PROFILES_ENABLED=0 for ordinary-build refusal.

#ifndef STFC_PROFILES_ENABLED
#define STFC_PROFILES_ENABLED 1
#endif

#include <Windows.h>

#include <stfc_profiles/catalog.h>
#include <stfc_profiles/community_mod_adapter.h>
#include <stfc_profiles/installation.h>
#include <stfc_profiles/prefs_store.h>
#include <stfc_profiles/session.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace profile_host_test
{
namespace fs                   = std::filesystem;
constexpr std::string_view kId = "0123456789abcdef0123456789abcdef";
fs::path                   root;
std::string                scenario;
int                        installations = 0, sessions = 0, admissions = 0, preparations = 0, installs = 0, ready = 0;
stfc::profiles::SessionLease* prepared_session = nullptr;

void Check(bool condition, std::string_view message)
{
  if (!condition)
    throw std::runtime_error(std::string(message));
}

void Event(std::string_view event)
{
  std::ofstream file(root / "events", std::ios::app);
  file.exceptions(std::ios::failbit | std::ios::badbit);
  file << event << '\n';
}

int WINAPI SuppressDialog(HWND, LPCSTR, LPCSTR, UINT)
{
  Event("stop");
  return IDOK;
}
} // namespace profile_host_test

// Keep the real termination path. Only its blocking error dialog is suppressed.
#define MessageBoxA profile_host_test::SuppressDialog
#include "../src/mod/profile_launch.cc"
#undef MessageBoxA

namespace stfc::profiles
{
struct InstallationLease::Impl {
  std::filesystem::path root, directory;
};

InstallationLease::InstallationLease(const std::filesystem::path& supplied_root, const std::filesystem::path& game,
                                     bool exclusive, bool observation_only)
{
  using namespace profile_host_test;
  Check(supplied_root == root && !exclusive && !observation_only, "unexpected installation admission");
  Check(std::filesystem::is_directory(game), "host did not resolve its real executable directory");
  Event("installation");
  if (scenario == "installation-failure")
    throw std::runtime_error("synthetic installation failure");
  impl_ = std::make_unique<Impl>(Impl{supplied_root, game});
  ++installations;
}

InstallationLease::~InstallationLease()
{
  --profile_host_test::installations;
  profile_host_test::Event("release-installation");
}

const std::filesystem::path& InstallationLease::Directory() const
{ return impl_->directory; }

void CheckInstallationReady(const std::filesystem::path& supplied_root, const std::filesystem::path& game)
{
  using namespace profile_host_test;
  Check(supplied_root == root && installations == 1 && std::filesystem::is_directory(game),
        "installation lease was not retained through readiness inspection");
  Event("check-installation");
  if (scenario == "installation-not-ready")
    throw std::runtime_error("synthetic incomplete installation");
}

std::filesystem::path DefaultCatalogRoot()
{
  profile_host_test::Check(!profile_host_test::root.empty(), "fixture root was not supplied");
  return profile_host_test::root;
}

struct SessionLease::Impl {
  std::filesystem::path root, directory;
  std::string           id;
};

SessionLease::SessionLease(const std::filesystem::path& supplied_root, std::string_view id)
{
  using namespace profile_host_test;
  Check(supplied_root == root && id == kId && installations == 1 && sessions == 0,
        "session admission lost its selected ID or installation lease");
  Event("session");
  if (scenario == "session-failure")
    throw std::runtime_error("synthetic session conflict");
  impl_ = std::make_unique<Impl>(Impl{supplied_root, supplied_root / "profiles" / id, std::string(id)});
  ++sessions;
}

SessionLease::~SessionLease()
{
  --profile_host_test::sessions;
  profile_host_test::Event("release-session");
}

const std::filesystem::path& SessionLease::Directory() const
{ return impl_->directory; }

bool SessionLease::PreferencesInitialized() const
{ return profile_host_test::scenario == "existing-metadata"; }

void SessionLease::MarkReady()
{
  using namespace profile_host_test;
  Check(installations == 1 && sessions == 1 && installs == 1 && ready == 0,
        "readiness was published outside the mandatory adapter installation");
  ++ready;
  Event("ready");
}

void SessionLease::MarkFailed(std::string_view)
{
  using namespace profile_host_test;
  Check(installations == 1 && sessions == 1 && ready == 0, "failed request lost admission or was already ready");
  Event("failed");
}

namespace community_mod
{
  void InstallProcessAdmission(std::string_view id)
  {
    using namespace profile_host_test;
    Check(id == kId && installations == 1 && sessions == 0 && admissions == 0,
          "process admission used the wrong ID or ran more than once");
    ++admissions;
    Event("admission");
    if (scenario == "admission-failure")
      throw std::runtime_error("synthetic process admission failure");
  }

  void PrepareProfile(const std::filesystem::path& supplied_root, std::string_view id, ProfileOpenMode mode,
                      SessionLease& lease)
  {
    using namespace profile_host_test;
    Check(supplied_root == root && id == kId && installations == 1 && sessions == 1 && preparations == 0,
          "profile preparation did not retain its selected session");
    Check(lease.Directory() == root / "profiles" / id, "profile preparation resolved the wrong directory");
    ++preparations;
    prepared_session = &lease;
    const auto name  = mode == ProfileOpenMode::New ? "New" : mode == ProfileOpenMode::Resume ? "Resume" : "Existing";
    Event(std::string("prepare:") + name);
    if (scenario == "prepare-failure")
      throw std::runtime_error("synthetic store preparation failure");
    if (scenario == "prepare-unknown")
      throw 7;
  }

  void InstallProfileHooks()
  {
    using namespace profile_host_test;
    Check(installations == 1 && sessions == 1 && preparations == 1 && installs == 0 && ready == 0,
          "mandatory installation ran without prepared, retained admission");
    ++installs;
    Event("install");
    if (scenario == "install-failure")
      throw std::runtime_error("synthetic mandatory hook failure");
    if (scenario == "install-unknown")
      throw 7;
    prepared_session->MarkReady();
  }
} // namespace community_mod
} // namespace stfc::profiles

namespace profile_host_test
{
void Child()
{
  const auto directory = root / "profiles" / kId;
  fs::create_directories(directory);
  if (scenario == "resume" || scenario == "existing-marker-and-bin")
    std::ofstream(directory / "player_prefs.bin") << "synthetic sentinel";
  if (scenario == "existing-marker" || scenario == "existing-marker-and-bin")
    std::ofstream(directory / "player_prefs.bin.initialized") << "synthetic sentinel";

  profile_launch::Initialize();
  profile_launch::Initialize(); // Repeated initialization must not acquire another lease or install admission again.
  if (scenario == "ordinary") {
    Check(!profile_launch::Requested(), "ordinary launch selected isolation");
    profile_launch::Prepare();
    profile_launch::Install();
    Check(sessions == 0 && admissions == 0 && preparations == 0 && installs == 0 && ready == 0,
          "ordinary launch entered profile isolation");
    Check(installations == (STFC_PROFILES_ENABLED ? 1 : 0), "ordinary installation access was not retained");
    Event("ordinary");
    return;
  }

  Check(profile_launch::Requested(), "valid explicit selector was lost");
  if (scenario == "directory-before-admission")
    (void)profile_launch::Directory();
  profile_launch::Prepare();
  Check(profile_launch::Directory() == directory, "host returned another profile directory");
  Check(installations == 1 && sessions == 1 && ready == 0, "admission was released or marked ready before hooks");
  if (scenario == "duplicate-prepare")
    profile_launch::Prepare();
  profile_launch::Install();
  Check(installations == 1 && sessions == 1 && ready == 1, "admission was not retained after readiness");
  Event("named");
}

struct Case {
  std::string               name;
  std::vector<std::wstring> arguments;
  DWORD                     exit_code;
  std::vector<std::string>  events;
  bool                      releases = false;
};

std::vector<Case> Cases()
{
  const std::wstring              id(kId.begin(), kId.end());
  const std::vector<std::wstring> named{L"-stfc-profile", id};
  std::vector<Case>               cases;
#if STFC_PROFILES_ENABLED
  const std::vector<std::string> admitted{"installation", "check-installation", "admission"};
  cases.push_back({"ordinary", {}, 0, {"installation", "check-installation", "ordinary"}, true});
  for (const auto& [name, mode] :
       std::vector<std::pair<std::string, std::string>>{{"new", "New"},
                                                        {"resume", "Resume"},
                                                        {"existing-metadata", "Existing"},
                                                        {"existing-marker", "Existing"},
                                                        {"existing-marker-and-bin", "Existing"}}) {
    auto events = admitted;
    events.insert(events.end(), {"session", "prepare:" + mode, "install", "ready", "named"});
    cases.push_back({name, named, 0, events, true});
  }
  for (const auto& name :
       {"prepare-failure", "prepare-unknown", "install-failure", "install-unknown", "duplicate-prepare"}) {
    auto events = admitted;
    events.insert(events.end(), {"session", "prepare:New"});
    if (std::string_view(name).starts_with("install"))
      events.push_back("install");
    events.insert(events.end(), {"failed", "stop"});
    cases.push_back({name, named, 190, events});
  }
  cases.push_back(
      {"directory-before-admission", named, 190, {"installation", "check-installation", "admission", "stop"}});
  cases.push_back({"installation-failure", named, 190, {"installation", "stop"}});
  cases.push_back({"installation-not-ready", named, 190, {"installation", "check-installation", "stop"}});
  cases.push_back({"admission-failure", named, 190, {"installation", "check-installation", "admission", "stop"}});
  cases.push_back(
      {"session-failure", named, 190, {"installation", "check-installation", "admission", "session", "stop"}});
#else
  cases.push_back({"ordinary", {}, 0, {"ordinary"}});
  cases.push_back({"unsupported", named, 190, {"stop"}});
#endif
  cases.push_back({"missing", {L"-stfc-profile"}, 190, {"stop"}});
  cases.push_back({"short", {L"-stfc-profile", L"0123456789abcdef"}, 190, {"stop"}});
  cases.push_back({"uppercase", {L"-stfc-profile", L"0123456789ABCDEF0123456789ABCDEF"}, 190, {"stop"}});
  cases.push_back({"nonhex", {L"-stfc-profile", L"g123456789abcdef0123456789abcdef"}, 190, {"stop"}});
  cases.push_back({"nonascii", {L"-stfc-profile", L"\u00e9123456789abcdef0123456789abcdef"}, 190, {"stop"}});
  cases.push_back({"repeated", {L"-stfc-profile", id, L"-stfc-profile", id}, 190, {"stop"}});
  return cases;
}

std::wstring Quote(std::wstring_view value)
{ return L"\"" + std::wstring(value) + L"\""; }

void Run(const fs::path& executable, const fs::path& suite_root, const Case& test)
{
  const auto directory = suite_root / test.name;
  fs::create_directory(directory);
  auto command = Quote(executable.wstring()) + L" --child " + Quote(directory.wstring()) + L" "
                 + Quote(std::wstring(test.name.begin(), test.name.end()));
  for (const auto& argument : test.arguments)
    command += L" " + Quote(argument);
  STARTUPINFOW        startup{sizeof(startup)};
  PROCESS_INFORMATION process{};
  Check(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                       &startup, &process),
        "could not start fixture child");
  CloseHandle(process.hThread);
  const auto waited = WaitForSingleObject(process.hProcess, 15000);
  if (waited != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 191);
    CloseHandle(process.hProcess);
    throw std::runtime_error(test.name + ": child did not finish");
  }
  DWORD      exit_code = 0;
  const bool inspected = GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hProcess);
  Check(inspected && exit_code == test.exit_code, test.name + ": unexpected exit " + std::to_string(exit_code));

  std::ifstream            file(directory / "events");
  std::vector<std::string> events;
  for (std::string line; std::getline(file, line);)
    events.push_back(line);
  auto prefix = events;
  if (test.releases) {
    const bool named = test.name != "ordinary";
    const auto count = named ? 2u : 1u;
    Check(prefix.size() >= count, test.name + ": missing lease destruction events");
    std::vector<std::string> released(prefix.end() - count, prefix.end());
    std::sort(released.begin(), released.end());
    Check(released
              == (named ? std::vector<std::string>{"release-installation", "release-session"}
                        : std::vector<std::string>{"release-installation"}),
          test.name + ": lease did not survive to process exit");
    prefix.resize(prefix.size() - count);
  }
  if (prefix != test.events) {
    std::cerr << test.name << " events:";
    for (const auto& event : events)
      std::cerr << ' ' << event;
    std::cerr << '\n';
    throw std::runtime_error(test.name + ": unexpected host lifecycle");
  }
}
} // namespace profile_host_test

int wmain(int argc, wchar_t** argv)
{
  using namespace profile_host_test;
  try {
    if (argc >= 4 && std::wstring_view(argv[1]) == L"--child") {
      root = argv[2];
      const std::wstring name(argv[3]);
      scenario.assign(name.begin(), name.end());
      Child();
      return 0;
    }
    const auto executable = fs::canonical(argv[0]);
    const auto suite_root =
        fs::temp_directory_path()
        / ("stfc-profile-host-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    fs::create_directory(suite_root);
    const auto cases = Cases();
    try {
      for (const auto& test : cases)
        Run(executable, suite_root, test);
    } catch (...) {
      std::cerr << "Synthetic fixture evidence: " << suite_root << '\n';
      throw;
    }
    fs::remove_all(suite_root);
    std::cout << "Profile host: " << cases.size() << " subprocess cases passed (enabled=" << STFC_PROFILES_ENABLED
              << ").\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
