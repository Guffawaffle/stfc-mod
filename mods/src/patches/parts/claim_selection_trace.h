#pragma once
// Included within claim_trace.cc's private namespace. All observation is passive;
// the existing game call executes once and no preference getter is invoked by the probe.
namespace selection_trace
{
std::shared_ptr<spdlog::logger>     journal;
std::atomic_bool                    healthy{true};
std::atomic_uint64_t                sequence{1};
size_t                              installed{};
std::array<Il2CppMethodPointer, 13> installedPointers{};
std::atomic_bool                    sharedCloudLoadSeen{false};

std::mutex            checkpointMutex;
std::filesystem::path checkpointPath;
Json                  checkpoint = Json::object();

bool ValidList(const Json& list)
{
  if (!list.is_object() || !list.value("supported", false) || !list.contains("entries") || !list["entries"].is_array()
      || list["entries"].size() > 2048 || list.value("count", size_t(-1)) != list["entries"].size())
    return false;
  for (const auto& row : list["entries"]) {
    if (!row.is_object() || !row.contains("bundle") || !row["bundle"].is_string() || !row.contains("quantity")
        || !row["quantity"].is_number_unsigned())
      return false;
    const auto alias = row["bundle"].get<std::string>();
    if (alias.size() != 33 || alias[0] != 'b' || row["quantity"].get<uint64_t>() > UINT32_MAX)
      return false;
    for (auto c : alias.substr(1))
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
        return false;
  }
  return true;
}

void Checkpoint(Json& event)
{
  const auto method = event.value("event", std::string{});
  if (method != "preferences_get" && method != "preferences_set")
    return;
  const auto& detail = event.at("details");
  const auto  key    = detail.value("key", std::string{});
  if (key != "StoreSelection" && key != "RefineSelection" && key != "FactionSelection")
    return;
  std::lock_guard lock(checkpointMutex);
  if (method == "preferences_get" && checkpoint.contains(key))
    event["checkpoint"] = checkpoint[key];
  const auto& list    = detail.at(method == "preferences_get" ? "read" : "written");
  bool        changed = false;
  if (event.value("unwind", false) || !ValidList(list)) {
    if (method == "preferences_set")
      changed = checkpoint.erase(key) != 0;
  } else if (method == "preferences_set" || !checkpoint.contains(key)) {
    checkpoint[key] = {{"basis", method == "preferences_set" ? "write" : "initial_read"},
                       {"session", sessionId},
                       {"utc_ms", event.at("utc_ms")},
                       {"values", list}};
    changed         = true;
  }
  if (changed)
    SaveControlJson(checkpointPath, {{"schema", 1}, {"values", checkpoint}});
}

// Stable catalog aliases join sessions without writing catalog/account/order IDs.
// These are correlation labels, not a cryptographic privacy boundary.
std::string Alias(int64_t value)
{
  uint64_t a = 14695981039346656037ULL, b = 7809847782465536322ULL;
  for (unsigned char c : std::to_string(value)) {
    a = (a ^ c) * 1099511628211ULL;
    b = (b ^ c) * 14029467366897019727ULL;
  }
  return fmt::format("b{:016x}{:016x}", a, b);
}

std::string PreferenceKey(void* value)
{
  if (!value)
    return {};
  auto* object = static_cast<Il2CppObject*>(value);
  if (il2cpp_type_get_type(il2cpp_class_get_type(object->klass)) != IL2CPP_TYPE_STRING)
    return {};
  auto*      str = static_cast<Il2CppString*>(value);
  const auto n   = il2cpp_string_length(str);
  if (n <= 0 || n > 32)
    return {};
  std::u16string text(reinterpret_cast<const char16_t*>(il2cpp_string_chars(str)), n);
  for (auto [key, name] :
       {std::pair{u"StoreSelection", "StoreSelection"}, std::pair{u"RefineSelection", "RefineSelection"},
        std::pair{u"FactionSelection", "FactionSelection"}})
    if (text == key)
      return name;
  return {};
}

Json SerializedSelection(void* list)
{
  Json    result = {{"present", list != nullptr}, {"supported", false}};
  int32_t count{};
  if (!list || !Read(list, "_size", count) || count < 0 || count > 2048)
    return result;
  auto items = References(list, 2048);
  if (items.size() != static_cast<size_t>(count))
    return result;
  result["supported"] = true;
  result["count"]     = count;
  result["entries"]   = Json::array();
  for (auto* item : items) {
    auto* object = static_cast<Il2CppObject*>(item);
    if (!object || il2cpp_type_get_type(il2cpp_class_get_type(object->klass)) != IL2CPP_TYPE_STRING) {
      result["entries"].push_back({{"supported", false}});
      continue;
    }
    auto*       str = static_cast<Il2CppString*>(item);
    const auto  n   = il2cpp_string_length(str);
    std::string text;
    if (n > 0 && n <= 48) {
      auto* chars = il2cpp_string_chars(str);
      for (int i = 0; i < n; ++i) {
        if ((chars[i] >= '0' && chars[i] <= '9') || chars[i] == ':' || chars[i] == '-')
          text.push_back(static_cast<char>(chars[i]));
        else {
          text.clear();
          break;
        }
      }
    }
    auto     separator = text.find(':');
    int64_t  id{};
    uint32_t quantity{};
    bool     valid = separator != std::string::npos;
    if (valid) {
      auto first  = std::from_chars(text.data(), text.data() + separator, id);
      auto second = std::from_chars(text.data() + separator + 1, text.data() + text.size(), quantity);
      valid       = first.ec == std::errc{} && first.ptr == text.data() + separator && second.ec == std::errc{}
                    && second.ptr == text.data() + text.size();
    }
    if (valid)
      result["entries"].push_back({{"bundle", Alias(id)}, {"quantity", quantity}});
    else
      result["entries"].push_back({{"supported", false}});
  }
  return result;
}

// Dictionary layouts are checked through metadata, including the unboxed entry
// offsets/stride. Missing, changed or oversized layouts remain explicitly unknown.
Json Map(void* dictionary, bool groups, bool categories, int& budget)
{
  Json    result = {{"present", dictionary != nullptr}, {"supported", false}};
  int32_t count{}, free{};
  void*   entries{};
  if (!dictionary || !Read(dictionary, "_count", count) || !Read(dictionary, "_freeCount", free)
      || !Read(dictionary, "_entries", entries) || count < 0 || free < 0 || free > count || count > 4096)
    return result;
  result["count"] = count - free;
  if (!entries) {
    if (count == 0) {
      result["supported"] = true;
      result["entries"]   = Json::array();
    }
    return result;
  }
  auto* array   = static_cast<Il2CppArray*>(entries);
  auto* cls     = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(array));
  auto* element = il2cpp_class_get_element_class(cls);
  if (il2cpp_class_get_rank(cls) != 1 || !element || !il2cpp_class_is_valuetype(element)
      || static_cast<uint32_t>(count) > il2cpp_array_length(array))
    return result;
  const auto stride = il2cpp_array_element_size(cls);
  if (stride <= 0 || stride > 64)
    return result;
  auto member = [&](const char* name, size_t bytes) -> FieldInfo* {
    auto* f = il2cpp_class_get_field_from_name(element, name);
    return f && !(il2cpp_field_get_flags(f) & FIELD_ATTRIBUTE_STATIC) && !f->type->byref
                   && f->offset >= sizeof(Il2CppObject) && f->offset + bytes <= sizeof(Il2CppObject) + stride
               ? f
               : nullptr;
  };
  auto* hash  = member("hashCode", 4);
  auto* next  = member("next", 4);
  auto* key   = member("key", categories ? 4 : 8);
  auto* value = member("value", groups ? sizeof(void*) : 4);
  if (!hash || !next || !key || !value || il2cpp_type_get_type(next->type) != IL2CPP_TYPE_I4)
    return result;
  const auto hashType = il2cpp_type_get_type(hash->type);
  if (hashType != IL2CPP_TYPE_I4 && hashType != IL2CPP_TYPE_U4)
    return result;
  auto*      keyClass   = il2cpp_class_from_type(key->type);
  auto*      valueClass = il2cpp_class_from_type(value->type);
  const bool keyValid = categories
                            ? keyClass && !std::strcmp(keyClass->namespaze, "Digit.Prime.Shop")
                                  && !std::strcmp(keyClass->name, "ShopCategory") && il2cpp_class_is_valuetype(keyClass)
                                  && il2cpp_class_value_size(keyClass, nullptr) == 4
                            : il2cpp_type_get_type(key->type) == IL2CPP_TYPE_I8;
  const bool valueValid = groups ? valueClass && !il2cpp_class_is_valuetype(valueClass)
                                       && !std::strcmp(valueClass->namespaze, "Digit.Prime.Shop")
                                       && !std::strcmp(valueClass->name, "BundleQuantitySelection")
                                 : il2cpp_type_get_type(value->type) == IL2CPP_TYPE_U4;
  if (!keyValid || !valueValid)
    return result;
  Json rows = Json::array();
  for (int32_t i = 0; i < count; ++i) {
    auto* entry = reinterpret_cast<const char*>(static_cast<Il2CppArraySize*>(entries)->vector) + i * stride;
    auto  read  = [&](FieldInfo* field, auto& output) {
      std::memcpy(&output, entry + field->offset - sizeof(Il2CppObject), sizeof(output));
    };
    int32_t hashValue{}, nextValue{};
    read(hash, hashValue);
    read(next, nextValue);
    if ((hashType == IL2CPP_TYPE_I4 && hashValue < 0) || nextValue < -1)
      continue;
    if (--budget < 0) {
      result["bounded_out"] = true;
      return result;
    }
    int64_t id{};
    int32_t category{};
    if (categories)
      read(key, category);
    else
      read(key, id);
    Json row = {{"key", categories ? Json(category) : Json(Alias(id))}};
    if (groups) {
      void *selection{}, *selected{}, *editing{};
      read(value, selection);
      const bool selectedKnown = Read(selection, "<SelectedBundleQuantityDictionary>k__BackingField", selected);
      const bool editingKnown  = Read(selection, "<VolatileBundleQuantityDictionary>k__BackingField", editing);
      row["selected"]          = selectedKnown ? Map(selected, false, false, budget) : Json(nullptr);
      row["editing"]           = editingKnown ? Map(editing, false, false, budget) : Json(nullptr);
    } else {
      uint32_t quantity{};
      read(value, quantity);
      row["quantity"] = quantity;
    }
    rows.push_back(std::move(row));
  }
  if (rows.size() != static_cast<size_t>(count - free))
    return result;
  result["supported"] = true;
  result["entries"]   = std::move(rows);
  return result;
}

Json Container(void* object)
{
  Json result = {{"present", object != nullptr}};
  int  budget = 2048;
  for (auto [field, label, groups, categories] :
       {std::tuple{"_bundleQuantitySelectionByCategory", "categories", true, true},
        std::tuple{"_bundleQuantitySelectionById", "groups", true, false},
        std::tuple{"_pendingBundleQuantityDictionary", "pending", false, false}}) {
    void* value{};
    result[label] = Read(object, field, value) ? Map(value, groups, categories, budget) : Json(nullptr);
  }
  return result;
}

Json State(void* object, const char* kind)
{
  if (!std::strcmp(kind, "container"))
    return Container(object);
  Json result = {{"present", object != nullptr}};
  if (!std::strcmp(kind, "preferences")) {
    Scalar<bool>(result, object, "_isCloudFileLoaded", "cloud_loaded");
    Scalar<bool>(result, object, "_isLoading", "loading");
    Scalar<float>(result, object, "_saveInterval", "save_interval");
    void* data{};
    if (Read(object, "_savedData", data))
      Scalar<bool>(result, data, "<IsDirty>k__BackingField", "dirty");
    return result;
  }
  void* context = object;
  if (!std::strcmp(kind, "shop")) {
    Scalar<bool>(result, object, "_hasLoadedPersistedRefineSelection", "selection_loaded");
    if (!Read(object, "_mainContext", context))
      return result;
  }
  void* container{};
  Scalar<bool>(result, context, "_isInBulkSelectionMode", "editing");
  if (Read(context, "_bulkSelectionContainer", container))
    result["selection"] = Container(container);
  return result;
}

Json Bundle(void* object)
{
  Json    result = {{"present", object != nullptr}};
  int64_t id{};
  if (Read(object, "bundleId_", id))
    result["bundle"] = Alias(id);
  void* refs{};
  if (Read(object, "_idRefs", refs) && Read(refs, "locaId_", id))
    result["localization"] = Alias(id);
  Scalar<bool>(result, object, "_isWaitingForRefresh", "waiting_for_refresh");
  return result;
}

Json Bundles(void* list)
{
  int32_t count{};
  Json    result = {{"supported", false}};
  if (!Read(list, "_size", count) || count < 0 || count > 2048)
    return result;
  auto items = References(list, 2048);
  if (items.size() != static_cast<size_t>(count))
    return result;
  result["supported"] = true;
  result["items"]     = Json::array();
  for (auto* item : items)
    result["items"].push_back(Bundle(item));
  return result;
}

void WriteEvent(Json event) noexcept
{
  try {
    if (!journal)
      return;
    event["schema"]  = 1;
    event["session"] = sessionId;
    event["utc_ms"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    event["seq"] = sequence++;
    Checkpoint(event);
    journal->info("{}", event.dump());
  } catch (...) {
    healthy.store(false);
  }
}

Json QuantityMap(const Json& source)
{
  if (!source.is_object() || !source.value("supported", false) || !source.contains("entries"))
    return nullptr;
  Json result = Json::object();
  for (const auto& entry : source["entries"]) {
    const auto& key                                               = entry.at("key");
    result[key.is_string() ? key.get<std::string>() : key.dump()] = entry.at("quantity");
  }
  return result;
}

Json SelectionMaps(const Json& state)
{
  Json result = Json::object();
  if (!state.is_object())
    return result;
  const auto& container = state.contains("selection") ? state["selection"] : state;
  if (!container.is_object())
    return result;
  if (container.contains("pending"))
    result["pending"] = QuantityMap(container["pending"]);
  for (auto name : {"categories", "groups"}) {
    if (!container.contains(name))
      continue;
    const auto&       groups   = container[name];
    const std::string coverage = std::string(name) + "/$coverage";
    if (!groups.is_object() || !groups.value("supported", false) || !groups.contains("entries")) {
      result[coverage] = nullptr;
      continue;
    }
    result[coverage] = Json::object();
    for (const auto& group : groups["entries"]) {
      const auto& key    = group.at("key");
      const auto  prefix = std::string(name) + "/" + (key.is_string() ? key.get<std::string>() : key.dump()) + "/";
      for (auto phase : {"selected", "editing"})
        result[prefix + phase] = QuantityMap(group.at(phase));
    }
  }
  return result;
}

void CompactObservation(Json& event)
{
  auto before = SelectionMaps(event["before"]);
  auto after  = SelectionMaps(event["after"]);
  auto scopes = before;
  scopes.update(after);
  event["selection_changes"] = Json::array();
  event["selection_unknown"] = Json::array();
  for (const auto& [scope, unused] : scopes.items()) {
    Json       first = before.value(scope, Json{}), second = after.value(scope, Json{});
    const auto separator = scope.find('/');
    if (separator != std::string::npos && scope.find('/', separator + 1) != std::string::npos) {
      const auto coverage = scope.substr(0, separator) + "/$coverage";
      if (before.contains(coverage) && after.contains(coverage) && before[coverage].is_object()
          && after[coverage].is_object()) {
        if (!before.contains(scope))
          first = Json::object();
        if (!after.contains(scope))
          second = Json::object();
      }
    }
    if (!first.is_object() || !second.is_object()) {
      event["selection_unknown"].push_back(scope);
      continue;
    }
    if (scope.ends_with("/$coverage"))
      continue;
    auto keys = first;
    keys.update(second);
    Json changes = Json::array();
    for (const auto& [key, value] : keys.items()) {
      const auto oldQuantity = first.value(key, uint32_t{}), newQuantity = second.value(key, uint32_t{});
      if (oldQuantity != newQuantity)
        changes.push_back({{"bundle", key}, {"before", oldQuantity}, {"after", newQuantity}});
    }
    if (!changes.empty())
      event["selection_changes"].push_back({{"scope", scope}, {"changes", std::move(changes)}});
  }
  // Tab browsing must not consume retention with repeated full dictionaries.
  // Keep per-call deltas, flags, saved lists and bounded offer identities.
  for (auto phase : {"before", "after"})
    if (event[phase].is_object())
      for (auto key : {"selection", "categories", "groups", "pending"})
        event[phase].erase(key);
}

struct Observation {
  void*       object;
  const char* kind;
  Json        event;
  int         exceptions = std::uncaught_exceptions();
  Observation(const char* method, void* object, const char* kind, bool enabled = true) noexcept
      : object(object)
      , kind(kind)
  {
    try {
      if (enabled && ready.load() && journal) {
        event = {{"event", method}, {"span", nextSpan++}, {"before", State(object, kind)}};
        WriteEvent({{"event", "begin"}, {"method", method}, {"span", event["span"]}});
      }
    } catch (...) {
      event = nullptr;
    }
  }
  template <class F> void Details(F make) noexcept
  {
    try {
      if (!event.is_null())
        event["details"].update(make());
    } catch (...) {
      try {
        event["details_unknown"] = true;
      } catch (...) {
      }
    }
  }
  ~Observation() noexcept
  {
    try {
      if (!event.is_null()) {
        event["after"] = State(object, kind);
        CompactObservation(event);
        event["unwind"] = std::uncaught_exceptions() > exceptions;
        WriteEvent(std::move(event));
      }
    } catch (...) {
      healthy.store(false);
    }
  }
};

void Edit(auto original, void* self, void* bundle, uint32_t quantity)
{
  Observation observation("edit", self, "container");
  observation.Details([&] { return Json{{"bundle", Bundle(bundle)}, {"quantity", quantity}}; });
  original(self, bundle, quantity);
}
void Override(auto original, void* self, int64_t key, void* replacement)
{
  Observation observation("override", self, "container");
  observation.Details([&] {
    int budget = 2048;
    return Json{{"group", Alias(key)}, {"replacement", Map(replacement, false, false, budget)}};
  });
  original(self, key, replacement);
}
void Parse(auto original, void* self, void* list)
{
  Observation observation("parse", self, "container");
  observation.Details([&] { return Json{{"incoming", SerializedSelection(list)}}; });
  original(self, list);
}
void Apply(auto original, void* self)
{
  Observation observation("apply", self, "container");
  original(self);
}
void Discard(auto original, void* self)
{
  Observation observation("discard_edits", self, "container");
  original(self);
}
void Reset(auto original, void* self, int64_t key)
{
  Observation observation("reset", self, "container");
  observation.Details([&] { return Json{{"group", Alias(key)}}; });
  original(self, key);
}
void Save(auto original, void* self, bool force)
{
  Observation observation("save_selection", self, "shop");
  observation.Details([&] { return Json{{"forced", force}}; });
  original(self, force);
}
void Purge(auto original, void* self, int32_t category, void* bundles)
{
  Observation observation("prune", self, "shop");
  observation.Details([&] { return Json{{"category", category}, {"available", Bundles(bundles)}}; });
  original(self, category, bundles);
}
void Load(auto original, void* self, int32_t section)
{
  Observation observation("load_selection", self, "shop");
  observation.Details([&] { return Json{{"section", section}}; });
  original(self, section);
}
void Claim(auto original, void* self, int32_t category)
{
  Observation observation("claim_selected", self, "shop");
  observation.Details([&] { return Json{{"category", category}}; });
  original(self, category);
}
void Set(auto original, void* self, void* key, void* list, bool userKey)
{
  std::string name;
  try {
    name = PreferenceKey(key);
  } catch (...) {
  }
  Observation observation("preferences_set", self, "preferences", !name.empty());
  observation.Details(
      [&] { return Json{{"key", name}, {"user_key", userKey}, {"written", SerializedSelection(list)}}; });
  original(self, key, list, userKey);
}
void* Get(auto original, void* self, void* key, bool userKey)
{
  std::string name;
  try {
    name = PreferenceKey(key);
  } catch (...) {
  }
  Observation observation("preferences_get", self, "preferences", !name.empty());
  auto*       result = original(self, key, userKey);
  observation.Details(
      [&] { return Json{{"key", name}, {"user_key", userKey}, {"read", SerializedSelection(result)}}; });
  return result;
}
void CloudSave(auto original, void* self, void* success, void* failure)
{
  Observation       observation("cloud_save_attempt", self, "preferences");
  ClaimRequestScope scope(nullptr, "persistent-preferences-save");
  original(self, success, failure);
}
const Target targets[] = {
    {"Assembly-CSharp", "Digit.Prime.Shop", "BulkSelectionContainer", "SetBundleToBulkSelection", 2, false, "pi"},
    {"Assembly-CSharp", "Digit.Prime.Shop", "BulkSelectionContainer", "OverrideSelection", 2, false, "qp"},
    {"Assembly-CSharp", "Digit.Prime.Shop", "BulkSelectionContainer", "ParseBulkSelectionList", 1, false, "p"},
    {"Assembly-CSharp", "Digit.Prime.Shop", "BulkSelectionContainer", "ApplyChanges", 0, false, ""},
    {"Assembly-CSharp", "Digit.Prime.Shop", "BulkSelectionContainer", "ClearChanges", 0, false, ""},
    {"Assembly-CSharp", "Digit.Prime.Shop", "BulkSelectionContainer", "ResetSelection", 1, false, "q"},
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager", "SaveBulkSelectionCollection", 1, false, "b"},
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager", "PurgeSelectedBundles", 2, false, "ip"},
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager", "TryLoadPlayerBulkSelectionList", 1, false, "i"},
    {"Assembly-CSharp", "Digit.Prime.Shop", "ShopSceneManager", "BulkClaimSelectionByCategory", 1, false, "i"},
    {"Assembly-CSharp", "Digit.Prime.PersistentPrefs", "PersistentPrefsManager", "SetStringList", 3, false, "ppb"},
    {"Assembly-CSharp", "Digit.Prime.PersistentPrefs", "PersistentPrefsManager", "GetStringList", 2, false, "pb"},
    {"Assembly-CSharp", "Digit.Prime.PersistentPrefs", "PersistentPrefsManager", "SavePersistentPrefsToCloud", 2, false,
     "pp"},
};

const MethodInfo* Method(size_t index)
{
  const auto&       target = targets[index];
  auto*             cls    = Resolve(target.assembly, target.ns, target.cls);
  const MethodInfo* result{};
  void*             iterator{};
  while (auto* candidate = cls ? il2cpp_class_get_methods(cls, &iterator) : nullptr) {
    if (std::strcmp(candidate->name, target.method) || candidate->parameters_count != target.arguments
        || !candidate->methodPointer || (candidate->flags & METHOD_ATTRIBUTE_STATIC) || candidate->is_generic
        || candidate->is_inflated || candidate->has_full_generic_sharing_signature)
      continue;
    if (index == 11 ? !CompatibleParameter(candidate->return_type, 'p')
                    : il2cpp_type_get_type(candidate->return_type) != IL2CPP_TYPE_VOID)
      continue;
    bool compatible = true;
    for (int p = 0; p < target.arguments; ++p)
      compatible = compatible && CompatibleParameter(candidate->parameters[p], target.abi[p]);
    if (!compatible)
      continue;
    if (result)
      return nullptr;
    result = candidate;
  }
  return result;
}
const MethodInfo* UniqueMethod(size_t index)
{
  auto* result = Method(index);
  if (!result)
    return nullptr;
  for (auto* other : methods)
    if (other && other->methodPointer == result->methodPointer)
      return nullptr;
  for (auto pointer : installedPointers)
    if (pointer == result->methodPointer)
      return nullptr;
  installedPointers[index] = result->methodPointer;
  return result;
}
} // namespace selection_trace

Json SelectionTraceStatus()
{
  return {{"hooks", selection_trace::installed},
          {"expected_hooks", std::size(selection_trace::targets)},
          {"healthy", selection_trace::journal && selection_trace::healthy.load()
                          && selection_trace::installed == std::size(selection_trace::targets)},
          {"shared_cloud_load_seen", selection_trace::sharedCloudLoadSeen.load()},
          {"journal", "bulk_selection.jsonl"}};
}

void TraceSelectionTransport(const Json& event) noexcept
{
  try {
    auto found = event.find("correlation");
    if (found != event.end() && found->is_object()
        && found->value("purpose", std::string{}).starts_with("persistent-preferences-")) {
      auto copy     = event;
      copy["event"] = "transport";
      copy["phase"] = event.value("event", std::string{});
      selection_trace::WriteEvent(std::move(copy));
    }
  } catch (...) {
  }
}

void InstallSelectionTrace()
{
  using namespace selection_trace;
  try {
    auto path      = ScienceTracePath("claims", "bulk_selection.jsonl");
    checkpointPath = path.parent_path() / "bulk_selection_baseline.json";
    journal        = spdlog::rotating_logger_mt("bulk-selection-science", path.string(), 4 * 1024 * 1024, 4, false);
    journal->set_pattern("%v");
    journal->set_level(spdlog::level::info);
    journal->flush_on(spdlog::level::info);
    journal->set_error_handler([](const std::string&) { healthy.store(false); });
    if (std::filesystem::exists(checkpointPath)) {
      if (std::filesystem::file_size(checkpointPath) > 2 * 1024 * 1024)
        throw std::runtime_error("selection checkpoint exceeds its size limit");
      std::ifstream input(checkpointPath, std::ios::binary);
      Json          saved;
      input >> saved;
      if (saved.value("schema", 0) != 1 || !saved.contains("values") || !saved["values"].is_object())
        throw std::runtime_error("unsupported selection checkpoint");
      for (auto key : {"StoreSelection", "RefineSelection", "FactionSelection"}) {
        if (!saved["values"].contains(key))
          continue;
        const auto& entry   = saved["values"][key];
        const auto  session = entry.value("session", std::string{});
        bool        valid   = !session.empty() && session.size() <= 64;
        for (auto c : session)
          valid = valid && ((c >= '0' && c <= '9') || c == '_');
        const auto basis = entry.value("basis", std::string{});
        if (!valid || (basis != "write" && basis != "initial_read") || !entry.contains("values")
            || !ValidList(entry["values"]) || !entry.contains("utc_ms") || !entry["utc_ms"].is_number_integer())
          throw std::runtime_error("invalid selection checkpoint entry");
        checkpoint[key] = {
            {"basis", basis}, {"session", session}, {"utc_ms", entry["utc_ms"]}, {"values", entry["values"]}};
      }
    }
  } catch (const std::exception& error) {
    spdlog::warn("[BulkSelectionTrace] journal unavailable: {}", error.what());
    return;
  }
#define SELECTION_HOOK(index, hook)                                                                                    \
  if (auto* method = UniqueMethod(index); method && SPUD_STATIC_DETOUR(method->methodPointer, selection_trace::hook))  \
    ++installed;                                                                                                       \
  else                                                                                                                 \
    spdlog::warn("[BulkSelectionTrace] hook unavailable: {}.{}", targets[index].cls, targets[index].method)
  SELECTION_HOOK(0, Edit);
  SELECTION_HOOK(1, Override);
  SELECTION_HOOK(2, Parse);
  SELECTION_HOOK(3, Apply);
  SELECTION_HOOK(4, Discard);
  SELECTION_HOOK(5, Reset);
  SELECTION_HOOK(6, Save);
  SELECTION_HOOK(7, Purge);
  SELECTION_HOOK(8, Load);
  SELECTION_HOOK(9, Claim);
  SELECTION_HOOK(10, Set);
  SELECTION_HOOK(11, Get);
  SELECTION_HOOK(12, CloudSave);
#undef SELECTION_HOOK
  WriteEvent(
      {{"event", "session"},
       {"source", STFC_IDENTITY_COMMENT_STR},
       {"hooks", installed},
       {"expected_hooks", std::size(targets)},
       {"state_format", "per-call selection deltas; unchanged dictionaries omitted"},
       {"healthy", healthy.load() && installed == std::size(targets)},
       {"limits", "4 MiB plus 4 rotations; 2048 rows per snapshot/list; 4096 dictionary slots; unknown is explicit"},
       {"cloud", "attempt/transport observation is not durable cloud persistence; compare a later native read"}});
  spdlog::info("[BulkSelectionTrace] passive journal: {}/{} hooks; science/claims/bulk_selection.jsonl", installed,
               std::size(targets));
}
