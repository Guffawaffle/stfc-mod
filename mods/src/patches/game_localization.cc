#include "patches/game_localization.h"

#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>

#include <mutex>

namespace game_localization
{
namespace
{
  struct Cache {
    Il2CppClass*      locale_text_context         = nullptr;
    const MethodInfo* locale_text_context_ctor    = nullptr;
    const MethodInfo* apply_identifier_parameters = nullptr;
    const MethodInfo* locale_utilities_localize   = nullptr;
    Il2CppClass*      object_array                = nullptr;
    Il2CppClass*      int64_class                 = nullptr;

    [[nodiscard]] bool can_localize_context() const
    { return locale_utilities_localize != nullptr; }
    [[nodiscard]] bool can_localize_identifier() const
    {
      return locale_text_context && locale_text_context_ctor && apply_identifier_parameters && object_array
             && can_localize_context();
    }
  };

  Cache& cache()
  {
    static Cache value;
    return value;
  }

  std::once_flag initialize_once;

  std::string LocalizeIdentifierObject(std::string_view identifier, std::string_view category, Il2CppObject* parameter)
  {
    Initialize();
    auto& values = cache();
    if (!values.can_localize_identifier() || !parameter) {
      return {};
    }

    auto* id             = il2cpp_string_new(std::string(identifier).c_str());
    auto* category_value = il2cpp_string_new(std::string(category).c_str());
    auto* parameters     = il2cpp_array_new_specific(values.object_array, 1);
    auto* context        = il2cpp_object_new(values.locale_text_context);
    if (!id || !category_value || !parameters || !context) {
      return {};
    }

    reinterpret_cast<Il2CppObject**>(reinterpret_cast<Il2CppArraySize*>(parameters)->vector)[0] = parameter;

    Il2CppException* exception    = nullptr;
    void*            ctor_args[2] = {id, category_value};
    il2cpp_runtime_invoke(values.locale_text_context_ctor, context, ctor_args, &exception);
    if (exception) {
      return {};
    }

    void* apply_args[1] = {parameters};
    il2cpp_runtime_invoke(values.apply_identifier_parameters, context, apply_args, &exception);
    return exception ? std::string{} : LocalizeContext(context, true);
  }
} // namespace

void Initialize()
{
  std::call_once(initialize_once, [] {
    auto& values = cache();

    auto locale_context = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "LocaleTextContext");
    if (locale_context.isValidHelper()) {
      values.locale_text_context         = locale_context.get_cls();
      values.locale_text_context_ctor    = locale_context.GetMethodInfo(".ctor", 2);
      values.apply_identifier_parameters = locale_context.GetMethodInfo("ApplyIdentifierParameters", 1);
    }

    auto locale_utilities = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Localization", "LocaleUtilities");
    if (locale_utilities.isValidHelper()) {
      void* iterator = nullptr;
      while (auto* method = il2cpp_class_get_methods(locale_utilities.get_cls(), &iterator)) {
        if (std::string_view(il2cpp_method_get_name(method)) == "Localize"
            && il2cpp_method_get_param_count(method) == 3) {
          values.locale_utilities_localize = method;
          break;
        }
      }
    }

    auto object = il2cpp_get_class_helper("mscorlib", "System", "Object");
    if (object.isValidHelper()) {
      values.object_array = il2cpp_array_class_get(object.get_cls(), 1);
    }
    auto int64 = il2cpp_get_class_helper("mscorlib", "System", "Int64");
    if (int64.isValidHelper()) {
      values.int64_class = int64.get_cls();
    }

    if (!values.can_localize_context()) {
      spdlog::warn("[Localization] LocaleUtilities.Localize is unavailable; callers will use their fallbacks");
    }
  });
}

std::string LocalizeContext(void* locale_text_context, const bool localize_text_parameters)
{
  Initialize();
  auto& values = cache();
  if (!locale_text_context || !values.can_localize_context()) {
    return {};
  }

  auto             localize_parameters = localize_text_parameters;
  auto             invariant_culture   = false;
  void*            args[3]             = {locale_text_context, &localize_parameters, &invariant_culture};
  Il2CppException* exception           = nullptr;
  auto*            result = il2cpp_runtime_invoke(values.locale_utilities_localize, nullptr, args, &exception);
  return exception || !result ? std::string{} : to_string(reinterpret_cast<Il2CppString*>(result));
}

std::string LocalizeIdentifier(const std::string_view identifier, const std::string_view category,
                               const int64_t parameter)
{
  Initialize();
  auto& values = cache();
  if (!values.int64_class) {
    return {};
  }
  auto boxed_parameter = parameter;
  return LocalizeIdentifierObject(identifier, category, il2cpp_value_box(values.int64_class, &boxed_parameter));
}

std::string LocalizeIdentifier(const std::string_view identifier, const std::string_view category,
                               const std::string_view parameter)
{
  auto* value = il2cpp_string_new(std::string(parameter).c_str());
  return value ? LocalizeIdentifierObject(identifier, category, reinterpret_cast<Il2CppObject*>(value)) : std::string{};
}
} // namespace game_localization
