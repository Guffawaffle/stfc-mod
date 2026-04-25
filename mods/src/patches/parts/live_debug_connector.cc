#include "patches/live_debug_connector.h"

#include "file.h"
#include "patches/live_debug_request_dispatch.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view kRequestFile = "community_patch_debug.cmd";
constexpr std::string_view kResponseFile = "community_patch_debug.out";
constexpr std::string_view kTempSuffix = ".tmp";

std::filesystem::path get_live_debug_path(std::string_view filename)
{
  return std::filesystem::path(File::MakePathString(filename));
}

bool try_read_text_file(const std::filesystem::path& path, std::string& text)
{
  std::ifstream input(path, std::ios::in | std::ios::binary);
  if (!input.good()) {
    return false;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

void remove_file_if_exists(const std::filesystem::path& path)
{
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

bool try_write_text_file_atomic(const std::filesystem::path& path, std::string_view text)
{
  const auto temp_path = std::filesystem::path(path.string() + std::string(kTempSuffix));
  remove_file_if_exists(temp_path);

  {
    std::ofstream output(temp_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.good()) {
      return false;
    }

    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.flush();
    if (!output.good()) {
      return false;
    }
  }

  remove_file_if_exists(path);

  std::error_code rename_ec;
  std::filesystem::rename(temp_path, path, rename_ec);
  if (!rename_ec) {
    return true;
  }

  std::error_code copy_ec;
  std::filesystem::copy_file(temp_path, path, std::filesystem::copy_options::overwrite_existing, copy_ec);
  remove_file_if_exists(temp_path);
  return !copy_ec;
}

} // namespace

void live_debug_process_request_cycle()
{
  const auto request_path = get_live_debug_path(kRequestFile);
  if (!std::filesystem::exists(request_path)) {
    return;
  }

  std::string request_text;
  if (!try_read_text_file(request_path, request_text)) {
    return;
  }

  remove_file_if_exists(request_path);

  const auto response_text = live_debug_handle_request_text(request_text);

  const auto response_path = get_live_debug_path(kResponseFile);
  if (!try_write_text_file_atomic(response_path, response_text)) {
    return;
  }
}