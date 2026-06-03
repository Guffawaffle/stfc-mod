#include <doctest/doctest.h>

#include "diagnostics_file_policy.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
  class ScopedTempDir
  {
  public:
    ScopedTempDir()
    {
      const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
      path_          = std::filesystem::temp_directory_path() / ("stfc-mod-diagnostics-file-policy-" + std::to_string(now));
      std::filesystem::create_directories(path_);
    }

    ~ScopedTempDir() { std::filesystem::remove_all(path_); }

    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
  };

  void write_text(const std::filesystem::path& path, std::string_view text)
  {
    if (!path.parent_path().empty()) {
      std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return;
    }
    file << text;
  }

  std::string read_text(const std::filesystem::path& path)
  {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  }
} // namespace

TEST_SUITE("diagnostics_file_policy")
{
  TEST_CASE("resolves configured diagnostics roots and falls back when the root is unusable")
  {
    ScopedTempDir temp_dir;
    const auto    fallback_path = temp_dir.path() / "fallback" / "community_patch_navhook_trace.log";

    const auto default_target = ResolveDiagnosticsFileTarget("community_patch_navhook_trace.log", fallback_path, "");
    CHECK(default_target.path == fallback_path);
    CHECK_FALSE(default_target.warning.has_value());

    const auto custom_root   = temp_dir.path() / "native-logs";
    const auto custom_target = ResolveDiagnosticsFileTarget("community_patch_navhook_trace.log", fallback_path,
                                                            custom_root.string());
    CHECK(custom_target.path == custom_root / "community_patch_navhook_trace.log");
    CHECK_FALSE(custom_target.warning.has_value());
    CHECK(std::filesystem::is_directory(custom_root));

    const auto bad_root = temp_dir.path() / "not-a-directory.txt";
    write_text(bad_root, "x");
    const auto fallback_target =
        ResolveDiagnosticsFileTarget("community_patch_navhook_trace.log", fallback_path, bad_root.string());
    CHECK(fallback_target.path == fallback_path);
    REQUIRE(fallback_target.warning.has_value());
    CHECK(fallback_target.warning->find("Falling back") != std::string::npos);
  }

  TEST_CASE("rotates bounded diagnostics files using total-file semantics")
  {
    ScopedTempDir temp_dir;
    const auto    path = temp_dir.path() / "community_patch_navhook_trace.log";

    write_text(path, "current");
    write_text(DiagnosticsRotatedPath(path, 1), "previous");
    write_text(DiagnosticsRotatedPath(path, 2), "oldest");

    const auto prepare = PrepareDiagnosticsFileForAppend(path, 10, 3, 6);
    CHECK(prepare.append_allowed);
    CHECK_FALSE(prepare.warning.has_value());
    CHECK_FALSE(std::filesystem::exists(path));
    CHECK(read_text(DiagnosticsRotatedPath(path, 1)) == "current");
    CHECK(read_text(DiagnosticsRotatedPath(path, 2)) == "previous");
    CHECK_FALSE(std::filesystem::exists(DiagnosticsRotatedPath(path, 3)));
  }

  TEST_CASE("single-file policy truncates the active file before append")
  {
    ScopedTempDir temp_dir;
    const auto    path = temp_dir.path() / "community_patch_action_queue_probe.jsonl";

    write_text(path, "0123456789");
    const auto prepare = PrepareDiagnosticsFileForAppend(path, 10, 1, 1);
    CHECK(prepare.append_allowed);
    CHECK_FALSE(prepare.warning.has_value());
    CHECK_FALSE(std::filesystem::exists(path));
  }

  TEST_CASE("drops a single oversize append to preserve the configured bound")
  {
    ScopedTempDir temp_dir;
    const auto    path = temp_dir.path() / "community_patch_navhook_trace.log";

    write_text(path, "current");
    const auto prepare = PrepareDiagnosticsFileForAppend(path, 10, 3, 11);
    CHECK_FALSE(prepare.append_allowed);
    REQUIRE(prepare.warning.has_value());
    CHECK(prepare.warning->find("Dropping diagnostics append") != std::string::npos);
    CHECK(read_text(path) == "current");
  }
}
