#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

class ShopSectionContext;

enum class BundleQuantitySelectionInfoState : int32_t {
  NotSelected       = 0,
  NotAffordable     = 1,
  InCooldown        = 2,
  AvailableToRefine = 3,
};

class BundleQuantitySelectionWidget
{
public:
  __declspec(property(get = __get__shopSectionContext)) ShopSectionContext*              _shopSectionContext;
  __declspec(property(get = __get__selectionInfoState)) BundleQuantitySelectionInfoState _selectionInfoState;

  ShopSectionContext* __get__shopSectionContext()
  {
    static auto field = get_class_helper().GetField("_shopSectionContext");
    return *reinterpret_cast<ShopSectionContext**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  BundleQuantitySelectionInfoState __get__selectionInfoState()
  {
    static auto field = get_class_helper().GetField("_selectionInfoState");
    return *reinterpret_cast<BundleQuantitySelectionInfoState*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop.RefineSelection", "BundleQuantitySelectionWidget");
    return class_helper;
  }
};