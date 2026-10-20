#pragma once
// Included inside claim_trace.cc's private namespace. Reuses existing callbacks;
// catalog references are retained, account/order/instance identifiers are not.
namespace receipt_trace
{
std::shared_ptr<spdlog::logger> journal;
std::atomic_bool                healthy{true};
std::atomic_uint64_t            sequence{1};
constexpr int                   kLimit = 2048;

bool Is(void* object, const char* ns, const char* name)
{
  if (!object)
    return false;
  auto* cls = il2cpp_object_get_class(static_cast<Il2CppObject*>(object));
  return !std::strcmp(il2cpp_class_get_namespace(cls), ns) && !std::strcmp(il2cpp_class_get_name(cls), name);
}

// Each nested list shares one budget. Unsupported/oversize collections never
// masquerade as empty lists. Reference array layout and element size are checked.
template <typename F> Json List(void* list, int& budget, F item)
{
  Json result = {{"present", list != nullptr}, {"supported", false}, {"complete", false}};
  if (!list)
    return result;
  auto*   cls   = il2cpp_object_get_class(static_cast<Il2CppObject*>(list));
  void*   array = list;
  int32_t count{};
  if (il2cpp_class_get_rank(cls))
    count = static_cast<int32_t>(il2cpp_array_length(static_cast<Il2CppArray*>(list)));
  else {
    const bool repeated = Is(list, "Google.Protobuf.Collections", "RepeatedField`1");
    if (!repeated && !Is(list, "System.Collections.Generic", "List`1"))
      return result;
    if (!Read(list, repeated ? "count" : "_size", count) || !Read(list, repeated ? "array" : "_items", array) || !array)
      return result;
  }
  result["count"] = count;
  cls             = il2cpp_object_get_class(static_cast<Il2CppObject*>(array));
  if (count < 0 || il2cpp_class_get_rank(cls) != 1
      || static_cast<uint32_t>(count) > il2cpp_array_length(static_cast<Il2CppArray*>(array))
      || il2cpp_class_is_valuetype(il2cpp_class_get_element_class(cls))
      || il2cpp_array_element_size(cls) != sizeof(void*))
    return result;
  if (count > budget) {
    result["bounded_out"] = true;
    return result;
  }
  budget -= count;
  result["supported"] = true;
  result["complete"]  = true;
  result["items"]     = Json::array();
  auto** entries      = reinterpret_cast<void**>(static_cast<Il2CppArraySize*>(array)->vector);
  for (int32_t i = 0; i < count; ++i) {
    auto row = item(entries[i]);
    if (!row.value("complete", false))
      result["complete"] = false;
    result["items"].push_back(std::move(row));
  }
  return result;
}

Json CatalogText(void* object, const char* field)
{
  std::u16string value;
  if (!Text(object, field, value) || value.empty() || value.size() > 96)
    return nullptr;
  std::string result;
  for (auto c : value) {
    if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || c == u'_' || c == u'-'
          || c == u':' || c == u'.'))
      return nullptr;
    result.push_back(static_cast<char>(c));
  }
  return result;
}

Json ContentItem(void* object)
{
  Json result = {{"complete", false}};
  if (!Is(object, "Digit.PrimePlatform.Content", "ContentItem"))
    return result;
  result["catalog_id"] = CatalogText(object, "id_");
  result["type"]       = CatalogText(object, "type_");
  Scalar<int64_t>(result, object, "quantity_", "quantity");
  result["complete"] = !result["catalog_id"].is_null() && !result["type"].is_null() && !result["quantity"].is_null();
  return result;
}

Json ContentList(void* object, int& budget)
{
  void* items{};
  if (!Is(object, "Digit.PrimePlatform.Content", "ContentItemList") || !Read(object, "items_", items))
    return {{"present", object != nullptr}, {"complete", false}};
  return List(items, budget, ContentItem);
}

Json BundleContents(void* object, int& budget)
{
  Json result = {{"present", object != nullptr}, {"complete", false}};
  if (!Is(object, "Digit.PrimePlatform.Content", "BundleContent"))
    return result;
  bool complete = true;
  for (const char* field : {"static_", "multiplied_", "gifts_", "showcase_", "inviter_"}) {
    void* value{};
    if (!Read(object, field, value)) {
      result[field] = {{"complete", false}};
      complete      = false;
    } else if (value) {
      result[field] = ContentList(value, budget);
      complete      = complete && result[field].value("complete", false);
    } else
      result[field] = {{"present", false}, {"complete", true}};
  }
  void* value{};
  if (Read(object, "dynamic_", value)) {
    result["dynamic"] = List(value, budget, [&](void* row) { return ContentList(row, budget); });
    complete          = complete && result["dynamic"].value("complete", false);
  } else
    complete = false;
  if (Read(object, "customized_", value)) {
    result["customized"] = List(value, budget, [&](void* row) {
      void* contents{};
      return Is(row, "Digit.PrimePlatform.Content", "CustomizedSet") && Read(row, "contents_", contents)
                 ? ContentList(contents, budget)
                 : Json{{"complete", false}};
    });
    complete             = complete && result["customized"].value("complete", false);
  } else
    complete = false;
  result["complete"] = complete;
  return result;
}

Json GrantItem(void* object, int& budget, int depth = 0)
{
  Json result = {{"complete", false}};
  if (depth > 4 || !Is(object, "Digit.PrimePlatform.Content", "ClaimGrantInventoryItem"))
    return result;
  Scalar<int32_t>(result, object, "type_", "type");
  Scalar<int64_t>(result, object, "count_", "count");
  void* params{};
  if (!Read(object, "params_", params) || !Is(params, "Digit.PrimePlatform.Content", "ClaimGrantInventoryItemParams"))
    return result;
  Scalar<int64_t>(result, params, "refId_", "ref_id");
  Scalar<int64_t>(result, params, "amount_", "amount");
  void* children{};
  if (!Read(params, "items_", children))
    return result;
  result["children"] = List(children, budget, [&](void* row) { return GrantItem(row, budget, depth + 1); });
  result["complete"] = !result["type"].is_null() && !result["count"].is_null() && !result["ref_id"].is_null()
                       && !result["amount"].is_null() && result["children"].value("complete", false);
  return result;
}

Json Grant(void* object, int& budget)
{
  Json result = {{"present", object != nullptr}, {"complete", false}};
  if (!Is(object, "Digit.PrimePlatform.Content", "ClaimGrant"))
    return result;
  Scalar<int32_t>(result, object, "state_", "state");
  Scalar<bool>(result, object, "requiresSync_", "requires_sync");
  void *delta{}, *notifications{};
  if (!Read(object, "delta_", delta) || !Read(delta, "notifications_", notifications))
    return result;
  result["chest_notifications"] = List(notifications, budget, [&](void* notification) {
    Json  row = {{"complete", false}};
    void *params{}, *chests{};
    if (!Is(notification, "Digit.PrimePlatform.Content", "ClaimGrantNotification")
        || !Read(notification, "platformChestParams_", params))
      return row;
    if (!params)
      return Json{{"platform_chest", false}, {"complete", true}};
    Scalar<int32_t>(row, params, "numChests_", "chest_count");
    if (!Read(params, "chestsContents_", chests))
      return row;
    row["chests"]   = List(chests, budget, [&](void* chest) {
      void* items{};
      return Is(chest, "Digit.PrimePlatform.Content", "ClaimGrantChestContent") && Read(chest, "items_", items)
                 ? List(items, budget, [&](void* item) { return GrantItem(item, budget); })
                 : Json{{"complete", false}};
    });
    row["complete"] = !row["chest_count"].is_null() && row["chests"].value("complete", false);
    return row;
  });
  // Resource/inventory maps are absolute server state, not incremental rewards.
  // This receipt captures chest notifications; it does not claim map coverage.
  result["complete"] = result["chest_notifications"].value("complete", false) && !result["state"].is_null()
                       && !result["requires_sync"].is_null();
  result["scope"]    = "chest_notifications_only";
  return result;
}

Json PurchaseOrders(void* orders)
{
  int budget = kLimit;
  return List(orders, budget, [&](void* order) {
    Json row = {{"complete", false}};
    if (!Is(order, "Digit.PrimePlatform.Content", "Order"))
      return row;
    std::u16string id;
    if (Text(order, "orderId_", id))
      row["order_token"] = Token(orderTokens, id);
    void *items{}, *grant{};
    if (!Read(order, "orderItems_", items) || !Read(order, "grant_", grant))
      return row;
    row["content"]  = List(items, budget, [&](void* item) {
      void* contents{};
      return Is(item, "Digit.PrimePlatform.Content", "OrderItem") && Read(item, "content_", contents)
                 ? BundleContents(contents, budget)
                 : Json{{"complete", false}};
    });
    row["grant"]    = Grant(grant, budget);
    row["complete"] = row.contains("order_token") && row["content"].value("complete", false);
    return row;
  });
}

Json LongGetter(void* object, const char* name)
{
  auto* cls    = il2cpp_object_get_class(static_cast<Il2CppObject*>(object));
  auto* method = il2cpp_class_get_method_from_name(cls, name, 0);
  if (!method || il2cpp_type_get_type(il2cpp_method_get_return_type(method)) != IL2CPP_TYPE_I8
      || (il2cpp_method_get_flags(method, nullptr) & METHOD_ATTRIBUTE_STATIC))
    return nullptr;
  auto* value = Invoke(object, name);
  if (!value || il2cpp_type_get_type(il2cpp_class_get_type(value->klass)) != IL2CPP_TYPE_I8)
    return nullptr;
  return *static_cast<int64_t*>(il2cpp_object_unbox(value));
}

Json InventoryItem(void* object)
{
  Json row = {{"complete", false}};
  if (!Is(object, "Digit.PrimeServer.Models", "InventoryItem"))
    return row;
  Scalar<int32_t>(row, object, "type_", "type");
  Scalar<int64_t>(row, object, "count_", "count");
  Scalar<int64_t>(row, object, "quantity_", "quantity");
  row["ref_id"]   = LongGetter(object, "get_RefId");
  row["amount"]   = LongGetter(object, "get_Amount");
  row["complete"] = !row["type"].is_null() && !row["count"].is_null() && !row["quantity"].is_null()
                    && !row["ref_id"].is_null() && !row["amount"].is_null();
  return row;
}

Json InventoryList(void* list, int& budget)
{ return List(list, budget, InventoryItem); }

// Read a validated inline value-type field without boxing or calling container
// methods (FlattenedItemsList can merge/mutate its cache).
void* ContainerList(void* context, const char* member)
{
  auto* field = Field(context, "_internalRewardContainer");
  auto* cls   = field ? il2cpp_class_from_type(field->type) : nullptr;
  if (!cls || !il2cpp_class_is_valuetype(cls)
      || std::strcmp(il2cpp_class_get_namespace(cls), "Digit.Prime.SharedFeatures")
      || std::strcmp(il2cpp_class_get_name(cls), "InternalRewardContainer"))
    return nullptr;
  const auto size  = il2cpp_class_value_size(cls, nullptr);
  auto*      inner = il2cpp_class_get_field_from_name(cls, member);
  if (size <= 0 || !inner || (il2cpp_field_get_flags(inner) & FIELD_ATTRIBUTE_STATIC) || inner->type->byref
      || inner->offset < sizeof(Il2CppObject) || inner->offset + sizeof(void*) > sizeof(Il2CppObject) + size
      || field->offset + size
             > il2cpp_class_instance_size(il2cpp_object_get_class(static_cast<Il2CppObject*>(context))))
    return nullptr;
  auto* valueClass = il2cpp_class_from_type(inner->type);
  if (!valueClass || il2cpp_class_is_valuetype(valueClass)
      || std::strcmp(il2cpp_class_get_namespace(valueClass), "System.Collections.Generic")
      || std::strcmp(il2cpp_class_get_name(valueClass), "List`1"))
    return nullptr;
  void* result{};
  std::memcpy(&result, static_cast<const char*>(context) + field->offset + inner->offset - sizeof(Il2CppObject),
              sizeof(result));
  return result;
}

Json Presentation(void* context)
{
  int   budget = kLimit;
  Json  result;
  void* orders{};
  if (Read(context, "<RewardPresentationOrderIds>k__BackingField", orders))
    result["orders"] = Orders(orders);
  Scalar<int32_t>(result, context, "<PurchasedChestsNumber>k__BackingField", "purchased_chests");
  for (const char* member :
       {"_currentItemsList", "_sortedCurrentItemsList", "_flattenedItemsList", "_secondRewardItemsList"})
    result[member] = InventoryList(ContainerList(context, member), budget);
  result["all_pages"] =
      List(ContainerList(context, "_allItemsList"), budget, [&](void* page) { return InventoryList(page, budget); });
  return result;
}

Json StoryboardReceipt(void* object)
{
  int   budget = kLimit;
  Json  result = {{"storyboard", Storyboard(object)}};
  void* list{};
  if (Read(object, "_filteredItemsForReveal", list))
    result["items"] = InventoryList(list, budget);
  void* context{};
  if (Read(object, "_shopSectionContext", context))
    result["context"] = Presentation(context);
  return result;
}

template <typename F> void Write(const char* phase, uint64_t span, F capture) noexcept
{
  if (!ready.load() || !journal || !span)
    return;
  try {
    Json event = {{"schema", 1},
                  {"session", sessionId},
                  {"seq", sequence.fetch_add(1)},
                  {"span", span},
                  {"phase", phase},
                  {"data", capture()},
                  {"utc_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count()}};
    journal->info("{}", event.dump());
    PublishDiagnosticEvent("claims/receipts", event);
  } catch (...) {
    healthy.store(false);
  }
}

void ViewReceipt(const char* phase, uint64_t span, void* object) noexcept
{
  Write(phase, span, [&] {
    void* context{};
    Json  result      = {{"context_read", Read(object, "m_context", context)}};
    result["context"] = Presentation(context);
    void* filtered{};
    int   budget = kLimit;
    if (Read(object, "_filteredRewards", filtered))
      result["summary_items"] = InventoryList(filtered, budget);
    return result;
  });
}

void Install() noexcept
{
  try {
    journal = spdlog::rotating_logger_mt("claim-receipts", ScienceTracePath("claims", "claim_receipts.jsonl").string(),
                                         16 * 1024 * 1024, 4, false);
    journal->set_pattern("%v");
    journal->set_level(spdlog::level::info);
    journal->flush_on(spdlog::level::info);
    journal->set_error_handler([](const std::string&) { healthy.store(false); });
  } catch (...) {
    healthy.store(false);
  }
}

Json Status()
{
  bool metadataReady = true;
  for (auto [assembly, ns, cls, field] :
       {std::tuple{"Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "ContentItem", "quantity_"},
        std::tuple{"Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "Order", "orderItems_"},
        std::tuple{"Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "ClaimGrant", "delta_"},
        std::tuple{"Digit.Client.PrimeLib.Runtime", "Digit.PrimePlatform.Content", "ClaimGrantInventoryItemParams",
                   "amount_"},
        std::tuple{"Digit.Client.PrimeLib.Runtime", "Digit.PrimeServer.Models", "InventoryItem", "quantity_"},
        std::tuple{"Assembly-CSharp", "Digit.Prime.Shop", "ShopSectionContext", "_internalRewardContainer"},
        std::tuple{"Assembly-CSharp", "Digit.Prime.Shop", "ShopSummaryViewController", "_filteredRewards"}}) {
    auto* type    = Resolve(assembly, ns, cls);
    metadataReady = metadataReady && type && il2cpp_class_get_field_from_name(type, field);
  }
  return {
      {"healthy", journal != nullptr && healthy.load()},
      {"journal", "claim_receipts.jsonl"},
      {"metadata_ready", metadataReady},
      {"sequence", sequence.load()},
      {"max_entries_per_snapshot", kLimit},
      {"scope", "purchase order content, grant chest notifications, presentation lists; no absolute inventory map"}};
}
} // namespace receipt_trace
