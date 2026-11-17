#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace diagnostics
{
struct StorageLimits {
  uint64_t file_bytes     = 8 * 1024 * 1024;
  uint64_t recorder_bytes = 64 * 1024 * 1024;
  uint64_t total_bytes    = 256 * 1024 * 1024;
  uint64_t min_free_bytes = 32 * 1024 * 1024;
};

// Single-worker backend. Holds an exclusive lease on destination/captures-v1.
// Recorder/session names are 1..80 ASCII letters, digits, '-' or '_'. Recorder
// IDs are canonicalized to lowercase on every platform; session names retain
// case. Only recognized captures beneath that directory participate in retention.
class Storage
{
public:
  explicit Storage(std::filesystem::path destination, StorageLimits limits = {});
  ~Storage();
  Storage(Storage&&) noexcept;
  Storage& operator=(Storage&&) noexcept;
  Storage(const Storage&)            = delete;
  Storage& operator=(const Storage&) = delete;

  std::filesystem::path Open(std::string_view recorder, std::string_view session, std::string_view manifest_json);
  void                  Append(std::string_view recorder, std::string_view json_line);
  void                  Flush(std::string_view recorder);
  void                  Close(std::string_view recorder);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace diagnostics
