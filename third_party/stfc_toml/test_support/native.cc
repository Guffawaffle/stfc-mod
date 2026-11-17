#include "stfc_toml/c_api.h"
#include "stfc_toml/editor.h"
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>

namespace
{
using Json = nlohmann::json;
using namespace stfc::toml_edit;
Path RequestPath(const Json& request, const char* name)
{
  const auto found = request.find(name);
  if (found == request.end() || !found->is_array())
    throw Failure("InvalidPath");
  Path result;
  for (const auto& part : *found) {
    if (!part.is_string())
      throw Failure("InvalidPath");
    result.push_back(part.get<std::string>());
  }
  CheckPath(result);
  return result;
}
std::string RequestString(const Json& request, const char* name, const char* error)
{
  const auto found = request.find(name);
  if (found == request.end() || !found->is_string())
    throw Failure(error);
  return found->get<std::string>();
}
Json Run(const Json& request)
{
  if (!request.is_object())
    throw Failure("InvalidPath");
  const auto operation = RequestString(request, "operation", "InvalidPath");
  if (operation == "normalize_value")
    return {{"ok", true}, {"value", NormalizeValue(RequestString(request, "value", "InvalidValue"))}};
  if (operation == "decode_string") {
    const auto  parsed = ValueDocument(RequestString(request, "value", "InvalidValue"));
    const auto* value  = parsed.get("__stfc_value__")->as_string();
    if (!value)
      throw Failure("InvalidValue");
    return {{"ok", true}, {"value", value->get()}};
  }
  if (operation == "parse_path") {
    Path path;
    if (request.contains("path"))
      path = RequestPath(request, "path");
    else {
      const auto expression = RequestString(request, "value", "InvalidPath");
      try {
        const detail::Document document(expression + " = true\n");
        if (!document.headers.empty() || document.assignments.size() != 1
            || document.assignments[0].key_end != expression.size() + 1
            || document.assignments[0].value_begin != expression.size() + 3
            || document.assignments[0].value_end != expression.size() + 7)
          throw Failure("InvalidPath");
        path = document.assignments[0].path;
      } catch (const Failure&) {
        throw Failure("InvalidPath");
      }
    }
    CheckPath(path);
    return {{"ok", true}, {"path", path}, {"value", CanonicalPath(path)}};
  }
  const auto text = RequestString(request, "text", "InvalidDocument");
  if (operation == "validate") {
    Validate(text);
    return {{"ok", true}};
  }
  if (operation == "read") {
    const auto snapshot  = Read(text);
    Json       overrides = Json::array(), tables = Json::array();
    for (const auto& row : snapshot.overrides)
      overrides.push_back({{"path", row.path},
                           {"canonicalPath", CanonicalPath(row.path)},
                           {"value", row.value},
                           {"semanticValue", row.semantic_value},
                           {"line", row.line}});
    for (const auto& row : snapshot.tables)
      tables.push_back({{"path", row.path}, {"canonicalPath", CanonicalPath(row.path)}, {"line", row.line}});
    return {{"ok", true}, {"overrides", overrides}, {"tables", tables}};
  }
  const auto path        = RequestPath(request, "path");
  const auto value       = operation == "set" ? RequestString(request, "value", "InvalidValue") : std::string{};
  const auto destination = operation == "rename_table" ? RequestPath(request, "destination") : Path{};
  return {{"ok", true}, {"text", Prepare(text, operation, path, value, destination)}};
}
} // namespace
extern "C" uint32_t STFC_TOML_CALL stfc_toml_abi_version(void)
{ return 1; }
extern "C" int STFC_TOML_CALL stfc_toml_execute(const char* request, size_t request_length, char** response_out,
                                                size_t* response_length_out)
{
  if (!response_out || !response_length_out)
    return 1;
  *response_out        = nullptr;
  *response_length_out = 0;
  if (!request || request_length > 64 * 1024 * 1024)
    return 1;
  try {
    Json response;
    try {
      const std::string_view bytes(request, request_length);
      ValidateUtf8(bytes);
      response = Run(Json::parse(bytes));
    } catch (const Failure& error) {
      response = {{"ok", false}, {"error", {{"code", error.what()}}}};
      if (error.line && *error.line > 0)
        response["error"]["line"] = *error.line;
    } catch (const Json::exception&) {
      response = {{"ok", false}, {"error", {{"code", "InvalidPath"}}}};
    } catch (...) {
      response = {{"ok", false}, {"error", {{"code", "InternalError"}}}};
    }
    const auto bytes  = response.dump();
    auto*      buffer = static_cast<char*>(std::malloc(bytes.size() + 1));
    if (!buffer)
      return 2;
    std::memcpy(buffer, bytes.data(), bytes.size());
    buffer[bytes.size()] = '\0';
    *response_out        = buffer;
    *response_length_out = bytes.size();
    return 0;
  } catch (...) {
    return 2;
  }
}
extern "C" void STFC_TOML_CALL stfc_toml_free(void* response)
{ std::free(response); }
