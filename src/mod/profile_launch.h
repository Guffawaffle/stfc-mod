#pragma once

// The full mod owns one early initializer. Named launches prepare before the
// runtime starts and install mandatory isolation before account startup resumes.

#include <filesystem>

namespace profile_launch
{
void                         Initialize();
bool                         Requested();
void                         Prepare();
void                         Install();
const std::filesystem::path& Directory();
[[noreturn]] void            Abort(const char* reason) noexcept;
} // namespace profile_launch
