#pragma once

#include <cstdint>
#include <memory>

// Read-only science capture. Existing feature detours register ownership to avoid double hooking.
namespace queue_science
{
void Own(const char* method, bool installed);
void Install();

class Scope
{
  struct State;
  std::unique_ptr<State> state_;

public:
  Scope(void* manager, const char* source, std::int64_t target = 0, bool force = false) noexcept;
  ~Scope() noexcept;
  void Note(const char* key, std::int64_t value) noexcept;
  void Object(const char* key, void* value, bool list = false) noexcept;
  Scope(const Scope&)            = delete;
  Scope& operator=(const Scope&) = delete;
};
} // namespace queue_science
