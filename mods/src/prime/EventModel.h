#pragma once

#include <il2cpp/il2cpp_helper.h>

#include <optional>

struct EventModel {
  [[nodiscard]] std::optional<int32_t> CategoryValue() const
  {
    auto* object = reinterpret_cast<Il2CppObject*>(const_cast<EventModel*>(this));
    auto* klass  = il2cpp_object_get_class(object);
    auto* field  = klass == nullptr ? nullptr : il2cpp_class_get_field_from_name(klass, "category_");
    if (field == nullptr) {
      return std::nullopt;
    }
    return *reinterpret_cast<const int32_t*>(reinterpret_cast<const char*>(this) + il2cpp_field_get_offset(field));
  }
};
