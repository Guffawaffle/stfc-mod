#include "runtime_config_no_host.h"
#include "runtime_config.h"

#include <stdexcept>
#include <utility>

namespace
{
runtime_config::ReloadSnapshot protected_values;
std::size_t                    captures     = 0;
std::size_t                    queue_resets = 0;
bool                           fail_capture = false;
} // namespace

namespace runtime_config_fixture
{
void SetReloadSnapshot(runtime_config::ReloadSnapshot snapshot)
{ protected_values = std::move(snapshot); }
void SetCaptureFailure(bool fail)
{ fail_capture = fail; }
void Reset()
{
  protected_values.clear();
  captures     = 0;
  queue_resets = 0;
  fail_capture = false;
}
std::size_t CaptureCount() noexcept
{ return captures; }
std::size_t QueueResetCount() noexcept
{ return queue_resets; }
} // namespace runtime_config_fixture

void ResetQueueRecoveryTracking()
{ ++queue_resets; }

namespace runtime_config
{
void           Configure(const toml::table&) {}
ReloadSnapshot CaptureReload()
{
  ++captures;
  if (fail_capture)
    throw std::runtime_error("synthetic reload protection capture failure");
  return protected_values;
}
} // namespace runtime_config
