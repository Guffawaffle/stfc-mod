#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <cstdint>

enum class RefineSelectionActionType : int32_t {
  None   = 0,
  Save   = 1,
  Refine = 2,
};

class RefineSelectionAnalyticsObject
{
public:
  __declspec(property(get = __get_ActionType)) RefineSelectionActionType ActionType;
  __declspec(property(get = __get_RefineCategory)) Il2CppString*         RefineCategory;
  __declspec(property(get = __get_RefineAmount)) int32_t                 RefineAmount;
  __declspec(property(get = __get_UserId)) Il2CppString*                 UserId;

  RefineSelectionActionType __get_ActionType()
  {
    static auto field = get_class_helper().GetField("<ActionType>k__BackingField");
    return *reinterpret_cast<RefineSelectionActionType*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  Il2CppString* __get_RefineCategory()
  {
    static auto field = get_class_helper().GetField("<RefineCategory>k__BackingField");
    return *reinterpret_cast<Il2CppString**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  int32_t __get_RefineAmount()
  {
    static auto field = get_class_helper().GetField("<RefineAmount>k__BackingField");
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  Il2CppString* __get_UserId()
  {
    static auto field = get_class_helper().GetField("<UserId>k__BackingField");
    return *reinterpret_cast<Il2CppString**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop.RefineSelection.Analytics",
                                                       "RefineSelectionAnalyticsObject");
    return class_helper;
  }
};