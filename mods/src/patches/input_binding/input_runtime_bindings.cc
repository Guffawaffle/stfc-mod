#include "patches/input_binding/input_runtime_bindings.h"

#include <utility>

namespace input_binding
{
namespace {
CompileResult build_default_runtime_binding_model()
{ return CompileBindingSet(); }

uint64_t& mutable_runtime_binding_generation()
{
  static uint64_t generation = 1;
  return generation;
}

CompileResult& mutable_runtime_binding_model()
{
  static auto model = build_default_runtime_binding_model();
  return model;
}
}

void SetRuntimeBindingModel(CompileResult compile)
{
  mutable_runtime_binding_model() = std::move(compile);
  ++mutable_runtime_binding_generation();
}

const CompileResult& RuntimeBindingModel()
{ return mutable_runtime_binding_model(); }

uint64_t RuntimeBindingGeneration()
{ return mutable_runtime_binding_generation(); }
} // namespace input_binding