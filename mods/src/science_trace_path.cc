#include "science_trace_path.h"
#include "file.h"

std::filesystem::path ScienceTracePath(std::string_view group, std::string_view filename)
{
  auto directory = std::filesystem::path(File::MakePath("science", true)) / std::filesystem::path(group);
  std::filesystem::create_directories(directory);
  return directory / std::filesystem::path(filename);
}
