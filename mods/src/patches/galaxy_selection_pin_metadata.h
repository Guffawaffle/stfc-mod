#pragma once
#include <il2cpp-class-internals.h>
#include <il2cpp-tabledefs.h>

namespace galaxy_selection
{
// These accessors are managed-invoked, not detoured. An inflated declaring
// class (List<POI>) is valid; an open generic method is not.
inline bool PinCountGetter(const MethodInfo* method)
{
  return method && !(method->flags & METHOD_ATTRIBUTE_STATIC) && !method->is_generic
         && method->parameters_count == 0 && method->return_type && !method->return_type->byref
         && method->return_type->type == IL2CPP_TYPE_I4;
}
inline bool PinItemGetter(const MethodInfo* method)
{
  return method && !(method->flags & METHOD_ATTRIBUTE_STATIC) && !method->is_generic
         && method->parameters_count == 1 && method->parameters && method->parameters[0]
         && !method->parameters[0]->byref && method->parameters[0]->type == IL2CPP_TYPE_I4;
}
} // namespace galaxy_selection
