#include "config.h"
#include "errormsg.h"
#include "patches/hook_registry.h"
#include "patches/section_change_router.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>

#include <cstddef>
#include <cstring>
#include <string>

namespace
{
constexpr auto kGiftsCategoryKey    = "chests";
constexpr int  kAutoOpenRetryFrames = 300;

constexpr HookDescriptor kShopListViewControllerAboutToHideHook{
    "ShopListViewController.AboutToHide",
    "Clear pending Gifts bulk-claim flyout work when leaving the shop list.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopListViewController", "AboutToHide"},
    "Pending Gifts flyout open state may survive beyond the Gifts view."};

constexpr HookDescriptor kShopSectionContextInjectTabDataHook{
    "ShopSectionContext.InjectTabData",
    "Arm the Gifts bulk-claim flyout when shop tab data selects a bulk-claim-capable section.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext", "InjectTabData"},
    "Gifts bulk-claim flyout may not auto-open after moving between shop tabs."};

constexpr HookDescriptor kShopListScrollerViewControllerUpdateHook{
    "ShopListScrollerViewController.Update",
    "Open the existing Gifts bulk-claim drawer once its context is available.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopListScrollerViewController", "Update"},
    "Gifts bulk-claim flyout will not auto-open after entering Gifts."};

bool g_pending_auto_open   = false;
bool g_auto_open_attempted = false;
int  g_retry_frames_left   = 0;

class DrawerContext
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerContext");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  { return get_class_helper(); }

  bool Enabled()
  {
    static auto field = get_class_helper().GetField("Enabled");
    return field.isValidHelper() && *(bool*)((ptrdiff_t)this + field.offset());
  }
};

class DrawerWidget
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerWidget");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  { return get_class_helper(); }

  DrawerContext* Context()
  {
    static auto widget_base = get_class_helper().GetParent("Widget`1");
    static auto field       = widget_base.GetProperty("Context");
    return field.GetRaw<DrawerContext>(this);
  }

  bool ContextEnabled()
  {
    auto context = Context();
    return context != nullptr && context->Enabled();
  }

  void OpenViaButtonCallback()
  {
    static auto method = get_class_helper().GetMethod<void(DrawerWidget*)>("OnOpenButtonClicked");
    if (method != nullptr) {
      method(this);
    }
  }
};

class ShopCategory
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Prime.Shop", "ShopCategory");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  { return get_class_helper(); }

  static std::string KeyForValue(int value)
  {
    static auto method = get_class_helper().GetMethod<Il2CppString*(int)>("EnumToKey");
    auto        key    = method != nullptr ? method(value) : nullptr;
    return key != nullptr ? to_string(key) : std::string{};
  }
};

class BundleGroupConfig
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "BundleGroupConfig");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  { return get_class_helper(); }

  int Category()
  {
    static auto field = get_class_helper().GetField("_category");
    return field.isValidHelper() ? *(int*)((ptrdiff_t)this + field.offset()) : -1;
  }
};

class ShopSectionContext
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  { return get_class_helper(); }
};

class ShopListScrollerViewController
{
private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopListScrollerViewController");
    return class_helper;
  }

public:
  static IL2CppClassHelper& ClassHelper()
  { return get_class_helper(); }

  DrawerWidget* SelectionDrawer()
  {
    // Lower Gifts << drawer; scroller category can describe upper shop chrome such as latinum.
    static auto field = get_class_helper().GetField("_selectionDrawerWidget");
    return field.isValidHelper() ? *(DrawerWidget**)((ptrdiff_t)this + field.offset()) : nullptr;
  }
};

bool IsClassNamed(void* object, const char* namespaze, const char* name)
{
  if (object == nullptr) {
    return false;
  }

  const auto klass = ((Il2CppObject*)object)->klass;
  if (klass == nullptr) {
    return false;
  }

  const auto object_namespace = klass->namespaze != nullptr ? klass->namespaze : "";
  return strcmp(object_namespace, namespaze) == 0 && strcmp(klass->name, name) == 0;
}

void ClearAutoOpen()
{
  g_pending_auto_open = false;
  g_retry_frames_left = 0;
}

void ArmAutoOpen()
{
  g_pending_auto_open   = true;
  g_auto_open_attempted = false;
  g_retry_frames_left   = kAutoOpenRetryFrames;
}

bool IsGiftsShopPayload(SectionID section, void* args)
{
  if (section != SectionID::Shop_List || !IsClassNamed(args, "Digit.Prime.Shop", "BundleGroupConfig")) {
    return false;
  }

  const auto category = ((BundleGroupConfig*)args)->Category();
  return ShopCategory::KeyForValue(category) == kGiftsCategoryKey;
}

bool IsBulkClaimTabSection(SectionID section)
{
  return section == SectionID::Shop_List || section == SectionID::Shop_AllianceChests
         || section == SectionID::Consumables || section == SectionID::Missions_AwayTeamsList;
}

void TryAutoOpenDrawer(ShopListScrollerViewController* controller)
{
  if (!AutoOpenBulkClaimGiftsEnabled() || !g_pending_auto_open || g_auto_open_attempted || controller == nullptr) {
    return;
  }

  if (g_retry_frames_left-- <= 0) {
    ClearAutoOpen();
    return;
  }

  auto drawer = controller->SelectionDrawer();
  if (drawer == nullptr || !drawer->ContextEnabled()) {
    return;
  }

  g_auto_open_attempted = true;
  ClearAutoOpen();
  drawer->OpenViaButtonCallback();
}

void ShopListViewController_AboutToHide(auto original, void* self)
{
  ClearAutoOpen();
  original(self);
}

void ShopListScrollerViewController_Update(auto original, ShopListScrollerViewController* self)
{
  original(self);
  TryAutoOpenDrawer(self);
}

void ShopSectionContext_InjectTabData(auto original, ShopSectionContext* self, int section,
                                      Il2CppArraySize* tab_locale_contexts,
                                      Il2CppArraySize* additional_tab_locale_contexts,
                                      Il2CppArraySize* tab_icon_identifiers, Il2CppArraySize* pip_types,
                                      Il2CppArraySize* hide_if_no_content, bool set_current_section,
                                      bool override_existing_tabs)
{
  original(self, section, tab_locale_contexts, additional_tab_locale_contexts, tab_icon_identifiers, pip_types,
           hide_if_no_content, set_current_section, override_existing_tabs);

  if (!AutoOpenBulkClaimGiftsEnabled() || !set_current_section) {
    return;
  }

  if (IsBulkClaimTabSection(static_cast<SectionID>(section))) {
    ArmAutoOpen();
  } else {
    ClearAutoOpen();
  }
}

void GiftsBulkClaimSectionChangeAfterOriginal(const SectionChangeContext& context)
{
  if (AutoOpenBulkClaimGiftsEnabled() && IsGiftsShopPayload(context.next_section, context.args)) {
    ArmAutoOpen();
  }
}

void ValidateDrawerTypes()
{
  auto drawer_context = DrawerContext::ClassHelper();
  if (!drawer_context.isValidHelper()) {
    ErrorMsg::MissingHelper("Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerContext");
  } else if (!drawer_context.GetField("Enabled").isValidHelper()) {
    ErrorMsg::MissingMethod("DrawerContext", "Enabled");
  }

  auto drawer_widget = DrawerWidget::ClassHelper();
  if (!drawer_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Prime.SharedFeatures.Scripts.UI.Widgets", "DrawerWidget");
  } else {
    auto widget_base = drawer_widget.GetParent("Widget`1");
    if (!widget_base.isValidHelper() || !widget_base.GetProperty("Context").isValidHelper()) {
      ErrorMsg::MissingMethod("DrawerWidget", "Widget<DrawerContext>.Context");
    }
    if (drawer_widget.GetMethod("OnOpenButtonClicked") == nullptr) {
      ErrorMsg::MissingMethod("DrawerWidget", "OnOpenButtonClicked");
    }
  }

  auto shop_category = ShopCategory::ClassHelper();
  if (!shop_category.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "ShopCategory");
  } else if (shop_category.GetMethod("EnumToKey") == nullptr) {
    ErrorMsg::MissingMethod("ShopCategory", "EnumToKey");
  }

  auto bundle_group_config = BundleGroupConfig::ClassHelper();
  if (!bundle_group_config.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Shop", "BundleGroupConfig");
  } else if (!bundle_group_config.GetField("_category").isValidHelper()) {
    ErrorMsg::MissingMethod("BundleGroupConfig", "_category");
  }
}
} // namespace

void InstallOpenBulkClaimGiftsHooks()
{
  HookModuleHealth hooks("OpenBulkClaimGiftsHooks");
  ValidateDrawerTypes();

  RegisterSectionChangeObserver({"open_bulk_claim_gifts", nullptr, GiftsBulkClaimSectionChangeAfterOriginal});

  auto shop_section_context = ShopSectionContext::ClassHelper();
  if (!shop_section_context.isValidHelper()) {
    hooks.record_missing_helper(kShopSectionContextInjectTabDataHook);
  } else if (auto ptr = shop_section_context.GetMethod("InjectTabData"); ptr == nullptr) {
    hooks.record_missing_method(kShopSectionContextInjectTabDataHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kShopSectionContextInjectTabDataHook, ptr,
                                     ShopSectionContext_InjectTabData);
  }

  auto shop_list_controller = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Shop", "ShopListViewController");
  if (!shop_list_controller.isValidHelper()) {
    hooks.record_missing_helper(kShopListViewControllerAboutToHideHook);
  } else if (auto ptr = shop_list_controller.GetMethod("AboutToHide"); ptr == nullptr) {
    hooks.record_missing_method(kShopListViewControllerAboutToHideHook);
  } else {
    HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kShopListViewControllerAboutToHideHook, ptr,
                                     ShopListViewController_AboutToHide);
  }

  auto shop_list_scroller = ShopListScrollerViewController::ClassHelper();
  if (!shop_list_scroller.isValidHelper()) {
    hooks.record_missing_helper(kShopListScrollerViewControllerUpdateHook);
  } else {
    if (!shop_list_scroller.GetField("_selectionDrawerWidget").isValidHelper()) {
      ErrorMsg::MissingMethod("ShopListScrollerViewController", "_selectionDrawerWidget");
    }
    if (auto ptr = shop_list_scroller.GetMethod("Update"); ptr == nullptr) {
      hooks.record_missing_method(kShopListScrollerViewControllerUpdateHook);
    } else {
      HOOK_REGISTRY_SPUD_STATIC_DETOUR(hooks, kShopListScrollerViewControllerUpdateHook, ptr,
                                       ShopListScrollerViewController_Update);
    }
  }

  hooks.log_summary();
}
