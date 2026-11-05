#pragma once

// The Windows setter from shop.cc is already present through the shop fixtures. Supply real typed metadata and
// local getter bodies, not a replacement donation implementation. No native dialog or resource operation runs.
#if _WIN32
#include "shop_limits_cases.h"

namespace donation_policy_fixture
{
std::function<void()> on_tag;
int                   tag_reads  = 0;
bool                  tag_throws = false;

bool Tag(LimitsContext* context, const MethodInfo*)
{
  ++tag_reads;
  const bool value = context->donation;
  if (on_tag)
    std::exchange(on_tag, {})();
  if (tag_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return value;
}

void Run(std::string_view mode)
{
  auto& context_class = Define("Digit.Prime.Inventories", "InventoryForPopup", IL2CPP_TYPE_CLASS, &object_class);
  if (mode != "donation-policy-missing-tag")
    AddMethod(context_class, "get_IsDonationUse", reinterpret_cast<void*>(&Tag), T(bool_class), {});
  LimitsContext context{{&context_class.klass}};
  auto*         popup                      = reinterpret_cast<InventoryForPopup*>(&context);
  auto&         initial_config             = Config::Get();
  initial_config.ui.extend_donation_slider = true;
  initial_config.ui.extend_donation_max    = 80;
  context.donation                         = true;
  context.maximum                          = 9007199254740993LL;
  const auto roots                         = warp_roots.size();
  int        calls                         = 0;
  int64_t    supplied                      = 50;
  int64_t    expected                      = 80;
  int        expected_reads                = 1;
  int        expected_calls                = 1;
  bool       replaced                      = false;

  if (mode == "donation-policy-off") {
    on_tag   = [] { Config::Get().ui.extend_donation_slider = false; };
    expected = 50;
  } else if (mode == "donation-policy-replace-off") {
    on_tag = [&] {
      bindings_replace_config               = true;
      auto& replacement                     = Config::Get();
      replaced                              = &replacement != &initial_config;
      replacement.ui.extend_donation_slider = false;
      replacement.ui.extend_donation_max    = 170;
    };
    expected = 50;
  } else if (mode == "donation-policy-cap-change") {
    on_tag   = [] { Config::Get().ui.extend_donation_max = 95; };
    expected = 95;
  } else if (mode == "donation-policy-cap-zero" || mode == "donation-policy-cap-negative") {
    on_tag         = [mode] { Config::Get().ui.extend_donation_max = mode == "donation-policy-cap-zero" ? 0 : -1; };
    expected_calls = 0;
  } else if (mode == "donation-policy-disabled") {
    initial_config.ui.extend_donation_slider = false;
    expected                                 = 50;
    expected_reads                           = 0;
  } else if (mode == "donation-policy-native-value") {
    supplied       = 100;
    expected       = 100;
    expected_reads = 0;
  } else if (mode == "donation-policy-false-tag") {
    context.donation = false;
    expected         = 50;
  } else if (mode == "donation-policy-failed-tag") {
    tag_throws = true;
    expected   = 50;
  } else if (mode == "donation-policy-missing-tag") {
    expected       = 50;
    expected_reads = 0;
  } else {
    Require(mode == "donation-policy-native", "known donation policy fixture mode");
  }

  const auto original = [&](InventoryForPopup* received, int64_t value) {
    Require(received == popup, "donation setter preserves its original receiver");
    ++calls;
    context.maximum = value;
  };
  InventoryForPopup_set_MaxItemsToUse_Hook(original, popup, supplied);
  Require(tag_reads == expected_reads, "donation eligibility reads follow the selected native path");
  Require(calls == expected_calls,
          "donation setter preserves exactly one original call or the explicit native-cap hold");
  Require(context.maximum == (expected_calls ? expected : 9007199254740993LL),
          "donation override follows current policy after the managed getter without fabricating a cap");
  if (mode == "donation-policy-replace-off")
    Require(replaced && initial_config.ui.extend_donation_slider,
            "Config replacement is distinct and the old enabled policy remains unchanged");
  Require(warp_roots.size() == roots, "donation policy operation releases temporary managed custody");
  bindings_config_override = nullptr;
  bindings_replacement_config.reset();
  std::cout << "Donation policy callback passed: " << mode << '\n';
}
} // namespace donation_policy_fixture

namespace
{
void DonationPolicyCases(std::string_view mode)
{ donation_policy_fixture::Run(mode); }
} // namespace
#endif

#if !_WIN32
#include "shop_limits_cases.h"

namespace donation_row_policy_fixture
{
std::function<void()> on_current;
int                   context_reads = 0, writes = 0;
bool                  context_throws = false, owned_throws = false;

Il2CppObject* Context(LimitsRow* row, const MethodInfo*)
{
  ++context_reads;
  // The first read acquires the popup. Policy callbacks happen only while the later current-context guard runs.
  // That guard still returns the same context in policy cases; the replacement control reports its new identity.
  if (context_reads == 2) {
    if (on_current)
      std::exchange(on_current, {})();
    if (context_throws)
      throw Il2CppExceptionWrapper(&thrown);
  }
  return row->context;
}
int64_t Owned(LimitsContext* context, const MethodInfo*)
{
  if (owned_throws)
    throw Il2CppExceptionWrapper(&thrown);
  return context->owned;
}
int64_t Maximum(LimitsContext* context, const MethodInfo*)
{ return context->maximum; }
void SetMaximum(LimitsContext* context, int64_t value, const MethodInfo*)
{
  ++writes;
  context->maximum = value;
}
bool Donation(LimitsContext* context, const MethodInfo*)
{ return context->donation; }
bool Chest(LimitsContext* context, const MethodInfo*)
{ return context->chest; }

void Run(std::string_view mode)
{
  auto& context_class = Define("Digit.Prime.Inventories", "InventoryForPopup", IL2CPP_TYPE_CLASS, &object_class);
  AddMethod(context_class, "get_IsChestPurchase", reinterpret_cast<void*>(&Chest), T(bool_class), {});
  AddMethod(context_class, "get_IsDonationUse", reinterpret_cast<void*>(&Donation), T(bool_class), {});
  AddMethod(context_class, "get_AmountOwned", reinterpret_cast<void*>(&Owned), T(long_class), {});
  AddMethod(context_class, "get_MaxItemsToUse", reinterpret_cast<void*>(&Maximum), T(long_class), {});
  auto& setter = AddMethod(context_class, "set_MaxItemsToUse", reinterpret_cast<void*>(&SetMaximum), T(void_class),
                           {T(long_class)});
  AddProperty(context_class, "MaxItemsToUse", context_class.methods[3], &setter);
  auto& row_class = Define("Digit.Prime.Inventories", "InventoryUseRowWidget", IL2CPP_TYPE_CLASS, &object_class);
  auto& getter    = AddMethod(row_class, "get_Context", reinterpret_cast<void*>(&Context), T(context_class), {});
  AddProperty(row_class, "Context", &getter);

  LimitsContext context{{&context_class.klass}}, replacement = context;
  context.donation = replacement.donation = true;
  LimitsRow row{{&row_class.klass}, &context.header};
  auto*     owner                          = reinterpret_cast<InventoryUseRowWidget*>(&row);
  auto&     initial_config                 = Config::Get();
  initial_config.ui.extend_donation_slider = true;
  initial_config.ui.extend_donation_max    = 80;
  int64_t expected                         = 80;
  int     expected_writes                  = 1;
  bool    replaced                         = false;

  if (mode == "donation-row-off") {
    on_current      = [] { Config::Get().ui.extend_donation_slider = false; };
    expected        = 50;
    expected_writes = 0;
  } else if (mode == "donation-row-replace-off") {
    on_current = [&] {
      bindings_replace_config        = true;
      auto& next                     = Config::Get();
      replaced                       = &next != &initial_config;
      next.ui.extend_donation_slider = false;
      next.ui.extend_donation_max    = 170;
    };
    expected        = 50;
    expected_writes = 0;
  } else if (mode == "donation-row-cap-change") {
    on_current = [] { Config::Get().ui.extend_donation_max = 95; };
    expected   = 95;
  } else if (mode == "donation-row-cap-zero") {
    on_current = [] { Config::Get().ui.extend_donation_max = 0; };
    expected   = context.owned;
  } else if (mode == "donation-row-context-replaced") {
    on_current      = [&] { row.context = &replacement.header; };
    expected        = 50;
    expected_writes = 0;
  } else if (mode == "donation-row-failed-context") {
    context_throws  = true;
    expected        = 50;
    expected_writes = 0;
  } else if (mode == "donation-row-unavailable-owned") {
    owned_throws    = true;
    expected        = 50;
    expected_writes = 0;
  } else {
    Require(mode == "donation-row-native", "known scoped donation row policy mode");
    context.owned = 30;
    expected      = 30;
  }

  const auto roots            = warp_roots.size();
  int        originals        = 0;
  int64_t    rendered_maximum = -1;
  InventoryUseRowWidget_SetWidgetData_Hook(
      [&](InventoryUseRowWidget* actual) {
        Require(actual == owner, "scoped donation row keeps the original receiver");
        ++originals;
        rendered_maximum = context.maximum;
      },
      owner);
  Require(originals == 1, "scoped donation row always renders through original exactly once");
  Require(writes == expected_writes && context.maximum == expected && rendered_maximum == expected,
          "scoped donation applies current policy after the final managed context observation");
  Require(replacement.maximum == 50, "scoped donation never changes the replacement context");
  if (mode == "donation-row-replace-off")
    Require(replaced && initial_config.ui.extend_donation_slider,
            "scoped Config replacement leaves the previous enabled policy unchanged");
  Require(warp_roots.size() == roots, "scoped donation row releases all temporary roots");
  bindings_config_override = nullptr;
  bindings_replacement_config.reset();
  std::cout << "Scoped donation row policy callback passed: " << mode << '\n';
}
} // namespace donation_row_policy_fixture

namespace
{
void DonationRowPolicyCases(std::string_view mode)
{ donation_row_policy_fixture::Run(mode); }
} // namespace
#endif
