#pragma once

#include <filesystem>
#include <string_view>

namespace profile_launch
{
// Required installation admission precedes assembly load. Named preparation precedes all file/configuration
// access; required native hooks complete after successful original initialization and before optional features.
void                         Initialize() noexcept;
void                         Prepare() noexcept;
bool                         RuntimeInitialized(bool success) noexcept;
void                         Install() noexcept;
bool                         Requested() noexcept;
std::string_view             Id() noexcept;
const std::filesystem::path& Directory();
[[noreturn]] void            Abort(const char* reason) noexcept;
} // namespace profile_launch
