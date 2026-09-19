#pragma once

namespace native_hooks
{
// Validates the entry in the loaded image, before SPUD changes any bytes.
// Unsupported images/architectures fail closed.
bool MacHookFits(const void* method);
} // namespace native_hooks
