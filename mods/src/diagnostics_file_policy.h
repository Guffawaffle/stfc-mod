#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct DiagnosticsFileTarget {
  std::filesystem::path path;
  std::optional<std::string> warning;
};

struct DiagnosticsFilePrepareResult {
  bool append_allowed = true;
  bool rotated = false;
  std::optional<std::string> warning;
};

[[nodiscard]] DiagnosticsFileTarget ResolveDiagnosticsFileTarget(std::string_view filename,
                                                                 const std::filesystem::path& fallback_path,
                                                                 std::string_view configured_root);

[[nodiscard]] std::filesystem::path DiagnosticsRotatedPath(const std::filesystem::path& path, int index);

[[nodiscard]] DiagnosticsFilePrepareResult PrepareDiagnosticsFileForAppend(const std::filesystem::path& path,
                                                                           std::uintmax_t               max_bytes,
                                                                           int                          total_files,
                                                                           std::size_t                  incoming_bytes);
