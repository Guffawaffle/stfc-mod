#pragma once

#include <prime/Hub.h>

#include <cstdint>

class Bundle;
class RefineSelectionContainer;
class RefineSelectionAnalyticsObject;

enum class CancelRefineSelectionReason : int32_t {
  Back           = 0,
  Shop           = 1,
  Cancel         = 2,
  Mantis         = 3,
  MantisTutorial = 4,
  Bundle         = 5,
};

class ShopSectionContext
{
public:
  __declspec(property(get = __get_CurTransactionBundle)) Bundle*                           CurTransactionBundle;
  __declspec(property(get = __get_BundleToViewCache)) Bundle*                              BundleToViewCache;
  __declspec(property(get = __get__transactionInProgress)) bool                            _transactionInProgress;
  __declspec(property(get = __get__fetchingBundlesInProgress)) bool                        _fetchingBundlesInProgress;
  __declspec(property(get =
                          __get__cancelRefineSelectionReason)) CancelRefineSelectionReason _cancelRefineSelectionReason;
  __declspec(property(get = __get__isInRefineSelectionMode)) bool                          _isInRefineSelectionMode;
  __declspec(property(get = __get__isInCancelSelectionMode)) bool                          _isInCancelSelectionMode;
  __declspec(property(get = __get__refineSelectionContainer)) RefineSelectionContainer*    _refineSelectionContainer;
  __declspec(property(get = __get__refineSelectionAnalyticsObject))
  RefineSelectionAnalyticsObject*                                   _refineSelectionAnalyticsObject;
  __declspec(property(get = __get_TargetSecondaryBundle)) Bundle*   TargetSecondaryBundle;
  __declspec(property(get = __get_TargetBundleOverride)) Bundle*    TargetBundleOverride;
  __declspec(property(get = __get_SelectedStoreGroupIndex)) int32_t SelectedStoreGroupIndex;
  __declspec(property(get = __get_PurchasedChestsNumber)) int32_t   PurchasedChestsNumber;
  __declspec(property(get = __get__currentTabSection)) SectionID    _currentTabSection;
  __declspec(property(get = __get_TargetBundleStatus)) int32_t      TargetBundleStatus;

  Bundle* __get_CurTransactionBundle()
  {
    static auto field = get_class_helper().GetField("CurTransactionBundle");
    return *reinterpret_cast<Bundle**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  Bundle* __get_BundleToViewCache()
  {
    static auto field = get_class_helper().GetField("BundleToViewCache");
    return *reinterpret_cast<Bundle**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  bool __get__transactionInProgress()
  {
    static auto field = get_class_helper().GetField("_transactionInProgress");
    return *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  bool __get__fetchingBundlesInProgress()
  {
    static auto field = get_class_helper().GetField("_fetchingBundlesInProgress");
    return *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  CancelRefineSelectionReason __get__cancelRefineSelectionReason()
  {
    static auto field = get_class_helper().GetField("_cancelRefineSelectionReason");
    return *reinterpret_cast<CancelRefineSelectionReason*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  bool __get__isInRefineSelectionMode()
  {
    static auto field = get_class_helper().GetField("_isInRefineSelectionMode");
    return *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  bool __get__isInCancelSelectionMode()
  {
    static auto field = get_class_helper().GetField("_isInCancelSelectionMode");
    return *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  RefineSelectionContainer* __get__refineSelectionContainer()
  {
    static auto field = get_class_helper().GetField("_refineSelectionContainer");
    return *reinterpret_cast<RefineSelectionContainer**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  RefineSelectionAnalyticsObject* __get__refineSelectionAnalyticsObject()
  {
    static auto field = get_class_helper().GetField("_refineSelectionAnalyticsObject");
    return *reinterpret_cast<RefineSelectionAnalyticsObject**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  Bundle* __get_TargetSecondaryBundle()
  {
    static auto field = get_class_helper().GetField("<TargetSecondaryBundle>k__BackingField");
    return *reinterpret_cast<Bundle**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  Bundle* __get_TargetBundleOverride()
  {
    static auto field = get_class_helper().GetField("<TargetBundleOverride>k__BackingField");
    return *reinterpret_cast<Bundle**>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  int32_t __get_SelectedStoreGroupIndex()
  {
    static auto field = get_class_helper().GetField("SelectedStoreGroupIndex");
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  int32_t __get_PurchasedChestsNumber()
  {
    static auto field = get_class_helper().GetField("<PurchasedChestsNumber>k__BackingField");
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  SectionID __get__currentTabSection()
  {
    static auto field = get_class_helper().GetField("_currentTabSection");
    return *reinterpret_cast<SectionID*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

  int32_t __get_TargetBundleStatus()
  {
    static auto field = get_class_helper().GetField("<TargetBundleStatus>k__BackingField");
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(this) + field.offset());
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext");
    return class_helper;
  }
};