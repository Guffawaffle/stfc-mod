// Real reloads against an isolated filesystem boundary, including readers that outlive a settings replacement.
#include "config.h"
#include "config_save_fixture.h"
#include "file.h"
#include "runtime_config_no_host.h"

#include <spdlog/spdlog.h>
#include <toml++/toml.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace
{
const ConfigSaveFixtureDirectory fixture_directory;
const auto                       directory   = fixture_directory.Root();
const std::string                config_name = "settings.toml", vars_name = "runtime.vars";
bool                             fail_vars                 = false;
bool                             check_capture_before_read = false;
std::size_t                      expected_capture_count    = 0;
bool                             clear_snapshot_at_read    = false;

void Check(bool ok, const char* message)
{
  if (!ok)
    throw std::runtime_error(message);
}
void Write(std::string_view settings)
{
  std::ofstream file(directory / config_name);
  file.exceptions(std::ios::failbit | std::ios::badbit);
  file << settings;
}
} // namespace

namespace File
{
const std::string& Config()
{ return config_name; }
const std::string& DefaultConfig()
{ return config_name; }
const std::string& Vars()
{ return vars_name; }
std::filesystem::path Path(std::string_view name, bool)
{
  if (name == config_name && check_capture_before_read) {
    Check(runtime_config_fixture::CaptureCount() == expected_capture_count,
          "reload must capture protected values before resolving the primary file");
    check_capture_before_read = false;
    if (clear_snapshot_at_read) {
      // Simulate an acknowledgement arriving after CaptureReload. The current
      // reload must retain its independently owned pre-read protection.
      runtime_config_fixture::SetReloadSnapshot({});
      clear_snapshot_at_read = false;
    }
  }
  if (fail_vars && name == vars_name)
    throw std::runtime_error("runtime vars directory is unavailable");
  return directory / name;
}
void MigrateLegacyFiles() {}
} // namespace File

namespace unity::Key
{
bool Pressed(KeyCode)
{ return false; }
bool Down(KeyCode)
{ return false; }
bool IsModified()
{ return false; }
} // namespace unity::Key

// Each mode runs in its own process so it exercises the first real Config::Get,
// not a reset or alternate implementation of its startup decisions.
int StartupPrimaryCase(bool malformed)
{
  std::filesystem::create_directories(directory);
  int result = 0;
  try {
    const std::string original = "# SYNTHETIC invalid settings: keep these bytes\n"
                                 "[unrelated.plugin]\nunknown = \"preserve me\"\n[broken";
    std::string       original_bytes;
    if (malformed) {
      Write(original);
      // Write uses the host's text mode. Capture the actual preexisting bytes,
      // including Windows CRLF, rather than comparing disk bytes with LF source.
      std::ifstream before(directory / config_name, std::ios::binary);
      Check(before.is_open(), "malformed fixture primary was not written");
      original_bytes.assign(std::istreambuf_iterator<char>{before}, std::istreambuf_iterator<char>{});
    } else
      Check(!std::filesystem::exists(directory / config_name), "missing-primary fixture was not empty");

    Check(Config::Get().patches.uiscalehooks && Config::Sync::Get()->resolver_cache_ttl == 300,
          "startup did not activate the expected defaults");
    std::ifstream primary(directory / config_name, std::ios::binary);
    Check(primary.is_open(), "startup primary settings are unavailable");
    const std::string actual(std::istreambuf_iterator<char>{primary}, std::istreambuf_iterator<char>{});
    if (malformed) {
      Check(actual == original_bytes, "startup overwrote malformed primary bytes, unknown keys or comments");
    } else {
      const auto generated = toml::parse(actual);
      Check(generated["sync"]["resolver_cache_ttl"].value<int>() == 300,
            "missing-primary creation did not produce the expected valid defaults");
    }
    const auto vars = toml::parse_file((directory / vars_name).string());
    Check(vars["sync"]["resolver_cache_ttl"].value<int>() == 300,
          "startup runtime vars did not contain the active defaults");
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    result = 1;
  }
  if (!result)
    std::cout << "Config startup primary preservation passed: " << (malformed ? "malformed" : "missing") << '\n';
  return result;
}

int main(int argc, char** argv)
{
  const bool fail_startup_output = argc == 2 && std::string_view(argv[1]) == "startup-output-failure";
  const bool malformed_primary   = argc == 2 && std::string_view(argv[1]) == "startup-invalid-primary";
  const bool missing_primary     = argc == 2 && std::string_view(argv[1]) == "startup-missing-primary";
  if (argc != 1 && !fail_startup_output && !malformed_primary && !missing_primary) {
    std::cerr << "pass no arguments, startup-output-failure, startup-invalid-primary or startup-missing-primary\n";
    return 1;
  }
  spdlog::set_level(spdlog::level::off);
  if (malformed_primary || missing_primary)
    return StartupPrimaryCase(malformed_primary);
  std::filesystem::create_directories(directory);
  int result = 0;
  try {
    Write(R"(
      # SYNTHETIC primary settings: startup output must not rewrite this text.
      [patches]
      uiscalehooks = false
      [shortcuts]
      quit = "F1"
      [sync]
      resolver_cache_ttl = 123
      [sync.targets.test]
      url = "https://example.invalid"
      token = "original"
      resources = true
      [unrelated.plugin]
      unknown = "preserve this key and comment"
    )");
    const auto read_primary = [] {
      std::ifstream file(directory / config_name, std::ios::binary);
      Check(file.is_open(), "could not read the synthetic primary settings");
      return std::string(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
    };
    const auto primary_before = read_primary();
    fail_vars                 = fail_startup_output;
    Check(!Config::Get().patches.uiscalehooks && Shortcut::Describe(Action::kQuit) == "F1", "startup activation");
    fail_vars = false;
    Check(read_primary() == primary_before, "startup rewrote existing primary settings, unknown keys or comments");
    if (fail_startup_output)
      Check(!std::filesystem::exists(directory / vars_name),
            "startup output failure unexpectedly created a runtime-vars file");
    auto                              first   = Config::Sync::Get();
    std::weak_ptr<const Config::Sync> retired = first;
    std::latch                        captured(1);
    bool                              reader_ok = false;
    // This reader spans every reload below and must keep its original settings throughout.
    std::jthread reader([snapshot = std::move(first), &captured, &reader_ok](std::stop_token stop) {
      std::mutex                  mutex;
      std::condition_variable_any ready;
      std::unique_lock            lock(mutex);
      captured.count_down();
      ready.wait(lock, stop, [] { return false; });
      reader_ok = snapshot->resolver_cache_ttl == 123 && snapshot->options.resources
                  && snapshot->targets.at("test").token == "original";
    });
    captured.wait();
    bool reloads_ok = true;
    for (int i = 0; i < 20; ++i) {
      std::weak_ptr<const Config::Sync> previous = Config::Sync::Get();
      Write(std::format("[patches]\nuiscalehooks = true\n[shortcuts]\nquit = \"F2\"\n[sync]\nresolver_cache_ttl = {}",
                        200 + i));
      reloads_ok &= Config::Reload();
      reloads_ok &= Config::Sync::Get()->resolver_cache_ttl == 200 + i;
      if (i != 0)
        reloads_ok &= previous.expired();
    }
    const bool reader_retained = !retired.expired();
    reader.request_stop();
    reader.join();
    Check(reloads_ok, "reloads replace snapshots and reclaim unreferenced settings");
    Check(reader_retained && reader_ok && retired.expired(), "an old snapshot lives exactly as long as its reader");
    Check(!Config::Get().patches.uiscalehooks && Shortcut::Describe(Action::kQuit) == "F2",
          "reload activates bindings but preserves startup hook groups");
    const auto current  = Config::Sync::Get();
    const auto ui_scale = Config::Get().graphics.ui_scale;
    Write("[shortcuts]\nquit = \"F3\"\n[sync]\nresolver_cache_ttl = 999\n[graphics]\nui_scale = 2.0");
    fail_vars = true;
    Check(!Config::Reload(), "a filesystem preparation failure rejects the reload");
    fail_vars = false;
    Check(Config::Sync::Get() == current && Shortcut::Describe(Action::kQuit) == "F2"
              && Config::Get().graphics.ui_scale == ui_scale,
          "a rejected reload changes neither settings, snapshots nor bindings");
    Write("[broken");
    Check(!Config::Reload() && Config::Sync::Get() == current && Shortcut::Describe(Action::kQuit) == "F2",
          "invalid TOML preserves the current settings");

    // The native owner decides which intents are unacknowledged. This fixture
    // supplies that boundary deterministically and exercises the real reload
    // consumer without starting a writer, frame hook or game runtime.
    Write("# SYNTHETIC external edit; reload must not rewrite primary bytes\n"
          "[graphics]\nui_scale = 0.5\n[ui]\nshow_player_cargo = false\n"
          "[shortcuts]\nquit = \"F3\"\n[sync]\nresolver_cache_ttl = 777\n");
    const auto protected_primary = read_primary();
    runtime_config_fixture::SetReloadSnapshot({{"graphics", "ui_scale", 1.5}, {"ui", "show_player_cargo", true}});
    expected_capture_count    = runtime_config_fixture::CaptureCount() + 1;
    check_capture_before_read = true;
    clear_snapshot_at_read    = true;
    Check(Config::Reload(), "protected session values must allow an otherwise valid reload");
    Check(!check_capture_before_read && !clear_snapshot_at_read,
          "reload did not reach the expected primary read boundary");
    Check(Config::Get().graphics.ui_scale == 1.5f && Config::Get().ui.show_player_cargo,
          "completion during the read must not discard the pre-read protected values");
    Check(Config::Sync::Get()->resolver_cache_ttl == 777 && Shortcut::Describe(Action::kQuit) == "F3",
          "unrelated external keys must still activate alongside protected values");
    Check(read_primary() == protected_primary, "session overlay must not rewrite the primary file");

    // The next snapshot is empty: an acknowledgement observed before this read
    // allows an external value to flow. It does not alter a writer baseline.
    Check(Config::Reload(), "acknowledged values must allow the following reload");
    Check(Config::Get().graphics.ui_scale == 0.5f && !Config::Get().ui.show_player_cargo,
          "an empty protection snapshot must preserve false and external values");

    const auto acknowledged = Config::Sync::Get();
    Write("[graphics]\nui_scale = 2.5\n[shortcuts]\nquit = \"F4\"\n[sync]\nresolver_cache_ttl = 888\n");
    runtime_config_fixture::SetCaptureFailure(true);
    Check(!Config::Reload(), "protection acquisition failure must reject rather than erase session values");
    runtime_config_fixture::SetCaptureFailure(false);
    Check(Config::Sync::Get() == acknowledged && Config::Get().graphics.ui_scale == 0.5f
              && Shortcut::Describe(Action::kQuit) == "F3",
          "capture failure must leave all activated state and bindings unchanged");

    runtime_config_fixture::SetReloadSnapshot({{"ui", "show_player_cargo", true}});
    Write("ui = false\n[graphics]\nui_scale = 3.0\n[sync]\nresolver_cache_ttl = 999\n");
    Check(!Config::Reload() && Config::Sync::Get() == acknowledged && Config::Get().graphics.ui_scale == 0.5f
              && Shortcut::Describe(Action::kQuit) == "F3",
          "an incompatible section must reject the reload without overwriting or dropping protection");
    runtime_config_fixture::Reset();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    result = 1;
  }
  if (!result)
    std::cout << "config reload tests passed\n";
  return result;
}
