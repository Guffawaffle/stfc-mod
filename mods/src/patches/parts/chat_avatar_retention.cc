// Retain chat portraits while their widgets are reused across screen transitions.
#include <atomic>
#include <cstring>
#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

namespace
{
FieldInfo *  profileField{}, *avatarField{}, *adminField{}, *keepField{};
Il2CppClass* avatarClass{};

void ReportFailure(const char* detail)
{
  spdlog::critical("[ChatAvatarRetention] unavailable: {}", detail);
}

bool IsType(const Il2CppType* type, const char* expected)
{
  if (!type || type->byref)
    return false;
  auto*      name    = il2cpp_type_get_name(type);
  const bool matches = name && std::strcmp(name, expected) == 0;
  il2cpp_free(name);
  return matches;
}

FieldInfo* Field(Il2CppClass* cls, const char* name, const char* type, size_t size)
{
  auto* field = cls ? il2cpp_class_get_field_from_name(cls, name) : nullptr;
  if (!field || (il2cpp_field_get_flags(field) & FIELD_ATTRIBUTE_STATIC) || field->offset < sizeof(Il2CppObject)
      || field->offset + size > il2cpp_class_instance_size(cls) || !IsType(field->type, type)) {
    ReportFailure(name);
    return nullptr;
  }
  return field;
}

Il2CppObject* Read(Il2CppObject* self, FieldInfo* field)
{
  Il2CppObject* value{};
  if (self)
    il2cpp_field_get_value(self, field, &value);
  return value;
}

void Retain(Il2CppObject* avatar)
{
  if (!avatar)
    return;
  if (il2cpp_object_get_class(avatar) != avatarClass) {
    static std::atomic_flag reported = ATOMIC_FLAG_INIT;
    if (!reported.test_and_set(std::memory_order_relaxed)) {
      ReportFailure("unexpected avatar class");
    }
    return;
  }
  bool keep = true;
  il2cpp_field_set_value(avatar, keepField, &keep);
}

void SetWidgetData(auto original, Il2CppObject* self)
{
  // Set before native child binding/download. Keep it set for later widget reloads;
  // restoring it on return would leave asynchronous reloads vulnerable again.
  auto* avatar = Read(Read(self, profileField), avatarField);
  auto* admin  = Read(self, adminField);
  Retain(avatar);
  if (admin != avatar)
    Retain(admin);
  original(self);
}

} // namespace

void InstallChatAvatarRetention()
{
  auto chat    = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatMessageWidget");
  auto profile = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlayerProfile", "UserProfileWidget");
  auto avatar  = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlayerAvatars", "FrameAndAvatarWidget");
  avatarClass  = avatar.get_cls();
  if (!avatarClass) {
    ReportFailure("FrameAndAvatarWidget");
    return;
  }
  // Older clients always retained avatar downloads and do not expose this setting.
  if (!il2cpp_class_get_field_from_name(avatarClass, "_keepInCache")) {
    spdlog::info("[ChatAvatarRetention] client has no serialized retention setting; native behavior retained");
    return;
  }
  profileField =
      Field(chat.get_cls(), "_userProfileWidget", "Digit.Prime.PlayerProfile.UserProfileWidget", sizeof(void*));
  avatarField = Field(profile.get_cls(), "_profileFrameAndAvatar", "Digit.Prime.PlayerAvatars.FrameAndAvatarWidget",
                      sizeof(void*));
  adminField =
      Field(chat.get_cls(), "_adminAvatarWidget", "Digit.Prime.PlayerAvatars.FrameAndAvatarWidget", sizeof(void*));
  keepField          = Field(avatarClass, "_keepInCache", "System.Boolean", sizeof(bool));
  if (!profileField || !avatarField || !adminField || !keepField)
    return;
  const auto* method = chat.GetMethodInfo("SetWidgetData", 0);
  if (!method || method->klass != chat.get_cls() || !method->methodPointer || method->is_generic || method->is_inflated
      || (method->flags & METHOD_ATTRIBUTE_STATIC) || method->parameters_count != 0
      || !IsType(method->return_type, "System.Void")) {
    ReportFailure("ChatMessageWidget.SetWidgetData contract");
    return;
  }
  if (!SPUD_STATIC_DETOUR(method->methodPointer, SetWidgetData)) {
    ReportFailure("detour installation");
    return;
  }
  spdlog::info("[ChatAvatarRetention] chat portrait retention enabled");
}
