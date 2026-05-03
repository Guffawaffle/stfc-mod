#pragma once

#include <cstdint>

struct ShopCategory {
public:
  __declspec(property(get = __get_Value)) int32_t Value;

  int32_t __get_Value() const
  { return _flagValue; }

private:
  int32_t _flagValue = 0;
};