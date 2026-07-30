#include "patches/hook_registry.h"

#include "prime/IList.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>

#include <string>
#include <string_view>

namespace
{
struct OfficerSortGenerators {
};

constexpr std::string_view kBelowDeckSortDisplaySuffix = "_below_deck_ability";
constexpr std::string_view kBelowDeckSortLabel         = "Below Deck Ability";

constexpr HookDescriptor kInitializeAssignmentSortersHook{
    "OfficerSortGenerators.InitializeAssignmentSorters",
    "Restore the removed Below Deck Ability assignment sort through the game's existing assignment sort generator.",
    {"Assembly-CSharp", "Digit.Prime.Officers", "OfficerSortGenerators", "InitializeAssignmentSorters"},
    "The Below Deck Ability option will be absent from the Manage Ship officer-assignment sort dropdown.",
    HookSupportTier::Production};

IL2CppClassHelper& OfficerSortGeneratorsHelper()
{
  static auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Officers", "OfficerSortGenerators");
  return helper;
}

Il2CppObject* ReadReferenceField(Il2CppObject* object, IL2CppClassHelper& helper, const char* field_name)
{
  if (!object || !helper.isValidHelper()) {
    return nullptr;
  }

  auto field = helper.GetField(field_name);
  if (!field.isValidHelper() || field.offset() < 0) {
    return nullptr;
  }

  return *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(object) + field.offset());
}

IList* AssignmentOptions(OfficerSortGenerators* generators)
{
  auto& helper = OfficerSortGeneratorsHelper();
  if (!generators || !helper.isValidHelper()) {
    return nullptr;
  }

  auto field = helper.GetField("_assignmentOptions");
  if (!field.isValidHelper() || field.offset() < 0) {
    return nullptr;
  }

  return *reinterpret_cast<IList**>(reinterpret_cast<char*>(generators) + field.offset());
}

bool HasBelowDeckAssignmentSort(IList* options)
{
  if (!options) {
    return false;
  }

  for (auto index = 0; index < options->Count; ++index) {
    auto* option = options->Get(index);
    if (!option || !option->klass) {
      continue;
    }

    auto helper            = IL2CppClassHelper{option->klass};
    auto display_key_field = helper.GetField("_displayKey");
    if (!display_key_field.isValidHelper() || display_key_field.offset() < 0) {
      continue;
    }

    auto* display_key = *reinterpret_cast<Il2CppString**>(reinterpret_cast<char*>(option) + display_key_field.offset());
    if (display_key) {
      const auto value = to_string(display_key);
      if (value.ends_with(kBelowDeckSortDisplaySuffix)) {
        return true;
      }
    }
  }

  return false;
}

Il2CppObject* FindSampleAssignmentComparatorDelegate(IList* options)
{
  if (!options) {
    spdlog::warn("[OfficerAssignmentSort] sample delegate lookup has no assignment options");
    return nullptr;
  }

  for (auto index = 0; index < options->Count; ++index) {
    auto* option = options->Get(index);
    if (!option || !option->klass) {
      continue;
    }

    auto  processing_option = IL2CppClassHelper{option->klass};
    auto* sort              = ReadReferenceField(option, processing_option, "_sortingOption");
    if (!sort || !sort->klass) {
      continue;
    }

    auto  sort_function = IL2CppClassHelper{sort->klass};
    auto* sample        = ReadReferenceField(sort, sort_function, "_ascending");
    if (sample) {
      spdlog::debug("[OfficerAssignmentSort] borrowed comparator delegate from assignment option {}", index);
      return sample;
    }
  }

  spdlog::warn("[OfficerAssignmentSort] no existing assignment option exposed an ascending comparator (options={})",
               options->Count);
  return nullptr;
}

Il2CppObject* CreateBelowDeckComparatorDelegate(Il2CppObject* sample_delegate)
{
  if (!sample_delegate || !sample_delegate->klass) {
    return nullptr;
  }

  auto sorting_predicates = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "SortingPredicates");
  auto delegate_helper    = il2cpp_get_class_helper("mscorlib", "System", "Delegate");
  if (!sorting_predicates.isValidHelper() || !delegate_helper.isValidHelper()) {
    spdlog::warn("[OfficerAssignmentSort] delegate helpers unavailable (sorting_predicates={} delegate={})",
                 sorting_predicates.isValidHelper(), delegate_helper.isValidHelper());
    return nullptr;
  }

  const auto* comparator      = sorting_predicates.GetMethodInfo("OfficerSortByBelowDeckAbilityAscending", 2);
  const auto* create_delegate = delegate_helper.GetMethodInfo("CreateDelegate", 2);
  if (!comparator || !create_delegate) {
    spdlog::warn("[OfficerAssignmentSort] delegate methods unavailable (comparator={} create_delegate={})",
                 comparator != nullptr, create_delegate != nullptr);
    return nullptr;
  }

  auto* delegate_type = il2cpp_type_get_object(il2cpp_class_get_type(sample_delegate->klass));
  auto* method_object = il2cpp_method_get_object(comparator, comparator->klass);
  if (!delegate_type || !method_object) {
    spdlog::warn("[OfficerAssignmentSort] delegate reflection objects unavailable (type={} method={})",
                 delegate_type != nullptr, method_object != nullptr);
    return nullptr;
  }

  void*            arguments[2] = {delegate_type, method_object};
  Il2CppException* exception    = nullptr;
  auto*            result       = il2cpp_runtime_invoke(create_delegate, nullptr, arguments, &exception);
  if (exception) {
    spdlog::warn("[OfficerAssignmentSort] Delegate.CreateDelegate raised a managed exception");
    return nullptr;
  }
  if (!result) {
    spdlog::warn("[OfficerAssignmentSort] Delegate.CreateDelegate returned null");
  }

  return result;
}

Il2CppObject* BuildBelowDeckAssignmentSort(OfficerSortGenerators* generators, Il2CppObject* comparator_delegate)
{
  if (!generators || !comparator_delegate || !comparator_delegate->klass) {
    return nullptr;
  }

  const auto* generate_sort = OfficerSortGeneratorsHelper().GetMethodInfo("StandardAssignmentSortFunction", 1);
  if (!generate_sort) {
    spdlog::warn("[OfficerAssignmentSort] StandardAssignmentSortFunction method unavailable");
    return nullptr;
  }

  void*            arguments[1] = {comparator_delegate};
  Il2CppException* exception    = nullptr;
  auto*            result       = il2cpp_runtime_invoke(generate_sort, generators, arguments, &exception);
  if (exception) {
    spdlog::warn("[OfficerAssignmentSort] StandardAssignmentSortFunction raised a managed exception");
    return nullptr;
  }
  if (!result) {
    spdlog::warn("[OfficerAssignmentSort] StandardAssignmentSortFunction returned null");
  }

  return result;
}

bool AppendBelowDeckAssignmentSort(OfficerSortGenerators* generators, Il2CppObject* sort_function)
{
  if (!generators || !sort_function) {
    return false;
  }

  const auto* add_sorter  = OfficerSortGeneratorsHelper().GetMethodInfo("AddAssignmentSorter", 2);
  auto*       display_key = il2cpp_string_new(kBelowDeckSortDisplaySuffix.data());
  if (!add_sorter || !display_key) {
    spdlog::warn("[OfficerAssignmentSort] unable to prepare AddAssignmentSorter (method={} display_key={})",
                 add_sorter != nullptr, display_key != nullptr);
    return false;
  }

  void*            arguments[2] = {display_key, sort_function};
  Il2CppException* exception    = nullptr;
  il2cpp_runtime_invoke(add_sorter, generators, arguments, &exception);
  if (exception) {
    spdlog::warn("[OfficerAssignmentSort] AddAssignmentSorter raised a managed exception");
    return false;
  }

  return true;
}

void RestoreBelowDeckAssignmentSort(OfficerSortGenerators* generators)
{
  auto* options = AssignmentOptions(generators);
  if (!options) {
    spdlog::warn("[OfficerAssignmentSort] assignment options unavailable after initialization");
    return;
  }
  if (HasBelowDeckAssignmentSort(options)) {
    spdlog::debug("[OfficerAssignmentSort] Below Deck Ability sort already present");
    return;
  }

  auto* sample_delegate = FindSampleAssignmentComparatorDelegate(options);
  if (!sample_delegate) {
    return;
  }

  auto* comparator = CreateBelowDeckComparatorDelegate(sample_delegate);
  if (!comparator) {
    return;
  }

  auto* sort_function = BuildBelowDeckAssignmentSort(generators, comparator);
  if (!sort_function) {
    return;
  }

  const auto previous_count = options->Count;
  if (!AppendBelowDeckAssignmentSort(generators, sort_function) || options->Count != previous_count + 1) {
    spdlog::warn("[OfficerAssignmentSort] game did not append Below Deck Ability sort");
    return;
  }

  spdlog::info("[OfficerAssignmentSort] restored '{}' assignment sort (display_suffix='{}' options={})",
               kBelowDeckSortLabel, kBelowDeckSortDisplaySuffix, options->Count);
}

void OfficerSortGenerators_InitializeAssignmentSorters_Hook(auto original, OfficerSortGenerators* generators)
{
  original(generators);
  RestoreBelowDeckAssignmentSort(generators);
}
} // namespace

void InstallOfficerAssignmentSortHooks()
{
  HookModuleHealth hooks("OfficerAssignmentSort");

  auto& helper = OfficerSortGeneratorsHelper();
  if (!helper.isValidHelper()) {
    hooks.record_missing_helper(kInitializeAssignmentSortersHook);
  } else if (auto initialize = helper.GetMethod("InitializeAssignmentSorters", 0); initialize == nullptr) {
    hooks.record_missing_method(kInitializeAssignmentSortersHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kInitializeAssignmentSortersHook, initialize,
                                     OfficerSortGenerators_InitializeAssignmentSorters_Hook);
  }

  hooks.log_summary();
}
