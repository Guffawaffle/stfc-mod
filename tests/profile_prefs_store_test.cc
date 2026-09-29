#include "patches/parts/profile_prefs_store.h"

#include <Windows.h>

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

namespace fs = std::filesystem;

void Check(bool condition, std::string_view message)
{
  if (!condition)
    throw std::runtime_error(std::string(message));
}

template <typename Action>
void CheckThrows(Action&& action, std::string_view message)
{
  try {
    action();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

std::string ReadBytes(const fs::path& path)
{
  std::ifstream file(path, std::ios::binary);
  Check(file.is_open(), "could not read synthetic preferences");
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

class TemporaryDirectory
{
public:
  TemporaryDirectory()
  {
    wchar_t temp_path[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(MAX_PATH + 1, temp_path);
    if (length == 0 || length > MAX_PATH)
      throw std::runtime_error("could not locate the temporary directory");

    wchar_t unique_path[MAX_PATH + 1]{};
    if (!GetTempFileNameW(temp_path, L"sps", 0, unique_path))
      throw std::runtime_error("could not allocate a temporary test path");
    path_ = unique_path;
    fs::remove(path_);
    fs::create_directory(path_);
  }

  ~TemporaryDirectory()
  {
    std::error_code error;
    fs::remove_all(path_, error);
  }

  TemporaryDirectory(const TemporaryDirectory&)            = delete;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

  const fs::path& Path() const
  { return path_; }

  fs::path PrefsPath(std::wstring_view id) const
  { return path_ / L"STFC Community Mod" / L"Profiles" / std::wstring(id) / L"player_prefs.bin"; }

private:
  fs::path path_;
};

void NewProfilePersistsAndExistingReopens()
{
  TemporaryDirectory temp;
  const auto file = temp.PrefsPath(L"josep");
  {
    ProfilePrefsStore fresh(temp.Path(), L"josep", ProfileOpenMode::New);
    Check(!fs::exists(file), "new profile must wait for explicit initialization");
    fresh.FinishNewProfile();
    Check(fs::is_regular_file(file), "new profile did not create player_prefs.bin");
    fresh.SetInt(u"synthetic-int", 42);
    fresh.SetFloat(u"synthetic-float", 1.25f);
    fresh.SetString(u"synthetic-string", u"test value");
  }
  {
    ProfilePrefsStore existing(temp.Path(), L"josep", ProfileOpenMode::Existing);
    Check(existing.GetInt(u"synthetic-int", -1) == 42, "integer did not survive reopen");
    Check(existing.GetFloat(u"synthetic-float", -1.0f) == 1.25f, "float did not survive reopen");
    Check(existing.GetString(u"synthetic-string") == u"test value", "string did not survive reopen");
    Check(existing.GetInt(u"missing", 19) == 19, "missing integer fallback changed");
  }
}

void OpenOrCreatePreservesExistingPreferences()
{
  TemporaryDirectory temp;
  {
    ProfilePrefsStore fresh(temp.Path(), L"second", ProfileOpenMode::New);
    fresh.SetInt(u"synthetic-account", 314);
  }
  {
    ProfilePrefsStore reopened(temp.Path(), L"second", ProfileOpenMode::OpenOrCreate);
    Check(reopened.GetInt(u"synthetic-account", -1) == 314,
          "OpenOrCreate replaced an established preference store");
    reopened.FinishNewProfile();
    Check(reopened.GetInt(u"synthetic-account", -1) == 314,
          "FinishNewProfile replaced an established preference store");
  }
}

void OpenOrCreateInitializesOnlyAnUntouchedProfile()
{
  TemporaryDirectory temp;
  const auto file = temp.PrefsPath(L"fresh");
  {
    ProfilePrefsStore interrupted(temp.Path(), L"fresh", ProfileOpenMode::OpenOrCreate);
    Check(!fs::exists(file), "pre-install store unexpectedly wrote preferences");
  }
  // An interrupted first launch may leave its lock file, but no committed
  // preferences or prior-use marker. Retrying must still be possible.
  {
    ProfilePrefsStore fresh(temp.Path(), L"fresh", ProfileOpenMode::OpenOrCreate);
    Check(!fs::exists(file), "first use wrote preferences before hook installation");
    fresh.FinishNewProfile();
    Check(fs::is_regular_file(file), "first use did not initialize preferences");
  }
  Check(fs::remove(file), "could not remove synthetic preferences for enrollment test");
  CheckThrows([&] { ProfilePrefsStore lost(temp.Path(), L"fresh", ProfileOpenMode::OpenOrCreate); },
              "lost preferences were mistaken for first enrollment");
  Check(!fs::exists(file), "failed enrollment silently recreated preferences");
}

void MissingEstablishedPreferencesFailWithoutCreatingABin()
{
  TemporaryDirectory temp;
  const auto file = temp.PrefsPath(L"missing");
  {
    ProfilePrefsStore fresh(temp.Path(), L"missing", ProfileOpenMode::New);
    fresh.FinishNewProfile();
  }
  Check(fs::remove(file), "could not remove synthetic preferences for missing-bin test");
  CheckThrows([&] { ProfilePrefsStore existing(temp.Path(), L"missing", ProfileOpenMode::Existing); },
              "established profile silently accepted a missing preferences bin");
  Check(!fs::exists(file), "opening an established profile silently recreated its missing bin");
}

void CopiedPreferencesCannotOpenUnderAnotherId()
{
  TemporaryDirectory temp;
  {
    ProfilePrefsStore source(temp.Path(), L"josep", ProfileOpenMode::New);
    source.SetString(u"synthetic-secret", u"only josep can read this");
  }
  const auto copied = temp.PrefsPath(L"other");
  fs::create_directories(copied.parent_path());
  fs::copy_file(temp.PrefsPath(L"josep"), copied);
  CheckThrows([&] { ProfilePrefsStore wrong_id(temp.Path(), L"other", ProfileOpenMode::Existing); },
              "a copied preferences bin opened under a different profile ID");
  Check(fs::is_regular_file(copied), "failed cross-ID open changed the copied bin");
}

void NewModeRefusesExistingPreferences()
{
  TemporaryDirectory temp;
  {
    ProfilePrefsStore original(temp.Path(), L"existing", ProfileOpenMode::New);
    original.SetInt(u"synthetic-value", 77);
  }
  CheckThrows([&] { ProfilePrefsStore duplicate(temp.Path(), L"existing", ProfileOpenMode::New); },
              "New mode accepted an existing preferences bin");
  ProfilePrefsStore still_existing(temp.Path(), L"existing", ProfileOpenMode::Existing);
  Check(still_existing.GetInt(u"synthetic-value", -1) == 77,
        "failed New open changed existing preferences");
}

void UnchangedValuesDoNotRewritePreferences()
{
  TemporaryDirectory temp;
  const auto file = temp.PrefsPath(L"repeat");
  {
    ProfilePrefsStore store(temp.Path(), L"repeat", ProfileOpenMode::New);
    store.SetInt(u"count", 7);
    const auto first = ReadBytes(file);
    store.SetInt(u"count", 7);
    Check(ReadBytes(file) == first, "repeated integer rewrote preferences");
    store.SetInt(u"count", 8);
    Check(ReadBytes(file) != first, "changed integer did not persist");

    store.SetFloat(u"scale", 0.0f);
    const auto positive_zero = ReadBytes(file);
    store.SetFloat(u"scale", 0.0f);
    Check(ReadBytes(file) == positive_zero, "repeated float rewrote preferences");
    store.SetFloat(u"scale", -0.0f);
    Check(ReadBytes(file) != positive_zero, "float sign change did not persist");

    store.SetString(u"label", u"ready");
    const auto string_value = ReadBytes(file);
    store.SetString(u"label", u"ready");
    Check(ReadBytes(file) == string_value, "repeated string rewrote preferences");
  }
  {
    ProfilePrefsStore reopened(temp.Path(), L"repeat", ProfileOpenMode::Existing);
    Check(reopened.GetInt(u"count", -1) == 8, "changed integer was lost after reopen");
    Check(std::bit_cast<std::uint32_t>(reopened.GetFloat(u"scale", 1.0f))
              == std::bit_cast<std::uint32_t>(-0.0f),
          "float sign change was lost after reopen");
    Check(reopened.GetString(u"label") == u"ready", "string was lost after reopen");
    reopened.DeleteAll();
    const auto empty = ReadBytes(file);
    reopened.DeleteAll();
    Check(ReadBytes(file) == empty, "repeated DeleteAll rewrote preferences");
    Check(fs::remove(file), "could not remove synthetic preferences for missing-bin test");
    CheckThrows([&] { reopened.DeleteAll(); }, "repeated DeleteAll accepted a missing preferences bin");
  }
}

} // namespace

int main()
{
  try {
    NewProfilePersistsAndExistingReopens();
    OpenOrCreatePreservesExistingPreferences();
    OpenOrCreateInitializesOnlyAnUntouchedProfile();
    MissingEstablishedPreferencesFailWithoutCreatingABin();
    CopiedPreferencesCannotOpenUnderAnotherId();
    NewModeRefusesExistingPreferences();
    UnchangedValuesDoNotRewritePreferences();
    std::cout << "profile preference store tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "profile preference store tests failed: " << error.what() << '\n';
    return 1;
  }
}
