#include "config_save_fixture.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <nlohmann/json.hpp>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#if !_WIN32
#include <sys/wait.h>
#endif

std::filesystem::path fixture_path;
#define MOD_STATE_FIXTURE_PATH fixture_path
#include "../src/mod/mod_state.cc"

void Check(bool value, std::source_location where = std::source_location::current())
{
  if (!value)
    throw std::runtime_error("state transaction fixture failed at " + std::string(where.file_name()) + ":"
                             + std::to_string(where.line()));
}
void Child(const std::string& key)
{
  bool reported_contention = false;
  for (int index = 0; index < 30; ++index) {
    // Update deliberately gives up after a short lock wait. A competing writer
    // may still be busy; the fixture needs eventual commits, not immediate
    // success from every bounded attempt. Keep failures bounded as well.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!mod_state::Update([&](auto& state) { state[key] = state.value(key, 0) + 1; })) {
      if (!reported_contention) {
        std::ofstream marker(fixture_path.parent_path() / (key + ".contended"));
        marker.exceptions(std::ios::badbit | std::ios::failbit);
        marker << "contended";
        marker.close();
        reported_contention = true;
      }
      Check(std::chrono::steady_clock::now() < deadline);
      std::this_thread::yield();
    }
  }
}
bool AwaitContention()
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    if (std::filesystem::exists(fixture_path.parent_path() / "writerA.contended")
        && std::filesystem::exists(fixture_path.parent_path() / "writerB.contended"))
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}
std::string Bytes(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary);
  Check(file.is_open());
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void Write(std::string_view bytes)
{
  std::ofstream file(fixture_path, std::ios::binary | std::ios::trunc);
  file.exceptions(std::ios::badbit | std::ios::failbit);
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  file.close();
}
void ReadAndUpdatePolicy()
{
  Check(!mod_state::Read());
  Check(!mod_state::QueueShipPins({1}, {}) && !mod_state::QueueOfficerOrder({1})
        && !mod_state::QueueConsoleOpacity(50));
  Check(mod_state::Status(mod_state::Group::ShipPins).outcome == mod_state::SaveOutcome::None);
  for (const auto malformed : {"{invalid", "[1,2]", "null"}) {
    Write(malformed);
    Check(!mod_state::Read() && Bytes(fixture_path) == malformed);
    Check(mod_state::Update([](auto& data) { data["recovered"] = true; }));
    const auto state = mod_state::Read();
    Check(state && state->at("version") == 1 && state->at("recovered") == true);
  }
  Write(R"({"unrelated":{"exact":"9223372036854775807"}})");
  Check(mod_state::Read().has_value());
  Check(mod_state::Update([](auto& data) { data["added"] = true; }));
  const auto accepted = mod_state::Read();
  Check(accepted && accepted->at("version") == 1 && accepted->at("unrelated").at("exact") == "9223372036854775807");
  for (const auto unsupported : {R"({"version":999,"future":"preserve"})", R"({"version":"1","future":"preserve"})",
                                 R"({"version":1.0,"future":"preserve"})"}) {
    Write(unsupported);
    Check(!mod_state::Read());
    Check(!mod_state::Update([](auto& data) { data["must_not_write"] = true; }));
    Check(Bytes(fixture_path) == unsupported);
  }
  Write(R"({"version":1,"unrelated":"preserve"})");
  const auto original = Bytes(fixture_path);
  Check(!mod_state::Update([](auto& data) {
    data["unrelated"] = "changed";
    throw std::runtime_error("synthetic update refusal");
  }));
  Check(Bytes(fixture_path) == original);
}
#if _WIN32
struct ChildProcess {
  PROCESS_INFORMATION process;
  ~ChildProcess()
  {
    // Reap only the process this fixture created before its private directory is removed.
    if (WaitForSingleObject(process.hProcess, 10000) != WAIT_OBJECT_0) {
      TerminateProcess(process.hProcess, 1);
      WaitForSingleObject(process.hProcess, 10000);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
  }
  ChildProcess(const ChildProcess&) = delete;
  explicit ChildProcess(PROCESS_INFORMATION value)
      : process(value)
  {
  }
};
ChildProcess Start(const std::wstring& role, const std::wstring& nonce)
{
  wchar_t executable[32768];
  Check(GetModuleFileNameW(nullptr, executable, 32768) != 0);
  std::wstring command =
      L"\"" + std::wstring(executable) + L"\" --child \"" + fixture_path.native() + L"\" " + role + L" " + nonce;
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  Check(CreateProcessW(executable, command.data(), nullptr, nullptr, false, CREATE_NO_WINDOW, nullptr, nullptr,
                       &startup, &process));
  return ChildProcess(process);
}
void Finish(const ChildProcess& child)
{
  const auto& process = child.process;
  Check(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0);
  DWORD code = 1;
  Check(GetExitCodeProcess(process.hProcess, &code) && code == 0);
}
#endif
void AwaitDrain()
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!mod_state::QuitDrained() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::yield();
  Check(mod_state::QuitDrained());
}
void QuitDrainTransactions()
{
  using namespace mod_state;
  uint64_t pins = 0, officers = 0, opacity = 0;
  {
    // An actual OS lock keeps all three admitted groups from committing before
    // the shared quit API closes admission. No game process or native quit is involved.
    StateFileLock held(fixture_path, 1);
    Check(held.acquired());
    pins     = QueueShipPins({8, 7}, {"unresolved-after-quit"});
    officers = QueueOfficerOrder({1, 2});
    opacity  = QueueConsoleOpacity(71);
    Check(pins && officers && opacity);
    BeginQuitDrain();
    Check(!QuitDrained());
    Check(!QueueShipPins({99}, {}) && !QueueOfficerOrder({99}) && !QueueConsoleOpacity(99));
  }
  AwaitDrain();
  for (const auto [group, revision] :
       {std::pair{Group::ShipPins, pins}, {Group::OfficerOrder, officers}, {Group::ConsoleOpacity, opacity}}) {
    const auto status = Status(group);
    Check(status.revision == revision && status.outcome == SaveOutcome::Saved);
  }
  const auto saved = Read();
  Check(saved && saved->at("pinned_ship_ids") == nlohmann::json::array({"8", "7"})
        && saved->at("legacy_pinned_ship_names") == nlohmann::json::array({"unresolved-after-quit"})
        && saved->at("officer_preset_order") == nlohmann::json::array({"1", "2"})
        && saved->at("dev_console").at("background_opacity_percent") == 71);
  Check(saved->at("writerA") == 30 && saved->at("writerB") == 30 && saved->at("unrelated").at("external") == "preserve"
        && saved->at("dev_console").at("other") == "keep");

  // A native veto reopens the existing owner. A refused future-version write
  // settles as Failed, remains byte-identical, and must not block the quit drain.
  ResumeAfterQuitVeto();
  const std::string future = R"({"version":2,"unrelated":"future"})";
  Write(future);
  const auto failed = QueueShipPins({9}, {});
  Check(failed > pins);
  BeginQuitDrain();
  AwaitDrain();
  Check(Status(Group::ShipPins).revision == failed && Status(Group::ShipPins).outcome == SaveOutcome::Failed);
  Check(Bytes(fixture_path) == future);

  ResumeAfterQuitVeto();
  Write(saved->dump());
  const auto retried = QueueShipPins({9}, {});
  Check(retried > failed);
  BeginQuitDrain();
  AwaitDrain();
  Check(Status(Group::ShipPins).revision == retried && Status(Group::ShipPins).outcome == SaveOutcome::Saved);
  const auto retried_state = Read();
  Check(retried_state && retried_state->at("pinned_ship_ids") == nlohmann::json::array({"9"}));
  ResumeAfterQuitVeto();
  std::cout << "State quit drain: real file groups, closed admission, failed receipt and veto recovery passed\n";
}
void RunTransaction()
{
  ConfigSaveFixtureDirectory fixture;
  fixture_path = fixture.Root() / "state.json";
  ReadAndUpdatePolicy();
  const auto nonce = std::to_string(static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    std::ofstream owner(fixture.Root() / "state-fixture-owner", std::ios::binary);
    owner.exceptions(std::ios::badbit | std::ios::failbit);
    owner << nonce;
    owner.close();
  }
  Write(R"({"version":1,"unrelated":{"external":"preserve"},"dev_console":{"other":"keep","background_opacity":0.5}})");
  {
    StateFileLock held(fixture_path, 1);
    Check(held.acquired());
    Check(!mod_state::TryUpdate([](auto& state) { state["must_not_write"] = true; }));
  }
#if _WIN32
  const std::wstring wide_nonce(nonce.begin(), nonce.end());
#endif
  // Both writers must first observe a real bounded lock refusal. Release only
  // after their acknowledgements, rather than relying on scheduler timing.
  auto held = std::make_unique<StateFileLock>(fixture_path, 1);
  Check(held->acquired());
#if _WIN32
  auto       first     = Start(L"writerA", wide_nonce);
  auto       second    = Start(L"writerB", wide_nonce);
  const auto contended = AwaitContention();
  held.reset();
  Finish(first);
  Finish(second);
  Check(contended);
#else
  const auto first = fork();
  Check(first >= 0);
  if (!first) {
    try {
      Child("writerA");
      _exit(0);
    } catch (...) {
      _exit(1);
    }
  }
  const auto second = fork();
  if (second < 0) {
    int status = 0;
    waitpid(first, &status, 0);
    Check(false);
  }
  if (!second) {
    try {
      Child("writerB");
      _exit(0);
    } catch (...) {
      _exit(1);
    }
  }
  int        first_status = 0, second_status = 0;
  const auto contended = AwaitContention();
  held.reset();
  const auto first_wait  = waitpid(first, &first_status, 0);
  const auto second_wait = waitpid(second, &second_status, 0);
  Check(contended);
  Check(first_wait == first && first_status == 0 && second_wait == second && second_status == 0);
#endif
  const auto state = mod_state::Read();
  Check(state && state->at("writerA") == 30 && state->at("writerB") == 30);
  Check(state->at("unrelated").at("external") == "preserve" && !state->contains("must_not_write"));
  Check(mod_state::PrepareAsync());
  Check(mod_state::PrepareAsync());
  Check(!mod_state::QueueConsoleOpacity(-1) && !mod_state::QueueConsoleOpacity(101));
  Check(mod_state::QueueShipPins({7, 8}, {"unresolved"}));
  Check(mod_state::QueueOfficerOrder({2, 1}));
  Check(mod_state::QueueConsoleOpacity(70));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while ((mod_state::Status(mod_state::Group::ShipPins).outcome == mod_state::SaveOutcome::Pending
          || mod_state::Status(mod_state::Group::OfficerOrder).outcome == mod_state::SaveOutcome::Pending
          || mod_state::Status(mod_state::Group::ConsoleOpacity).outcome == mod_state::SaveOutcome::Pending)
         && std::chrono::steady_clock::now() < deadline)
    std::this_thread::yield();
  Check(mod_state::Status(mod_state::Group::ConsoleOpacity).outcome == mod_state::SaveOutcome::Saved);
  const auto saved = mod_state::Read();
  Check(saved && saved->at("writerA") == 30 && saved->at("writerB") == 30);
  Check(saved->at("dev_console").at("other") == "keep");
  Check(saved->at("dev_console").at("background_opacity_percent") == 70);
  Check(!saved->at("dev_console").contains("background_opacity"));
  Check(saved->at("officer_preset_order") == nlohmann::json::array({"2", "1"}));
  Check(saved->at("pinned_ship_ids") == nlohmann::json::array({"7", "8"}));
  QuitDrainTransactions();
  Check(!mod_state::Update([](auto& data) { data["version"] = 999; }));
  std::ofstream(fixture_path) << R"({"version":999,"unrelated":"future"})";
  Check(!mod_state::Update([](auto& data) { data["new"] = 1; }));
  std::ifstream read(fixture_path);
  const auto    future = nlohmann::json::parse(read);
  Check(future.at("version") == 999 && future.at("unrelated") == "future" && !future.contains("new"));
  std::cout << "State transaction: bounded lock refusals, two-process latest RMW, unrelated fields and version refusal "
               "passed\n";
}

#if _WIN32
int wmain(int count, wchar_t** arguments)
{
  try {
    if (count == 5 && std::wstring_view(arguments[1]) == L"--child") {
      const auto supplied  = std::filesystem::path(arguments[2]);
      const auto directory = std::filesystem::canonical(supplied.parent_path());
      const auto temporary = std::filesystem::canonical(std::filesystem::temp_directory_path());
      Check(directory.parent_path() == temporary
            && directory.filename().string().starts_with("stfc-config-save-fixture-")
            && supplied.filename() == "state.json");
      const std::wstring_view role = arguments[3];
      Check(role == L"writerA" || role == L"writerB");
      const std::wstring_view proof = arguments[4];
      Check(!proof.empty() && proof.find_first_not_of(L"0123456789") == std::wstring_view::npos);
      const std::string nonce(proof.begin(), proof.end());
      Check(Bytes(directory / "state-fixture-owner") == nonce);
      fixture_path = directory / "state.json";
      Child(role == L"writerA" ? "writerA" : "writerB");
      return 0;
    }
    Check(count == 1);
    RunTransaction();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
#else
int main(int count, char**)
{
  try {
    Check(count == 1);
    RunTransaction();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
#endif
