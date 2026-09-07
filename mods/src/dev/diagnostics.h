#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dev::diagnostics
{
using SourceId = uint32_t;

constexpr SourceId invalid_source = 0;

enum class Severity : uint8_t {
  Trace,
  Info,
  Warning,
  Error,
};

struct Entry {
  uint64_t    sequence   = 0;
  int64_t     elapsed_ms = 0;
  SourceId    source_id  = invalid_source;
  Severity    severity   = Severity::Info;
  std::string source;
  std::string message;
};

struct Snapshot {
  std::vector<Entry> entries;
  uint64_t           dropped = 0;
};

#ifdef _MODDBG
SourceId RegisterSource(std::string_view owner, bool enabled = true);
bool     SetSourceEnabled(SourceId source, bool enabled);
bool     IsAwake(SourceId source = invalid_source);
void     SetAwake(bool awake);
void     Clear();
void     Publish(SourceId source, Severity severity, std::string message);
Snapshot ReadSnapshot();

template <typename Builder> void PublishLazy(SourceId source, Severity severity, Builder&& builder)
{
  if (!IsAwake(source)) {
    return;
  }
  Publish(source, severity, std::invoke(std::forward<Builder>(builder)));
}
#else
inline SourceId RegisterSource(std::string_view, bool = true)
{ return invalid_source; }
inline bool SetSourceEnabled(SourceId, bool)
{ return false; }
inline bool IsAwake(SourceId = invalid_source)
{ return false; }
inline void                      SetAwake(bool) {}
inline void                      Clear() {}
inline void                      Publish(SourceId, Severity, std::string) {}
template <typename Builder> void PublishLazy(SourceId, Severity, Builder&&) {}
inline Snapshot                  ReadSnapshot()
{ return {}; }
#endif
} // namespace dev::diagnostics
