#pragma once

#include <il2cpp/il2cpp_helper.h>

class Bundle;

class ShopShowcaseViewController
{
public:
  __declspec(property(get = __get__isRefining)) bool               _isRefining;
  __declspec(property(get = __get__targetBundle)) Bundle*          _targetBundle;
  __declspec(property(get = __get__targetSecondaryBundle)) Bundle* _targetSecondaryBundle;

  bool __get__isRefining()
  {
    static auto field = get_class_helper().GetField("_isRefining");
    return *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  Bundle* __get__targetBundle()
  {
    static auto field = get_class_helper().GetField("_targetBundle");
    return *reinterpret_cast<Bundle**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  Bundle* __get__targetSecondaryBundle()
  {
    static auto field = get_class_helper().GetField("_targetSecondaryBundle");
    return *reinterpret_cast<Bundle**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopShowcaseViewController");
    return class_helper;
  }
};