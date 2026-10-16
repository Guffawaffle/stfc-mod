#pragma once

#include <filesystem>
#include <string_view>

// Group science output beneath the normal mod data location on each platform.
// Call inside the writer's initialization try/catch so a filesystem error only
// disables that capture. Groups and filenames are compile-time caller literals.
std::filesystem::path ScienceTracePath(std::string_view group, std::string_view filename);
