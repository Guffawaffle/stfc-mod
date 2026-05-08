#include <doctest/doctest.h>

#include "patches/il2cpp_safety.h"

TEST_SUITE("il2cpp_safety")
{
  TEST_CASE("array index bounds helper only allows in-range access")
  {
    CHECK_FALSE(il2cpp_array_index_is_valid(0, 0));
    CHECK(il2cpp_array_index_is_valid(1, 0));
    CHECK(il2cpp_array_index_is_valid(3, 2));
    CHECK_FALSE(il2cpp_array_index_is_valid(3, 3));
    CHECK_FALSE(il2cpp_array_index_is_valid(3, 99));
  }
}