// Exercise the same mutation helpers as native shortcuts through the real
// registered writer/editor and a fixture-owned file; no Unity or account state.
#include "config_save.h"
#include "config_save_fixture.h"
#include "runtime_config_writer.h"
#include "runtime_reload.h"
#include "settings/basic_gameplay_settings.h"

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include <type_traits>

using namespace std::chrono_literals;
using config_edit::Value;
namespace
{
std::optional<Value> Initial(const toml::table& input, const gameplay_settings::Key& key)
{
  const auto node = input[key.section][key.name];
  if (node.is_boolean())
    return *node.value<bool>();
  if (node.is_string())
    return *node.value<std::string>();
  if (node.is_integer())
    return *node.value<std::int64_t>();
  if (node.is_floating_point())
    return *node.value<double>();
  return {};
}
void Drain(config_edit::RuntimeConfigWriter& writer)
{
  writer.Stop(false);
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!writer.PollStopped()) {
    assert(std::chrono::steady_clock::now() < deadline);
    std::this_thread::sleep_for(1ms);
  }
}
} // namespace
int main()
{
  ConfigSaveFixtureDirectory directory;
  const auto                 path     = directory.Root() / "settings.toml";
  const std::string          original = "# SYNTHETIC primary: keep comments and unknown data\n"
                                        "[control]\nhotkeys_enabled = true # switch\n"
                                        "[ui]\nauto_confirm_instant_warp = 'none' # travel\n"
                                        "[graphics]\nui_scale = 0.6 # exact native action\ndefault_system_zoom = 1750.0\n"
                                        "[unrelated.plugin]\nunknown = 'keep me'\n";
  {
    std::ofstream file(path, std::ios::binary);
    file << original;
  }
  const auto                                    startup = toml::parse(original);
  config_edit::RuntimeConfigWriter              writer(path);
  std::set<std::pair<std::string, std::string>> registered;
  assert(gameplay_settings::RegisterKeys([&](const gameplay_settings::Key& key) {
    assert(registered.emplace(key.section, key.name).second);
    return writer.Register(key.section, key.name, Initial(startup, key));
  }));
  assert(registered.size() == 19);
  assert(writer.Submit("graphics", "not_registered", 1.0) == 0);
  std::map<std::pair<std::string, std::string>, Value> desired;
  std::size_t                                          submissions = 0;
  auto                                                 submit = [&](const gameplay_settings::Key& key, Value value) {
    desired.insert_or_assign({key.section, key.name}, value);
    assert(writer.Submit(key.section, key.name, std::move(value), key.delay));
    ++submissions;
  };
  bool enabled = true;
  gameplay_settings::SetBoolean(enabled, false, gameplay_settings::Boolean::Hotkeys, submit);
  assert(!enabled);
  for (std::size_t i = 1; i < gameplay_settings::booleans.size(); ++i) {
    bool value = false;
    gameplay_settings::ToggleBoolean(value, static_cast<gameplay_settings::Boolean>(i), submit);
    assert(value);
  }

  float      native        = 0.0f;
  const auto before_native = submissions;
  assert(!gameplay_settings::AdjustScale(native, 0.05f, true, gameplay_settings::Scale::Interface, submit).applied);
  assert(!gameplay_settings::AdjustScale(native, 0.05f, true, gameplay_settings::Scale::Viewer, submit).applied);
  assert(native == 0 && submissions == before_native);
  auto ship = gameplay_settings::AdjustScale(native, 0.05f, true, gameplay_settings::Scale::Ship, submit);
  assert(ship.applied && native == 0.1f); // Ship zero does not share the native-scale exemption.
  float ui = 1.95f, viewer = 1.0f, model = 19.99f;
  assert(gameplay_settings::AdjustScale(ui, 0.2f, true, gameplay_settings::Scale::Interface, submit).value == 2.0f);
  const auto viewed = gameplay_settings::AdjustScale(viewer, 0.05f, true, gameplay_settings::Scale::Viewer, submit);
  assert(viewed.value == 1.0f + 0.05f * 0.25f);
  assert(gameplay_settings::AdjustScale(model, 0.2f, true, gameplay_settings::Scale::Ship, submit).value == 20.0f);
  assert(gameplay_settings::AdjustScale(model, 100.0f, false, gameplay_settings::Scale::Ship, submit).value == 0.1f);
  // The native consumer separately asserts its actual enum has this ordering.
  int mode = 0;
  assert(gameplay_settings::CycleWarp(mode, submit) && mode == 1);
  assert(gameplay_settings::CycleWarp(mode, submit) && mode == 2);
  assert(gameplay_settings::CycleWarp(mode, submit) && mode == 0);
  assert(gameplay_settings::CycleWarp(mode, submit) && mode == 1);
  float      zoom     = 0.0f;
  const auto observed = gameplay_settings::CurrentZoom(30.0f, 10.0f, 110.0f, 5000.0f);
  assert(observed == 1000.0f);
  for (std::size_t i = 0; i < gameplay_settings::zooms.size(); ++i)
    gameplay_settings::StoreZoom(zoom, observed + static_cast<float>(i), static_cast<gameplay_settings::Zoom>(i),
                                 submit);
  Drain(writer);
  assert(!writer.HasFailures());
  auto text  = ReadConfigText(path);
  auto saved = toml::parse(text);
  assert(text.starts_with("# SYNTHETIC primary: keep comments and unknown data\n"));
  assert(text.find("# switch") != text.npos && text.find("# travel") != text.npos);
  assert(saved["unrelated"]["plugin"]["unknown"].value<std::string>() == "keep me");
  for (const auto& [key, value] : desired) {
    const auto status = writer.GetStatus(key.first, key.second);
    assert(!status.pending && status.subject == value
           && (status.outcome == config_edit::Outcome::Saved || status.outcome == config_edit::Outcome::AlreadySaved));
    std::visit(
        [&](const auto& expected) {
          using T = std::decay_t<decltype(expected)>;
          assert(saved[key.first][key.second].value<T>() == expected);
        },
        value);
  }

  // Ctrl-Q uses this same registered boolean path in both directions, without
  // changing its native recovery-reset action. Wait for real save receipts.
  assert(writer.Resume());
  bool queue_enabled = true;
  gameplay_settings::ToggleBoolean(queue_enabled, gameplay_settings::Boolean::Queue, submit);
  Drain(writer);
  assert(!queue_enabled && toml::parse(ReadConfigText(path))["control"]["queue_enabled"].value<bool>() == false);
  assert(writer.GetStatus("control", "queue_enabled").outcome == config_edit::Outcome::Saved);
  assert(writer.Resume());
  gameplay_settings::ToggleBoolean(queue_enabled, gameplay_settings::Boolean::Queue, submit);
  Drain(writer);
  assert(queue_enabled && toml::parse(ReadConfigText(path))["control"]["queue_enabled"].value<bool>() == true);
  text  = ReadConfigText(path);
  saved = toml::parse(text);

  // Automatic preset selection remains a nonpersistent live intent. The real
  // native RetainSessionIntent boundary is independently covered by its fixture.
  runtime_config::ReloadSnapshot session;
  const auto                     before_session = submissions;
  gameplay_settings::UsePresetAsDefault(zoom, 4321.125f, [&](const gameplay_settings::Key& key, double value) {
    session.push_back({key.section, key.name, value});
  });
  assert(submissions == before_session && ReadConfigText(path) == text && !writer.HasFailures());
  runtime_config::ApplyReloadSnapshot(saved, session);
  assert(saved["graphics"]["default_system_zoom"].value<double>() == static_cast<double>(zoom));
  assert(toml::parse(text)["graphics"]["default_system_zoom"].value<double>() != static_cast<double>(zoom));

  // A repeated registration must never rebase over an intervening external edit.
  assert(writer.Resume());
  auto       external = text;
  const auto ui_value = external.find("ui_scale = 2.0");
  assert(ui_value != std::string::npos);
  external.replace(ui_value, std::string("ui_scale = 2.0").size(), "ui_scale = 1.9");
  assert(ReplaceConfigText(path, external, text));
  const auto reparsed = toml::parse(external);
  assert(gameplay_settings::RegisterKeys([&](const gameplay_settings::Key& key) {
    return writer.Register(key.section, key.name, Initial(reparsed, key));
  }));
  gameplay_settings::AdjustScale(ui, 0.05f, false, gameplay_settings::Scale::Interface, submit);
  Drain(writer);
  assert(writer.GetStatus("graphics", "ui_scale").outcome == config_edit::Outcome::Conflict);
  assert(ReadConfigText(path) == external);
  // A missing key is not an observed false. External deletion conflicts with
  // the acknowledged true value, while the live false can survive a reload.
  auto       removed    = external;
  const auto queue_line = removed.find("queue_enabled = true");
  assert(queue_line != std::string::npos);
  const auto queue_end = removed.find('\n', queue_line);
  removed.erase(queue_line, queue_end == std::string::npos ? std::string::npos : queue_end - queue_line + 1);
  assert(ReplaceConfigText(path, removed, external));
  assert(writer.Resume());
  gameplay_settings::ToggleBoolean(queue_enabled, gameplay_settings::Boolean::Queue, submit);
  Drain(writer);
  assert(!queue_enabled && writer.GetStatus("control", "queue_enabled").outcome == config_edit::Outcome::Conflict);
  assert(ReadConfigText(path) == removed);
  auto reloaded = toml::parse(removed);
  assert(!reloaded["control"]["queue_enabled"]);
  runtime_config::ApplyReloadSnapshot(reloaded, {{"control", "queue_enabled", queue_enabled}});
  assert(reloaded["control"]["queue_enabled"].value<bool>() == false);
  std::cout << "Gameplay settings: registered native mutations, exact values, session-only zoom and preserving writer "
               "passed\n";
}
