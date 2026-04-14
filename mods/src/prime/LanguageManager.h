#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "MonoSingleton.h"

// Digit.Client.Localization.LanguageCategory (VALUE TYPE — 16 bytes)
//   Name    : string @ 0x0
//   Dynamic : bool   @ 0x8
// When iterating List<LanguageCategory>._items, stride = element_size (16), NOT sizeof(void*).

// Digit.Client.Localization.LanguageManager : MonoSingleton<LanguageManager>
//
// Il2CppDumper field layout:
//   _categories                    : List<LanguageCategory>       @ 0x20
//   _numberPlaceholderLocaleSettings : ...                        @ 0x28
//   _staticCategories              : List<LanguageCategory>       @ 0x30
//   _dynamicCategories             : List<LanguageCategory>       @ 0x38
//   _debugMode                     : DebugModeTypes               @ 0x40
//   _localeDB                      : LocalizationCacheDatabase    @ 0x48
//   _updatedStrings                : LinkedList<StringIntTuple>   @ 0x50
//   _activeLocalisers              : HashSet<TextLocalizer>       @ 0x58
//   _saveCoroutine                 : Coroutine                    @ 0x60
//   _lastSaveTimestamp             : float                        @ 0x68
//   _selectedLanguageString        : string                       @ 0x70
class LanguageManager : public MonoSingleton<LanguageManager>
{
public:
  // Existing: TryGetTranslation(string category, string key, out string) — uses string key
  bool TryGetTranslation(Il2CppString *category, Il2CppString *key, Il2CppString **translatedText)
  {
    static auto TryGetTranslation =
        get_class_helper().GetMethod<bool(LanguageManager *, Il2CppString *, Il2CppString *, Il2CppString **)>(
            "TryGetTranslation");
    return TryGetTranslation(this, category, key, translatedText);
  }

private:
  friend struct MonoSingleton<LanguageManager>;
  static IL2CppClassHelper &get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Localization", "LanguageManager");
    return class_helper;
  }
};