#pragma once

#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>

#include <spdlog/spdlog.h>

enum class VisibilityState // TypeDefIndex: 13743
{
  Unknown = 0,
  Show    = 1,
  Hide    = 2,
  Hidden  = 3,
  Visible = 4
};

struct VisibilityController {
public:
  __declspec(property(get = __get__state)) VisibilityState _state;
  __declspec(property(get = __get_State)) VisibilityState  State;

  void Show(bool instant = false)
  {
    static const auto show = ResolveVisibilityMethod("Show");
    if (show)
      show(this, instant);
  }

  void Hide(bool instant = false)
  {
    static const auto hide = ResolveVisibilityMethod("Hide");
    if (hide)
      hide(this, instant);
  }

private:
  using VisibilityMethod = void (*)(VisibilityController*, bool);

  // The checked native entry points live for the IL2CPP process lifetime. Cache
  // only method metadata, so pooling or replacing a controller needs no reset.
  static VisibilityMethod ResolveVisibilityMethod(const char* name)
  {
    const auto* method =
        method_contract::Resolve(get_class_helper().get_cls(), name, false, "System.Void", {"System.Boolean"});
    if (!method)
      spdlog::warn("VisibilityController: compatible instance void {}(bool) method unavailable", name);
    return reinterpret_cast<VisibilityMethod>(method_contract::Pointer(method));
  }

  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "VisibilityController");
    return class_helper;
  }

public:
  VisibilityState __get__state()
  {
    static auto field = get_class_helper().GetField("_state");
    if (field.isValidHelper()) {
      return *(VisibilityState*)((ptrdiff_t)this + field.offset());
    }

    return VisibilityState::Unknown;
  }

  VisibilityState __get_State()
  {
    static auto prop = get_class_helper().GetProperty("VisibilityState");
    if (prop.isValidHelper()) {
      return *prop.Get<VisibilityState>(this);
    }

    return VisibilityState::Unknown;
  }
};
