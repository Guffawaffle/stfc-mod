#include "patches/input_binding/input_runtime_bindings.h"

#include <utility>

namespace input_binding
{
namespace {
CompileResult build_default_runtime_binding_model()
{ return CompileBindingSet(); }

CompileResult& mutable_runtime_binding_model()
{
  static auto model = build_default_runtime_binding_model();
  return model;
}
}

void SetRuntimeBindingModel(CompileResult compile)
{ mutable_runtime_binding_model() = std::move(compile); }

const CompileResult& RuntimeBindingModel()
{ return mutable_runtime_binding_model(); }
} // namespace input_binding