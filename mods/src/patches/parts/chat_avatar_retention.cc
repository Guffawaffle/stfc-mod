// Retain chat portraits while their widgets are reused across screen transitions.
#include <cstdlib>
#include <cstring>
#include <il2cpp-tabledefs.h>
#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#if _WIN32
#include <Windows.h>
#elif __APPLE__
#include <dlfcn.h>
#endif

namespace
{
FieldInfo *  profileField{}, *avatarField{}, *adminField{}, *keepField{};
Il2CppClass* avatarClass{};

[[noreturn]] void Fail(const char* detail)
{
  spdlog::critical("[ChatAvatarRetention] required hook contract failed: {}", detail);
  spdlog::default_logger()->flush();
  std::abort();
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
      || field->offset + size > il2cpp_class_instance_size(cls) || !IsType(field->type, type))
    Fail(name);
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
  if (il2cpp_object_get_class(avatar) != avatarClass)
    Fail("unexpected avatar class");
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

bool NativeMatches(const void* pointer)
{
#if defined(_WIN32) && defined(_M_X64)
  // Windows265: native extent 2691 bytes; 25-byte relocation window.
  constexpr uintptr_t     rva      = 0x11cd040;
  constexpr unsigned char window[] = {0x40, 0x55, 0x48, 0x81, 0xec, 0x80, 0x00, 0x00, 0x00, 0x80, 0x3d, 0x0e, 0xe3,
                                      0xcd, 0x04, 0x00, 0x48, 0x8b, 0xe9, 0x0f, 0x85, 0xb1, 0x00, 0x00, 0x00};
#elif defined(__APPLE__) && defined(__aarch64__)
  // Mac199: native extent 1376 bytes; 32-byte relocation window.
  constexpr uintptr_t     rva      = 0xfbd748;
  constexpr unsigned char window[] = {0xff, 0x83, 0x01, 0xd1, 0xf8, 0x5f, 0x02, 0xa9, 0xf6, 0x57, 0x03,
                                      0xa9, 0xf4, 0x4f, 0x04, 0xa9, 0xfd, 0x7b, 0x05, 0xa9, 0xfd, 0x43,
                                      0x01, 0x91, 0xf3, 0x03, 0x00, 0xaa, 0x74, 0x67, 0x02, 0xd0};
#elif defined(__APPLE__) && defined(__x86_64__)
  // Mac199: native extent 1424 bytes; 27-byte relocation window.
  constexpr uintptr_t     rva      = 0xf60350;
  constexpr unsigned char window[] = {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41,
                                      0x55, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x28, 0x48,
                                      0x89, 0xfb, 0x80, 0x3d, 0x07, 0x3e, 0xc8, 0x04, 0x00};
#else
  return false;
#endif
#if defined(_WIN32) && defined(_M_X64)
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
#elif defined(__APPLE__) && (defined(__aarch64__) || defined(__x86_64__))
  Dl_info image{};
  if (!pointer || !dladdr(pointer, &image))
    return false;
  const auto base = reinterpret_cast<uintptr_t>(image.dli_fbase);
#endif
#if (defined(_WIN32) && defined(_M_X64)) || (defined(__APPLE__) && (defined(__aarch64__) || defined(__x86_64__)))
  return base && pointer && reinterpret_cast<uintptr_t>(pointer) == base + rva
         && std::memcmp(pointer, window, sizeof(window)) == 0;
#endif
}
} // namespace

void InstallChatAvatarRetention()
{
  auto chat    = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Chat", "ChatMessageWidget");
  auto profile = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlayerProfile", "UserProfileWidget");
  auto avatar  = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.PlayerAvatars", "FrameAndAvatarWidget");
  avatarClass  = avatar.get_cls();
  if (!avatarClass)
    Fail("FrameAndAvatarWidget");
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
  const auto* method = chat.GetMethodInfo("SetWidgetData", 0);
  if (!method || method->klass != chat.get_cls() || !method->methodPointer || method->is_generic || method->is_inflated
      || (method->flags & METHOD_ATTRIBUTE_STATIC) || method->parameters_count != 0
      || !IsType(method->return_type, "System.Void") || !NativeMatches(reinterpret_cast<void*>(method->methodPointer)))
    Fail("ChatMessageWidget.SetWidgetData");
  if (!SPUD_STATIC_DETOUR(method->methodPointer, SetWidgetData))
    Fail("detour installation");
  spdlog::info("[ChatAvatarRetention] chat portrait retention enabled");
}
