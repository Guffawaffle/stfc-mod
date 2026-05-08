#pragma once

#include "patches/input_binding/input_binding.h"

namespace input_binding
{
void SetRuntimeBindingModel(CompileResult compile);
[[nodiscard]] const CompileResult& RuntimeBindingModel();
[[nodiscard]] uint64_t RuntimeBindingGeneration();
} // namespace input_binding