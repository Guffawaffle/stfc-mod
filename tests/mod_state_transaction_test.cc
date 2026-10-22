#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#if !_WIN32
#include <sys/wait.h>
#endif

std::filesystem::path fixture_path;
#define MOD_STATE_FIXTURE_PATH fixture_path
#include "../mods/src/mod_state.cc"

void Check(bool value)
{
  if (!value)
    throw std::runtime_error("state transaction fixture failed");
}
void Child(const std::string& key)
{
  for (int index = 0; index < 30; ++index)
    Check(mod_state::Update([&](auto& state) { state[key] = state.value(key, 0) + 1; }));
}
#if _WIN32
PROCESS_INFORMATION Start(const std::wstring& role)
{
  wchar_t executable[32768];
  Check(GetModuleFileNameW(nullptr, executable, 32768) != 0);
  std::wstring command = L"\"" + std::wstring(executable) + L"\" \"" + fixture_path.native() + L"\" " + role;
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  Check(CreateProcessW(executable, command.data(), nullptr, nullptr, false, CREATE_NO_WINDOW, nullptr, nullptr,
                       &startup, &process));
  return process;
}
void Finish(PROCESS_INFORMATION process)
{
  Check(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0);
  DWORD code = 1;
  Check(GetExitCodeProcess(process.hProcess, &code) && code == 0);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
}
#endif
int main(int argc, char** argv)
{
  Check(argc >= 2);
  fixture_path = std::filesystem::absolute(argv[1]);
  if (argc == 3) {
    Child(argv[2]);
    return 0;
  }
  std::filesystem::create_directories(fixture_path.parent_path());
  std::ofstream(fixture_path)
      << R"({"version":1,"unrelated":{"external":"preserve"},"dev_console":{"other":"keep","background_opacity":0.5}})";
  {
    StateFileLock held(fixture_path, 1);
    Check(held.acquired());
    Check(!mod_state::TryUpdate([](auto& state) { state["must_not_write"] = true; }));
  }
#if _WIN32
  auto first  = Start(L"writerA");
  auto second = Start(L"writerB");
  Finish(first);
  Finish(second);
#else
  const auto first = fork();
  Check(first >= 0);
  if (!first) {
    Child("writerA");
    _exit(0);
  }
  const auto second = fork();
  Check(second >= 0);
  if (!second) {
    Child("writerB");
    _exit(0);
  }
  int status = 0;
  Check(waitpid(first, &status, 0) == first && status == 0);
  Check(waitpid(second, &status, 0) == second && status == 0);
#endif
  const auto state = mod_state::Read();
  Check(state && state->at("writerA") == 30 && state->at("writerB") == 30);
  Check(state->at("unrelated").at("external") == "preserve" && !state->contains("must_not_write"));
  Check(mod_state::PrepareAsync());
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
  Check(!mod_state::Update([](auto& data) { data["version"] = 999; }));
  std::ofstream(fixture_path) << R"({"version":999,"unrelated":"future"})";
  Check(!mod_state::Update([](auto& data) { data["new"] = 1; }));
  std::ifstream read(fixture_path);
  const auto    future = nlohmann::json::parse(read);
  Check(future.at("version") == 999 && future.at("unrelated") == "future" && !future.contains("new"));
  std::cout
      << "State transaction: actual OS lock, two-process latest RMW, unrelated fields and version refusal passed\n";
}
