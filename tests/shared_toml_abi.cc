// Draft external C-ABI client for M1b. No core/parser/native object is linked.
// Intended location after root integration/review: tests/shared_toml_abi.cc.
#include "stfc_toml/c_api.h"
#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#else
#error M1b external shared TOML client is scoped to Windows and macOS
#endif

namespace
{
using Json = nlohmann::json;
void Check(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}
class Library
{
public:
  explicit Library(const char* path)
  {
#if defined(_WIN32)
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
    Check(length > 0, "invalid UTF-8 library path");
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    Check(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide.data(), length) == length,
          "invalid UTF-8 library path");
    const auto absolute_length = GetFullPathNameW(wide.c_str(), 0, nullptr, nullptr);
    Check(absolute_length > 0, "cannot resolve selected library path");
    std::wstring absolute(static_cast<std::size_t>(absolute_length), L'\0');
    const auto   written = GetFullPathNameW(wide.c_str(), absolute_length, absolute.data(), nullptr);
    Check(written > 0 && written < absolute_length, "cannot resolve selected library path");
    handle_ =
        LoadLibraryExW(absolute.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
    handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    Check(handle_ != nullptr, "cannot load selected standalone TOML library");
  }
  ~Library()
  {
#if defined(_WIN32)
    if (handle_)
      FreeLibrary(handle_);
#else
    if (handle_)
      dlclose(handle_);
#endif
  }
  Library(const Library&)                                         = delete;
  Library&                              operator=(const Library&) = delete;
  template <typename Function> Function Find(const char* name) const
  {
#if defined(_WIN32)
    auto symbol = GetProcAddress(handle_, name);
#else
    auto symbol = dlsym(handle_, name);
#endif
    Check(symbol != nullptr, "missing required shared TOML C export");
    return reinterpret_cast<Function>(symbol);
  }

private:
#if defined(_WIN32)
  HMODULE handle_{};
#else
  void* handle_{};
#endif
};
struct Client {
  decltype(&stfc_toml_abi_version) version;
  decltype(&stfc_toml_execute)     execute;
  decltype(&stfc_toml_free)        release;
  Json                             Raw(std::string_view bytes) const
  {
    char*       response = nullptr;
    std::size_t length   = 0;
    Check(execute(bytes.data(), bytes.size(), &response, &length) == 0 && response,
          "native response was not allocated");
    // The response allocator belongs to the loaded module, not this client's
    // separately linked CRT. Free even when parsing/inspection throws.
    struct Owned {
      char*                     pointer;
      decltype(&stfc_toml_free) free;
      ~Owned()
      { free(pointer); }
    } owned{response, release};
    return Json::parse(response, response + length);
  }
  Json Run(const Json& request) const
  { return Raw(request.dump()); }
};
} // namespace

int main(int argc, char** argv)
{
  try {
    Check(argc == 2, "pass exactly one selected library path");
    Library      library(argv[1]);
    const Client api{library.Find<decltype(Client::version)>("stfc_toml_abi_version"),
                     library.Find<decltype(Client::execute)>("stfc_toml_execute"),
                     library.Find<decltype(Client::release)>("stfc_toml_free")};
    Check(api.version() == 1, "unsupported shared TOML ABI");
    api.release(nullptr);
    const auto valid = api.Run({{"operation", "validate"}, {"text", "# source\r\nx=0x1_f # keep\r\n"}});
    Check(valid.at("ok") == true, "validate operation failed");
    const auto read = api.Run({{"operation", "read"}, {"text", "# source\r\nx=0x1_f # keep\r\n"}});
    Check(read.at("ok") == true && read.at("overrides")[0].at("value") == "0x1_f"
              && read.at("overrides")[0].at("semanticValue") == "31",
          "read lost exact source/semantic distinction");
    const auto set =
        api.Run({{"operation", "set"}, {"text", "# keep\r\nx=false # tail\r\n"}, {"path", {"x"}}, {"value", "true"}});
    Check(set.at("ok") == true && set.at("text") == "# keep\r\nx=true # tail\r\n",
          "set changed unrelated source bytes");
    const auto removed = api.Run({{"operation", "remove"}, {"text", "x=1 # remove\ny=2\n"}, {"path", {"x"}}});
    Check(removed.at("ok") == true && removed.at("text") == "y=2\n", "remove operation failed");
    const auto table =
        api.Run({{"operation", "remove_table"}, {"text", "[old]\nx=1\n[keep]\ny=2\n"}, {"path", {"old"}}});
    Check(table.at("ok") == true && table.at("text") == "[keep]\ny=2\n", "remove_table operation failed");
    const auto renamed =
        api.Run({{"operation", "rename_table"}, {"text", "[old]\nx=1\n"}, {"path", {"old"}}, {"destination", {"new"}}});
    Check(renamed.at("ok") == true && renamed.at("text") == "[new]\nx=1\n", "rename_table operation failed");
    const auto normalized = api.Run({{"operation", "normalize_value"}, {"value", "0x7fff_ffff_ffff_ffff"}});
    Check(normalized.at("ok") == true && normalized.at("value") == "9223372036854775807",
          "normalize_value lost exact int64");
    const auto decoded = api.Run({{"operation", "decode_string"}, {"value", "'''caf\xc3\xa9\nline'''"}});
    Check(decoded.at("ok") == true && decoded.at("value") == "caf\xc3\xa9\nline",
          "decode_string failed UTF-8 decoding");
    const auto path = api.Run({{"operation", "parse_path"}, {"value", "sync.targets.\"a.b\""}});
    Check(path.at("ok") == true && path.at("path") == Json::array({"sync", "targets", "a.b"})
              && path.at("value") == "sync.targets.\"a.b\"",
          "parse_path flattened identity");
    const auto failed = api.Run({{"operation", "set"}, {"text", "x=1\n"}, {"path", {"x"}}, {"value", "1\nSECRET=2"}});
    Check(failed.at("ok") == false && !failed.contains("text") && failed.at("error").at("code") == "InvalidValue"
              && failed.dump().find("SECRET") == std::string::npos,
          "failure leaked or admitted candidate bytes");
    Check(api.Raw("{").at("ok") == false, "invalid JSON was accepted");
    Check(api.Raw(std::string(1, '\xff')).at("error").at("code") == "InvalidUtf8",
          "invalid UTF-8 classification changed");
    char*       response = reinterpret_cast<char*>(1);
    std::size_t length   = 999;
    Check(api.execute(nullptr, 0, &response, &length) != 0 && !response && length == 0,
          "invalid request did not clear outputs");
    response = reinterpret_cast<char*>(1);
    length   = 999;
    Check(api.execute("x", 64u * 1024 * 1024 + 1u, &response, &length) != 0 && !response && length == 0,
          "oversize request guard changed");
    for (int repeat = 0; repeat < 64; ++repeat)
      Check(api.Run({{"operation", "validate"}, {"text", "x=true\n"}}).at("ok") == true,
            "repeated native allocation/free failed");
    std::cout << "Shared TOML external ABI v1: three required symbols, nine operations, exact source and allocator "
                 "ownership passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
