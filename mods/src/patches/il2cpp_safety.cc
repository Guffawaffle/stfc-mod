#include "patches/il2cpp_safety.h"

bool il2cpp_array_index_is_valid(const size_t array_length, const size_t index)
{
  return index < array_length;
}