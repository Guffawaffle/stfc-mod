#pragma once

#if _WIN32

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// A single isolated game's PlayerPrefs. The lifetime lock allows different
// profiles to run together but never two writers for the same profile.
class ProfilePrefsStore
{
public:
  ProfilePrefsStore(const std::filesystem::path& local_app_data, std::wstring_view profile_id, bool create_new);
  ~ProfilePrefsStore();

  ProfilePrefsStore(const ProfilePrefsStore&)            = delete;
  ProfilePrefsStore& operator=(const ProfilePrefsStore&) = delete;

  void SetInt(std::u16string_view key, std::int32_t value);
  void SetFloat(std::u16string_view key, float value);
  void SetString(std::u16string_view key, std::u16string_view value);
  std::int32_t GetInt(std::u16string_view key, std::int32_t fallback) const;
  float GetFloat(std::u16string_view key, float fallback) const;
  std::optional<std::u16string> GetString(std::u16string_view key) const;
  bool HasKey(std::u16string_view key) const;
  void DeleteKey(std::u16string_view key);
  void DeleteAll();
  void Save();
  void FinishNewProfile();

private:
  using Value = std::variant<std::int32_t, float, std::u16string>;
  using Values = std::map<std::u16string, Value, std::less<>>;

  void Serialize(const Values& values, std::vector<BYTE>& output) const;
  Values Deserialize(const BYTE* data, std::size_t size) const;
  std::vector<BYTE> Entropy() const;
  void Persist(const Values& values);
  void Set(std::u16string_view key, Value value);

  std::filesystem::path file_path_;
  std::u16string profile_id_;
  HANDLE lock_handle_ = INVALID_HANDLE_VALUE;
  bool file_exists_ = false;
  mutable std::mutex mutex_;
  Values values_;
};

#endif
