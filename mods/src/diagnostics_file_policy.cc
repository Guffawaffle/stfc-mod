#include "diagnostics_file_policy.h"

#include <algorithm>
#include <system_error>

namespace
{
  std::optional<std::string> make_root_warning(std::string_view              filename,
                                               const std::filesystem::path& configured_root,
                                               const std::filesystem::path& fallback_path,
                                               const std::error_code&       error)
  {
    std::string message = "Configured diagnostics root '" + configured_root.string() + "' is unusable for '"
                          + std::string(filename) + "'. Falling back to '" + fallback_path.string() + "'.";
    if (error) {
      message += " ";
      message += error.message();
    }
    return message;
  }

  std::optional<std::string> make_rotation_warning(const std::filesystem::path& path,
                                                   std::string_view              operation,
                                                   const std::error_code&        error)
  {
    if (!error) {
      return std::nullopt;
    }

    return "Failed to " + std::string(operation) + " diagnostics file '" + path.string() + "': " + error.message();
  }

  std::optional<std::string> make_oversize_warning(const std::filesystem::path& path,
                                                   std::uintmax_t               max_bytes,
                                                   std::size_t                  incoming_bytes)
  {
    return "Dropping diagnostics append for '" + path.string() + "' because payload size "
           + std::to_string(incoming_bytes) + " bytes exceeds the configured cap of "
           + std::to_string(max_bytes) + " bytes.";
  }

  bool is_missing_path_error(const std::error_code& error)
  { return error == std::errc::no_such_file_or_directory; }
} // namespace

DiagnosticsFileTarget ResolveDiagnosticsFileTarget(std::string_view filename,
                                                   const std::filesystem::path& fallback_path,
                                                   std::string_view             configured_root)
{
  DiagnosticsFileTarget result{fallback_path, std::nullopt};
  if (configured_root.empty()) {
    return result;
  }

  const auto root_path = std::filesystem::path(configured_root);

  std::error_code error;
  std::filesystem::create_directories(root_path, error);
  if (error) {
    result.warning = make_root_warning(filename, root_path, fallback_path, error);
    return result;
  }

  error.clear();
  if (!std::filesystem::is_directory(root_path, error) || error) {
    result.warning = make_root_warning(filename, root_path, fallback_path, error);
    return result;
  }

  result.path = root_path / std::filesystem::path(filename).filename();
  return result;
}

std::filesystem::path DiagnosticsRotatedPath(const std::filesystem::path& path, int index)
{
  if (!path.has_extension()) {
    return path.string() + "." + std::to_string(index);
  }

  auto rotated = path;
  rotated.replace_extension("." + std::to_string(index) + path.extension().string());
  return rotated;
}

DiagnosticsFilePrepareResult PrepareDiagnosticsFileForAppend(const std::filesystem::path& path,
                                                             std::uintmax_t               max_bytes,
                                                             int                          total_files,
                                                             std::size_t                  incoming_bytes)
{
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
      return {true, make_rotation_warning(parent, "prepare directory for", error)};
    }
  }

  if (incoming_bytes > max_bytes) {
    return {false, make_oversize_warning(path, max_bytes, incoming_bytes)};
  }

  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    if (!error || is_missing_path_error(error)) {
      return {};
    }
    return {true, make_rotation_warning(path, "stat", error)};
  }

  error.clear();
  const auto current_size = std::filesystem::file_size(path, error);
  if (error) {
    return {true, make_rotation_warning(path, "read size for", error)};
  }

  if (current_size < max_bytes && incoming_bytes <= max_bytes - current_size) {
    return {};
  }

  const auto generations = std::max(0, total_files - 1);
  if (generations == 0) {
    std::filesystem::remove(path, error);
    if (!error || is_missing_path_error(error)) {
      return {};
    }
    return {true, make_rotation_warning(path, "remove", error)};
  }

  const auto oldest_path = DiagnosticsRotatedPath(path, generations);
  std::filesystem::remove(oldest_path, error);
  if (error && !is_missing_path_error(error)) {
    return {true, make_rotation_warning(oldest_path, "remove", error)};
  }

  for (int index = generations - 1; index >= 1; --index) {
    const auto source = DiagnosticsRotatedPath(path, index);

    error.clear();
    if (!std::filesystem::exists(source, error)) {
      if (error && !is_missing_path_error(error)) {
        return {true, make_rotation_warning(source, "stat", error)};
      }
      continue;
    }

    const auto target = DiagnosticsRotatedPath(path, index + 1);
    error.clear();
    std::filesystem::rename(source, target, error);
    if (error) {
      return {true, make_rotation_warning(source, "rotate", error)};
    }
  }

  error.clear();
  std::filesystem::rename(path, DiagnosticsRotatedPath(path, 1), error);
  if (error) {
    return {true, make_rotation_warning(path, "rotate", error)};
  }

  return {};
}
