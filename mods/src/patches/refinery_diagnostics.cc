#include "patches/hook_registry.h"

#include "str_utils.h"

#include <prime/BundleQuantitySelectionWidget.h>
#include <prime/Hub.h>
#include <prime/RefineSelectionAnalyticsObject.h>
#include <prime/ShopCategory.h>
#include <prime/ShopSectionContext.h>
#include <prime/ShopShowcaseViewController.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

class RefineSelectionContainer;

namespace
{
constexpr HookDescriptor kSectionManagerTriggerSectionChangeHook{
    "SectionManager.TriggerSectionChange",
    "Log refinery section transitions before the game starts loading the target section.",
    {"Assembly-CSharp", "Digit.Client.Sections", "SectionManager", "TriggerSectionChange"},
    "Missing refinery section-change entries around a hang."};

constexpr HookDescriptor kShopSectionChangeRefineSelectionModeHook{
    "ShopSectionContext.ChangeRefineSelectionMode",
    "Log entry and exit from refinery selection mode.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext", "ChangeRefineSelectionMode"},
    "Missing refine-selection mode transitions."};

constexpr HookDescriptor kShopSectionSetBundleQuantityHook{
    "ShopSectionContext.SetBundleQuantityForRefineSelection",
    "Log refinery bundle quantity changes.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext", "SetBundleQuantityForRefineSelection"},
    "Missing selected refinery bundle quantities."};

constexpr HookDescriptor kShopSectionShowCancelMessageHook{
    "ShopSectionContext.ShowRefineSelectionCancelMessageBox",
    "Log cancel prompts shown while leaving refinery selection mode.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext", "ShowRefineSelectionCancelMessageBox"},
    "Missing refinery cancel prompt reason."};

constexpr HookDescriptor kShopSectionHandleSelectionLeaveHook{
    "ShopSectionContext.HandleRefineSelectionLeave",
    "Log the private leave-selection path that can precede section changes.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext", "HandleRefineSelectionLeave"},
    "Missing refine-selection leave entries."};

constexpr HookDescriptor kShopShowcaseAboutToShowHook{
    "ShopShowcaseViewController.AboutToShow",
    "Log refinery showcase lifecycle before and after the game binds the screen.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopShowcaseViewController", "AboutToShow"},
    "Missing showcase AboutToShow before/after breadcrumbs."};

constexpr HookDescriptor kShopShowcaseAboutToHideHook{
    "ShopShowcaseViewController.AboutToHide",
    "Log refinery showcase hide lifecycle before and after teardown.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopShowcaseViewController", "AboutToHide"},
    "Missing showcase AboutToHide breadcrumbs."};

constexpr HookDescriptor kShopShowcaseOnBuyButtonHook{
    "ShopShowcaseViewController.OnBuyButton",
    "Log refinery purchase button intent before purchase handling.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopShowcaseViewController", "OnBuyButton"},
    "Missing purchase button intent."};

constexpr HookDescriptor kShopShowcasePurchaseCompletedHook{
    "ShopShowcaseViewController.OnPurchaseCompletedEventHandler",
    "Log successful refinery purchase completion events.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopShowcaseViewController", "OnPurchaseCompletedEventHandler"},
    "Missing purchase completion result."};

constexpr HookDescriptor kShopShowcasePurchaseFailedHook{
    "ShopShowcaseViewController.OnPurchaseFailedEventHandler",
    "Log refinery purchase failure reason and server error details.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopShowcaseViewController", "OnPurchaseFailedEventHandler"},
    "Missing purchase failure result."};

constexpr HookDescriptor kQuantityWidgetIncreaseHook{"BundleQuantitySelectionWidget.OnIncreaseSelectionButtonClicked",
                                                     "Log manual refinery quantity increases.",
                                                     {"Assembly-CSharp", "Digit.Prime.Shop.RefineSelection",
                                                      "BundleQuantitySelectionWidget",
                                                      "OnIncreaseSelectionButtonClicked"},
                                                     "Missing quantity increase click."};

constexpr HookDescriptor kQuantityWidgetDecreaseHook{"BundleQuantitySelectionWidget.OnDecreaseSelectionButtonClicked",
                                                     "Log manual refinery quantity decreases.",
                                                     {"Assembly-CSharp", "Digit.Prime.Shop.RefineSelection",
                                                      "BundleQuantitySelectionWidget",
                                                      "OnDecreaseSelectionButtonClicked"},
                                                     "Missing quantity decrease click."};

constexpr HookDescriptor kQuantityWidgetDirtyFlagsHook{
    "BundleQuantitySelectionWidget.OnRefineryDirtyFlagsChanged",
    "Log refinery dirty-flag updates that refresh selection widgets.",
    {"Assembly-CSharp", "Digit.Prime.Shop.RefineSelection", "BundleQuantitySelectionWidget",
     "OnRefineryDirtyFlagsChanged"},
    "Missing dirty flag updates."};

constexpr HookDescriptor kAnalyticsSetupRefineHook{"RefineSelectionAnalyticsObject.SetupAndPostRefineEvent",
                                                   "Log refine analytics setup parameters before posting.",
                                                   {"Assembly-CSharp", "Digit.Prime.Shop.RefineSelection.Analytics",
                                                    "RefineSelectionAnalyticsObject", "SetupAndPostRefineEvent"},
                                                   "Missing refine analytics setup."};

constexpr HookDescriptor kAnalyticsSetupSaveHook{"RefineSelectionAnalyticsObject.SetupAndPostListSaveEvent",
                                                 "Log saved refinery selection analytics before posting.",
                                                 {"Assembly-CSharp", "Digit.Prime.Shop.RefineSelection.Analytics",
                                                  "RefineSelectionAnalyticsObject", "SetupAndPostListSaveEvent"},
                                                 "Missing selection-save analytics setup."};

constexpr HookDescriptor kAnalyticsPostEventHook{
    "RefineSelectionAnalyticsObject.PostEvent",
    "Log the final refinery analytics object immediately before send/reset.",
    {"Assembly-CSharp", "Digit.Prime.Shop.RefineSelection.Analytics", "RefineSelectionAnalyticsObject", "PostEvent"},
    "Missing final analytics payload breadcrumb."};

constexpr HookDescriptor kRefineSelectionContainerSetBundleHook{
    "RefineSelectionContainer.SetBundleToRefineSelection",
    "Log direct mutations to the pending refinery selection container.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "RefineSelectionContainer", "SetBundleToRefineSelection"},
    "Missing direct pending selection mutations."};

constexpr HookDescriptor kRefineSelectionContainerTryResolveHook{
    "RefineSelectionContainer.TryResolvePendingSelection",
    "Log pending refinery selection resolution against the shop service.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "RefineSelectionContainer", "TryResolvePendingSelection"},
    "Missing pending refinery selection resolution."};

constexpr HookDescriptor kRefineSelectionContainerApplyChangesHook{
    "RefineSelectionContainer.ApplyChanges",
    "Log when pending refinery selections are committed.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "RefineSelectionContainer", "ApplyChanges"},
    "Missing refinery selection apply boundary."};

constexpr HookDescriptor kRefineSelectionContainerClearChangesHook{
    "RefineSelectionContainer.ClearChanges",
    "Log when pending refinery selection changes are discarded.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "RefineSelectionContainer", "ClearChanges"},
    "Missing refinery selection clear boundary."};

constexpr HookDescriptor kRefineSelectionContainerResetSelectionHook{
    "RefineSelectionContainer.ResetSelection",
    "Log when refinery selections are reset to their saved state.",
    {"Assembly-CSharp", "Digit.Prime.Shop", "RefineSelectionContainer", "ResetSelection"},
    "Missing refinery selection reset boundary."};

constexpr auto kRecentRefineryFlowWindow = std::chrono::seconds(30);

std::atomic<uint64_t>  g_refinery_diag_sequence{0};
std::atomic<int64_t>   g_last_refinery_activity_ms{0};
std::atomic<uintptr_t> g_last_refinery_activity_subject{0};

enum class RefineryShowcaseMatch {
  None,
  ExactShowcase,
  RecentFlow,
};

uint64_t next_refinery_diag_sequence()
{ return g_refinery_diag_sequence.fetch_add(1, std::memory_order_relaxed) + 1; }

int64_t steady_now_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void remember_refinery_activity(const void* subject)
{
  g_last_refinery_activity_ms.store(steady_now_ms(), std::memory_order_relaxed);
  g_last_refinery_activity_subject.store(reinterpret_cast<uintptr_t>(subject), std::memory_order_relaxed);
}

int64_t recent_refinery_activity_age_ms()
{
  const auto last = g_last_refinery_activity_ms.load(std::memory_order_relaxed);
  if (last == 0) {
    return -1;
  }

  const auto age = steady_now_ms() - last;
  return age < 0 ? 0 : age;
}

bool has_recent_refinery_activity()
{
  const auto age = recent_refinery_activity_age_ms();
  return age >= 0 && age <= kRecentRefineryFlowWindow.count() * 1000;
}

uintptr_t recent_refinery_activity_subject()
{ return g_last_refinery_activity_subject.load(std::memory_order_relaxed); }

size_t current_thread_hash()
{ return std::hash<std::thread::id>{}(std::this_thread::get_id()); }

void flush_refinery_diag_log()
{
  if (auto logger = spdlog::default_logger()) {
    logger->flush();
  }
}

void log_refinery_edge(std::string_view phase, const void* subject)
{
  spdlog::info("[RefineryDiag] edge seq={} thread={} phase={} subject={}", next_refinery_diag_sequence(),
               current_thread_hash(), phase, subject);
  flush_refinery_diag_log();
}

void log_refinery_edge_debug(std::string_view phase, const void* subject)
{
  spdlog::debug("[RefineryDiag] edge seq={} thread={} phase={} subject={}", next_refinery_diag_sequence(),
                current_thread_hash(), phase, subject);
  flush_refinery_diag_log();
}

std::string il2cpp_string_or_label(Il2CppString* value)
{
  if (!value) {
    return "<null>";
  }

  try {
    return to_string(value);
  } catch (...) {
    return "<conversion-failed>";
  }
}

const char* section_label(SectionID section)
{
  switch (section) {
    case SectionID::Shop_Refining_List:
      return "Shop_Refining_List";
    case SectionID::Shop_Refining_Showcase:
      return "Shop_Refining_Showcase";
    case SectionID::Shop_Showcase:
      return "Shop_Showcase";
    case SectionID::Shop_List:
      return "Shop_List";
    case SectionID::Shop_Summary:
      return "Shop_Summary";
    case SectionID::Interstitial:
      return "Interstitial";
    default:
      return "other";
  }
}

bool is_refinery_related_section(SectionID section)
{
  switch (section) {
    case SectionID::Shop_Refining_List:
    case SectionID::Shop_Refining_Showcase:
      return true;
    default:
      return false;
  }
}

bool is_refinery_showcase(ShopShowcaseViewController* controller)
{ return controller != nullptr && controller->_isRefining; }

RefineryShowcaseMatch classify_showcase_refinery_match(ShopShowcaseViewController* controller)
{
  if (is_refinery_showcase(controller)) {
    return RefineryShowcaseMatch::ExactShowcase;
  }

  if (has_recent_refinery_activity()) {
    return RefineryShowcaseMatch::RecentFlow;
  }

  return RefineryShowcaseMatch::None;
}

const char* showcase_match_label(RefineryShowcaseMatch match)
{
  switch (match) {
    case RefineryShowcaseMatch::ExactShowcase:
      return "exact-refinery-showcase";
    case RefineryShowcaseMatch::RecentFlow:
      return "recent-refinery-flow";
    default:
      return "none";
  }
}

const char* edge_phase(const char* base, const char* suffix)
{
  static thread_local std::string phase;
  phase = base;
  phase += suffix;
  return phase.c_str();
}

bool is_refinery_section_context(ShopSectionContext* context)
{
  if (!context) {
    return false;
  }

  switch (context->_currentTabSection) {
    case SectionID::Shop_Refining_List:
    case SectionID::Shop_Refining_Showcase:
      return true;
    default:
      return false;
  }
}

const char* cancel_reason_label(CancelRefineSelectionReason reason)
{
  switch (reason) {
    case CancelRefineSelectionReason::Back:
      return "Back";
    case CancelRefineSelectionReason::Shop:
      return "Shop";
    case CancelRefineSelectionReason::Cancel:
      return "Cancel";
    case CancelRefineSelectionReason::Mantis:
      return "Mantis";
    case CancelRefineSelectionReason::MantisTutorial:
      return "MantisTutorial";
    case CancelRefineSelectionReason::Bundle:
      return "Bundle";
    default:
      return "Unknown";
  }
}

const char* selection_state_label(BundleQuantitySelectionInfoState state)
{
  switch (state) {
    case BundleQuantitySelectionInfoState::NotSelected:
      return "NotSelected";
    case BundleQuantitySelectionInfoState::NotAffordable:
      return "NotAffordable";
    case BundleQuantitySelectionInfoState::InCooldown:
      return "InCooldown";
    case BundleQuantitySelectionInfoState::AvailableToRefine:
      return "AvailableToRefine";
    default:
      return "Unknown";
  }
}

const char* action_type_label(RefineSelectionActionType action)
{
  switch (action) {
    case RefineSelectionActionType::None:
      return "None";
    case RefineSelectionActionType::Save:
      return "Save";
    case RefineSelectionActionType::Refine:
      return "Refine";
    default:
      return "Unknown";
  }
}

const char* purchase_failure_reason_label(int32_t reason)
{
  switch (reason) {
    case 0:
      return "UnknownError";
    case 1:
      return "TransactionErrorPaymentTaken";
    case 2:
      return "TransactionErrorPaymentNotTaken";
    case 3:
      return "AlreadyOwned";
    case 4:
      return "NoConnection";
    case 5:
      return "UnknownProduct";
    default:
      return "Unknown";
  }
}

const char* gs_error_type_label(int32_t type)
{
  switch (type) {
    case 1:
      return "NETWORK";
    case 2:
      return "PLATFORM";
    case 3:
      return "SERVER";
    case 4:
      return "CLIENT";
    case 5:
      return "UNKNOWN";
    case 7:
      return "S3";
    default:
      return "UNSET";
  }
}

template <typename T> T read_managed_field(void* object, ptrdiff_t offset, T default_value = {})
{
  if (!object) {
    return default_value;
  }

  return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(object) + offset);
}

void log_context_state(std::string_view phase, ShopSectionContext* context)
{
  if (!context) {
    spdlog::info("[RefineryDiag] {} context=<null>", phase);
    return;
  }

  const auto cancel_reason = context->_cancelRefineSelectionReason;
  const auto current_tab   = context->_currentTabSection;
  spdlog::info(
      "[RefineryDiag] {} context={} selectionMode={} cancelMode={} transaction={} fetching={} "
      "selectedStoreGroup={} purchasedChests={} currentTab={}({}) targetStatus={} cancelReason={}({}) "
      "refineContainer={} analytics={} bundleToView={} targetSecondary={} targetOverride={} txBundle={}",
      phase, static_cast<const void*>(context), context->_isInRefineSelectionMode, context->_isInCancelSelectionMode,
      context->_transactionInProgress, context->_fetchingBundlesInProgress, context->SelectedStoreGroupIndex,
      context->PurchasedChestsNumber, static_cast<int32_t>(current_tab), section_label(current_tab),
      context->TargetBundleStatus, static_cast<int32_t>(cancel_reason), cancel_reason_label(cancel_reason),
      static_cast<const void*>(context->_refineSelectionContainer),
      static_cast<const void*>(context->_refineSelectionAnalyticsObject),
      static_cast<const void*>(context->BundleToViewCache), static_cast<const void*>(context->TargetSecondaryBundle),
      static_cast<const void*>(context->TargetBundleOverride), static_cast<const void*>(context->CurTransactionBundle));
}

void log_showcase_state(std::string_view phase, ShopShowcaseViewController* controller)
{
  if (!controller) {
    spdlog::info("[RefineryDiag] {} showcase=<null>", phase);
    return;
  }

  spdlog::info("[RefineryDiag] {} showcase={} isRefining={} targetBundle={} targetSecondaryBundle={}", phase,
               static_cast<const void*>(controller), controller->_isRefining,
               static_cast<const void*>(controller->_targetBundle),
               static_cast<const void*>(controller->_targetSecondaryBundle));
}

void log_widget_state(std::string_view phase, BundleQuantitySelectionWidget* widget)
{
  if (!widget) {
    spdlog::info("[RefineryDiag] {} quantityWidget=<null>", phase);
    return;
  }

  const auto state = widget->_selectionInfoState;
  spdlog::info("[RefineryDiag] {} quantityWidget={} selectionInfoState={}({}) context={}", phase,
               static_cast<const void*>(widget), static_cast<int32_t>(state), selection_state_label(state),
               static_cast<const void*>(widget->_shopSectionContext));
}

void log_analytics_state(std::string_view phase, RefineSelectionAnalyticsObject* analytics)
{
  if (!analytics) {
    spdlog::info("[RefineryDiag] {} analytics=<null>", phase);
    return;
  }

  const auto action = analytics->ActionType;
  spdlog::info("[RefineryDiag] {} analytics={} action={}({}) category={} amount={} userId={}", phase,
               static_cast<const void*>(analytics), static_cast<int32_t>(action), action_type_label(action),
               il2cpp_string_or_label(analytics->RefineCategory), analytics->RefineAmount,
               il2cpp_string_or_label(analytics->UserId));
}

void log_gs_error(void* error)
{
  if (!error) {
    spdlog::info("[RefineryDiag] purchase failure gsError=<null>");
    return;
  }

  const auto type       = read_managed_field<int32_t>(error, 0x10);
  const auto code       = read_managed_field<int32_t>(error, 0x14);
  const auto category   = read_managed_field<Il2CppString*>(error, 0x18);
  const auto message    = read_managed_field<Il2CppString*>(error, 0x20);
  const auto requestUrl = read_managed_field<Il2CppString*>(error, 0x28);
  const auto httpCode   = read_managed_field<int32_t>(error, 0x30);
  const auto txId       = read_managed_field<Il2CppString*>(error, 0x38);

  spdlog::info("[RefineryDiag] purchase failure gsError={} type={}({}) code={} http={} category={} message={} "
               "transactionId={} requestUrl={}",
               static_cast<const void*>(error), type, gs_error_type_label(type), code, httpCode,
               il2cpp_string_or_label(category), il2cpp_string_or_label(message), il2cpp_string_or_label(txId),
               il2cpp_string_or_label(requestUrl));
}

void SectionManager_TriggerSectionChange_RefineryDiagnostics_Hook(auto original, SectionManager* self,
                                                                  SectionID nextSectionID, void* args,
                                                                  bool forcedSectionChange, bool isGoBackStep,
                                                                  bool allowSameSection)
{
  if (is_refinery_related_section(nextSectionID)) {
    remember_refinery_activity(self);
    log_refinery_edge("section-change enter", self);
    spdlog::info("[RefineryDiag] section-change begin manager={} next={}({}) args={} forced={} goBack={} allowSame={}",
                 static_cast<const void*>(self), static_cast<int32_t>(nextSectionID), section_label(nextSectionID),
                 args, forcedSectionChange, isGoBackStep, allowSameSection);
    original(self, nextSectionID, args, forcedSectionChange, isGoBackStep, allowSameSection);
    log_refinery_edge("section-change returned", self);
    return;
  }

  original(self, nextSectionID, args, forcedSectionChange, isGoBackStep, allowSameSection);
}

void ShopSectionContext_ChangeRefineSelectionMode_Hook(auto original, ShopSectionContext* context, bool enabled)
{
  if (!enabled && !is_refinery_section_context(context)) {
    original(context, enabled);
    return;
  }

  spdlog::info("[RefineryDiag] refine-selection-mode begin enabled={} context={}", enabled,
               static_cast<const void*>(context));
  remember_refinery_activity(context);
  log_context_state("refine-selection-mode before", context);
  log_refinery_edge("refine-selection-mode enter", context);
  original(context, enabled);
  log_refinery_edge("refine-selection-mode returned", context);
  log_context_state("refine-selection-mode after", context);
}

void ShopSectionContext_SetBundleQuantityForRefineSelection_Hook(auto original, ShopSectionContext* context,
                                                                 Bundle* bundle, uint32_t quantity)
{
  spdlog::info("[RefineryDiag] set-bundle-quantity begin context={} bundle={} quantity={}",
               static_cast<const void*>(context), static_cast<const void*>(bundle), quantity);
  remember_refinery_activity(context);
  log_context_state("set-bundle-quantity before", context);
  log_refinery_edge("set-bundle-quantity enter", context);
  original(context, bundle, quantity);
  log_refinery_edge("set-bundle-quantity returned", context);
  log_context_state("set-bundle-quantity after", context);
}

void ShopSectionContext_ShowRefineSelectionCancelMessageBox_Hook(auto original, ShopSectionContext* context,
                                                                 CancelRefineSelectionReason cancelReason)
{
  spdlog::info("[RefineryDiag] cancel-message begin context={} reason={}({})", static_cast<const void*>(context),
               static_cast<int32_t>(cancelReason), cancel_reason_label(cancelReason));
  remember_refinery_activity(context);
  log_context_state("cancel-message before", context);
  log_refinery_edge("cancel-message enter", context);
  original(context, cancelReason);
  log_refinery_edge("cancel-message returned", context);
  log_context_state("cancel-message after", context);
}

void ShopSectionContext_HandleRefineSelectionLeave_Hook(auto original, ShopSectionContext* context)
{
  remember_refinery_activity(context);
  log_context_state("selection-leave before", context);
  log_refinery_edge("selection-leave enter", context);
  original(context);
  log_refinery_edge("selection-leave returned", context);
  log_context_state("selection-leave after", context);
}

void ShopShowcaseViewController_AboutToShow_Hook(auto original, ShopShowcaseViewController* controller)
{
  const auto before_match = classify_showcase_refinery_match(controller);
  if (before_match == RefineryShowcaseMatch::None) {
    log_refinery_edge_debug("showcase-about-to-show enter", controller);
  } else {
    log_refinery_edge(edge_phase("showcase-about-to-show enter-", showcase_match_label(before_match)), controller);
  }

  original(controller);

  const auto after_match = classify_showcase_refinery_match(controller);
  if (after_match == RefineryShowcaseMatch::ExactShowcase) {
    remember_refinery_activity(controller);
    log_refinery_edge("showcase-about-to-show returned-refinery", controller);
    log_showcase_state("showcase-about-to-show after", controller);
  } else if (after_match == RefineryShowcaseMatch::RecentFlow) {
    remember_refinery_activity(controller);
    log_refinery_edge("showcase-about-to-show returned-recent-refinery-flow", controller);
    log_showcase_state("showcase-about-to-show after", controller);
  } else {
    log_refinery_edge_debug("showcase-about-to-show returned-non-refinery", controller);
  }
}

void ShopShowcaseViewController_AboutToHide_Hook(auto original, ShopShowcaseViewController* controller)
{
  const auto before_match = classify_showcase_refinery_match(controller);
  if (before_match == RefineryShowcaseMatch::ExactShowcase) {
    remember_refinery_activity(controller);
    log_refinery_edge("showcase-about-to-hide enter-refinery", controller);
    log_showcase_state("showcase-about-to-hide before", controller);
  } else if (before_match == RefineryShowcaseMatch::RecentFlow) {
    remember_refinery_activity(controller);
    log_refinery_edge("showcase-about-to-hide enter-recent-refinery-flow", controller);
    log_showcase_state("showcase-about-to-hide before", controller);
  } else {
    log_refinery_edge_debug("showcase-about-to-hide enter-non-refinery", controller);
  }

  original(controller);

  const auto after_match = classify_showcase_refinery_match(controller);
  if (after_match == RefineryShowcaseMatch::ExactShowcase) {
    remember_refinery_activity(controller);
    log_refinery_edge("showcase-about-to-hide returned-refinery", controller);
  } else if (after_match == RefineryShowcaseMatch::RecentFlow) {
    remember_refinery_activity(controller);
    log_refinery_edge("showcase-about-to-hide returned-recent-refinery-flow", controller);
  } else {
    log_refinery_edge_debug("showcase-about-to-hide returned-non-refinery", controller);
  }
}

void ShopShowcaseViewController_OnBuyButton_Hook(auto original, ShopShowcaseViewController* controller, int32_t index,
                                                 Il2CppString* buttonID)
{
  const auto match = classify_showcase_refinery_match(controller);
  if (match == RefineryShowcaseMatch::None) {
    log_refinery_edge_debug("buy-button passthrough-non-refinery enter", controller);
    original(controller, index, buttonID);
    log_refinery_edge_debug("buy-button passthrough-non-refinery returned", controller);
    return;
  }

  const auto recent_age_ms  = recent_refinery_activity_age_ms();
  const auto recent_subject = recent_refinery_activity_subject();
  remember_refinery_activity(controller);
  spdlog::info("[RefineryDiag] buy-button begin showcase={} match={} recentAgeMs={} recentSubject={} index={} "
               "buttonId={}",
               static_cast<const void*>(controller), showcase_match_label(match), recent_age_ms,
               reinterpret_cast<const void*>(recent_subject), index, il2cpp_string_or_label(buttonID));
  log_showcase_state("buy-button before", controller);
  log_refinery_edge(edge_phase("buy-button enter-", showcase_match_label(match)), controller);
  original(controller, index, buttonID);
  log_refinery_edge(edge_phase("buy-button returned-", showcase_match_label(match)), controller);
  remember_refinery_activity(controller);
  log_showcase_state("buy-button after", controller);
}

void ShopShowcaseViewController_OnPurchaseCompletedEventHandler_Hook(auto                        original,
                                                                     ShopShowcaseViewController* controller,
                                                                     Bundle*                     item)
{
  const auto match = classify_showcase_refinery_match(controller);
  if (match == RefineryShowcaseMatch::None) {
    log_refinery_edge_debug("purchase-completed passthrough-non-refinery enter", controller);
    original(controller, item);
    log_refinery_edge_debug("purchase-completed passthrough-non-refinery returned", controller);
    return;
  }

  const auto recent_age_ms  = recent_refinery_activity_age_ms();
  const auto recent_subject = recent_refinery_activity_subject();
  remember_refinery_activity(controller);
  spdlog::info("[RefineryDiag] purchase-completed begin showcase={} match={} recentAgeMs={} recentSubject={} bundle={}",
               static_cast<const void*>(controller), showcase_match_label(match), recent_age_ms,
               reinterpret_cast<const void*>(recent_subject), static_cast<const void*>(item));
  log_showcase_state("purchase-completed before", controller);
  log_refinery_edge(edge_phase("purchase-completed enter-", showcase_match_label(match)), controller);
  original(controller, item);
  log_refinery_edge(edge_phase("purchase-completed returned-", showcase_match_label(match)), controller);
  remember_refinery_activity(controller);
  log_showcase_state("purchase-completed after", controller);
}

void ShopShowcaseViewController_OnPurchaseFailedEventHandler_Hook(auto original, ShopShowcaseViewController* controller,
                                                                  Bundle* bundle, int32_t failureReason, void* error)
{
  const auto match = classify_showcase_refinery_match(controller);
  if (match == RefineryShowcaseMatch::None) {
    log_refinery_edge_debug("purchase-failed passthrough-non-refinery enter", controller);
    original(controller, bundle, failureReason, error);
    log_refinery_edge_debug("purchase-failed passthrough-non-refinery returned", controller);
    return;
  }

  const auto recent_age_ms  = recent_refinery_activity_age_ms();
  const auto recent_subject = recent_refinery_activity_subject();
  remember_refinery_activity(controller);
  spdlog::info("[RefineryDiag] purchase-failed begin showcase={} match={} recentAgeMs={} recentSubject={} bundle={} "
               "reason={}({})",
               static_cast<const void*>(controller), showcase_match_label(match), recent_age_ms,
               reinterpret_cast<const void*>(recent_subject), static_cast<const void*>(bundle), failureReason,
               purchase_failure_reason_label(failureReason));
  log_showcase_state("purchase-failed before", controller);
  log_gs_error(error);
  log_refinery_edge(edge_phase("purchase-failed enter-", showcase_match_label(match)), controller);
  original(controller, bundle, failureReason, error);
  log_refinery_edge(edge_phase("purchase-failed returned-", showcase_match_label(match)), controller);
  remember_refinery_activity(controller);
  log_showcase_state("purchase-failed after", controller);
}

void BundleQuantitySelectionWidget_OnIncreaseSelectionButtonClicked_Hook(auto                           original,
                                                                         BundleQuantitySelectionWidget* widget)
{
  remember_refinery_activity(widget);
  log_widget_state("quantity-increase before", widget);
  log_refinery_edge("quantity-increase enter", widget);
  original(widget);
  log_refinery_edge("quantity-increase returned", widget);
  log_widget_state("quantity-increase after", widget);
}

void BundleQuantitySelectionWidget_OnDecreaseSelectionButtonClicked_Hook(auto                           original,
                                                                         BundleQuantitySelectionWidget* widget)
{
  remember_refinery_activity(widget);
  log_widget_state("quantity-decrease before", widget);
  log_refinery_edge("quantity-decrease enter", widget);
  original(widget);
  log_refinery_edge("quantity-decrease returned", widget);
  log_widget_state("quantity-decrease after", widget);
}

void BundleQuantitySelectionWidget_OnRefineryDirtyFlagsChanged_Hook(auto                           original,
                                                                    BundleQuantitySelectionWidget* widget,
                                                                    int32_t                        flags)
{
  spdlog::debug("[RefineryDiag] refinery-dirty-flags begin quantityWidget={} flags={}",
                static_cast<const void*>(widget), flags);
  original(widget, flags);
  if (widget) {
    remember_refinery_activity(widget->_shopSectionContext);
    const auto state = widget->_selectionInfoState;
    spdlog::debug(
        "[RefineryDiag] refinery-dirty-flags after quantityWidget={} flags={} selectionInfoState={}({}) context={}",
        static_cast<const void*>(widget), flags, static_cast<int32_t>(state), selection_state_label(state),
        static_cast<const void*>(widget->_shopSectionContext));
  }
}

void RefineSelectionAnalyticsObject_SetupAndPostRefineEvent_Hook(auto                            original,
                                                                 RefineSelectionAnalyticsObject* analytics,
                                                                 ShopCategory category, int32_t bundlesCount)
{
  spdlog::info("[RefineryDiag] analytics-setup-refine begin analytics={} category={} bundlesCount={}",
               static_cast<const void*>(analytics), category.Value, bundlesCount);
  remember_refinery_activity(analytics);
  log_analytics_state("analytics-setup-refine before", analytics);
  log_refinery_edge("analytics-setup-refine enter", analytics);
  original(analytics, category, bundlesCount);
  log_refinery_edge("analytics-setup-refine returned", analytics);
  log_analytics_state("analytics-setup-refine after", analytics);
}

void RefineSelectionAnalyticsObject_SetupAndPostListSaveEvent_Hook(auto                            original,
                                                                   RefineSelectionAnalyticsObject* analytics,
                                                                   int32_t                         bundlesCount)
{
  spdlog::info("[RefineryDiag] analytics-setup-save begin analytics={} bundlesCount={}",
               static_cast<const void*>(analytics), bundlesCount);
  remember_refinery_activity(analytics);
  log_analytics_state("analytics-setup-save before", analytics);
  log_refinery_edge("analytics-setup-save enter", analytics);
  original(analytics, bundlesCount);
  log_refinery_edge("analytics-setup-save returned", analytics);
  log_analytics_state("analytics-setup-save after", analytics);
}

void RefineSelectionAnalyticsObject_PostEvent_Hook(auto original, RefineSelectionAnalyticsObject* analytics)
{
  remember_refinery_activity(analytics);
  log_analytics_state("analytics-post-event before", analytics);
  log_refinery_edge("analytics-post-event enter", analytics);
  original(analytics);
  log_refinery_edge("analytics-post-event returned", analytics);
  log_analytics_state("analytics-post-event after", analytics);
}

void RefineSelectionContainer_SetBundleToRefineSelection_Hook(auto original, RefineSelectionContainer* container,
                                                              Bundle* bundle, uint32_t quantity)
{
  spdlog::info("[RefineryDiag] container-set-bundle begin container={} bundle={} quantity={}",
               static_cast<const void*>(container), static_cast<const void*>(bundle), quantity);
  remember_refinery_activity(container);
  log_refinery_edge("container-set-bundle enter", container);
  original(container, bundle, quantity);
  log_refinery_edge("container-set-bundle returned", container);
}

bool RefineSelectionContainer_TryResolvePendingSelection_Hook(auto original, RefineSelectionContainer* container,
                                                              void* shopService)
{
  spdlog::info("[RefineryDiag] container-try-resolve-pending begin container={} shopService={}",
               static_cast<const void*>(container), shopService);
  remember_refinery_activity(container);
  log_refinery_edge("container-try-resolve-pending enter", container);
  const auto resolved = original(container, shopService);
  log_refinery_edge("container-try-resolve-pending returned", container);
  spdlog::info("[RefineryDiag] container-try-resolve-pending result container={} resolved={}",
               static_cast<const void*>(container), resolved);
  return resolved;
}

void RefineSelectionContainer_ApplyChanges_Hook(auto original, RefineSelectionContainer* container)
{
  remember_refinery_activity(container);
  log_refinery_edge("container-apply-changes enter", container);
  original(container);
  log_refinery_edge("container-apply-changes returned", container);
}

void RefineSelectionContainer_ClearChanges_Hook(auto original, RefineSelectionContainer* container)
{
  remember_refinery_activity(container);
  log_refinery_edge("container-clear-changes enter", container);
  original(container);
  log_refinery_edge("container-clear-changes returned", container);
}

void RefineSelectionContainer_ResetSelection_Hook(auto original, RefineSelectionContainer* container)
{
  remember_refinery_activity(container);
  log_refinery_edge("container-reset-selection enter", container);
  original(container);
  log_refinery_edge("container-reset-selection returned", container);
}

#define INSTALL_REFINERY_DIAG_HOOK(registry, descriptor, hook_fn)                                                      \
  do {                                                                                                                 \
    auto class_helper =                                                                                                \
        il2cpp_get_class_helper((descriptor).target.assembly.data(), (descriptor).target.namespc.data(),               \
                                (descriptor).target.class_name.data());                                                \
    if (!class_helper.isValidHelper()) {                                                                               \
      (registry).record_missing_helper((descriptor));                                                                  \
      break;                                                                                                           \
    }                                                                                                                  \
    auto method = class_helper.GetMethod((descriptor).target.method_name.data());                                      \
    if (method == nullptr) {                                                                                           \
      (registry).record_missing_method((descriptor));                                                                  \
      break;                                                                                                           \
    }                                                                                                                  \
    HOOK_REGISTRY_SPUD_STATIC_DETOUR((registry), (descriptor), method, hook_fn);                                       \
  } while (false)
} // namespace

void InstallRefineryDiagnosticsHooks()
{
  HookModuleHealth hooks("RefineryDiagnosticsHooks");

  INSTALL_REFINERY_DIAG_HOOK(hooks, kSectionManagerTriggerSectionChangeHook,
                             SectionManager_TriggerSectionChange_RefineryDiagnostics_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopSectionChangeRefineSelectionModeHook,
                             ShopSectionContext_ChangeRefineSelectionMode_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopSectionSetBundleQuantityHook,
                             ShopSectionContext_SetBundleQuantityForRefineSelection_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopSectionShowCancelMessageHook,
                             ShopSectionContext_ShowRefineSelectionCancelMessageBox_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopSectionHandleSelectionLeaveHook,
                             ShopSectionContext_HandleRefineSelectionLeave_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopShowcaseAboutToShowHook, ShopShowcaseViewController_AboutToShow_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopShowcaseAboutToHideHook, ShopShowcaseViewController_AboutToHide_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopShowcaseOnBuyButtonHook, ShopShowcaseViewController_OnBuyButton_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopShowcasePurchaseCompletedHook,
                             ShopShowcaseViewController_OnPurchaseCompletedEventHandler_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kShopShowcasePurchaseFailedHook,
                             ShopShowcaseViewController_OnPurchaseFailedEventHandler_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kQuantityWidgetIncreaseHook,
                             BundleQuantitySelectionWidget_OnIncreaseSelectionButtonClicked_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kQuantityWidgetDecreaseHook,
                             BundleQuantitySelectionWidget_OnDecreaseSelectionButtonClicked_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kQuantityWidgetDirtyFlagsHook,
                             BundleQuantitySelectionWidget_OnRefineryDirtyFlagsChanged_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kAnalyticsSetupRefineHook,
                             RefineSelectionAnalyticsObject_SetupAndPostRefineEvent_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kAnalyticsSetupSaveHook,
                             RefineSelectionAnalyticsObject_SetupAndPostListSaveEvent_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kAnalyticsPostEventHook, RefineSelectionAnalyticsObject_PostEvent_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kRefineSelectionContainerSetBundleHook,
                             RefineSelectionContainer_SetBundleToRefineSelection_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kRefineSelectionContainerTryResolveHook,
                             RefineSelectionContainer_TryResolvePendingSelection_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kRefineSelectionContainerApplyChangesHook,
                             RefineSelectionContainer_ApplyChanges_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kRefineSelectionContainerClearChangesHook,
                             RefineSelectionContainer_ClearChanges_Hook);
  INSTALL_REFINERY_DIAG_HOOK(hooks, kRefineSelectionContainerResetSelectionHook,
                             RefineSelectionContainer_ResetSelection_Hook);

  hooks.log_summary();
}
