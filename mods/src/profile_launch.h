#pragma once

#include <filesystem>

namespace profile_launch {

// Called by the native bootstrap before Unity's installation-wide admission.
void Initialize();
bool Requested();
// The mod's one il2cpp_init hook prepares before original and installs after it.
void Prepare();
void Install();
const std::filesystem::path& Directory();
[[noreturn]] void Abort(const char* reason);

} // namespace profile_launch
