#include "stfc_toml/c_api.h"
#include "stfc_toml/editor.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <source_location>
#include <stdexcept>

using namespace stfc::toml_edit;
using Json = nlohmann::json;
void Check(bool condition, std::source_location at = std::source_location::current())
{
  if (!condition)
    throw std::runtime_error("fixture failed at line " + std::to_string(at.line()));
}
template <class Function> void Fails(const char* code, Function function)
{
  try {
    function();
  } catch (const Failure& error) {
    Check(std::string(error.what()) == code);
    return;
  }
  throw std::runtime_error("expected refusal");
}
Json Execute(const Json& request)
{
  const auto  bytes    = request.dump();
  char*       response = nullptr;
  std::size_t length   = 0;
  Check(stfc_toml_execute(bytes.data(), bytes.size(), &response, &length) == 0);
  const auto parsed = Json::parse(response, response + length);
  stfc_toml_free(response);
  return parsed;
}
int main()
{
  try {
    Check(stfc_toml_abi_version() == 1);
    // A long unrelated array must not make source coordinate lookup quadratic.
    std::string long_array = "unknown=[";
    for (int i = 0; i < 65536; ++i)
      long_array += i ? ",1" : "1";
    long_array += "]\n[ui]\nenabled=false # preserved\n";
    const auto started    = std::chrono::steady_clock::now();
    auto       array_edit = Prepare(long_array, "set", {"ui", "enabled"}, "true");
    Check(std::chrono::steady_clock::now() - started < std::chrono::seconds(2));
    auto expected_array = long_array;
    expected_array.replace(expected_array.find("false"), 5, "true");
    Check(array_edit == expected_array);
    const std::string unusual =
        "\xef\xbb\xbf# preserve\r\n[\"ui.é\"] # keep\r\n\"é.mode\" = '''none\nmode''' # tail\r\n"
        "[elsewhere]\r\nx=nan\r\n[[unknown.items]]\r\nname='one'\r\n[[unknown.items]]\r\nname='two'\r\n";
    auto expected = unusual;
    expected.replace(expected.find("'''none\nmode'''"), 15, "\"warp\"");
    Check(Prepare(unusual, "set", {"ui.é", "é.mode"}, "'warp'") == expected);
    const std::string emoji = "\xf0\x9f\x9a\x80";
    const Path        emoji_path{"ui." + emoji, "e\xcc\x81." + emoji};
    const Path        transport_path{"future", emoji + ".key", "e\xcc\x81.\n tab\t"};
    const auto        rendered_transport = Execute({{"operation", "parse_path"}, {"path", transport_path}});
    const auto reparsed_transport = Execute({{"operation", "parse_path"}, {"value", rendered_transport.at("value")}});
    Check(reparsed_transport.at("ok") == true && reparsed_transport.at("path") == Json(transport_path));
    const auto spaced_transport = Execute(
        {{"operation", "parse_path"}, {"value", "  " + rendered_transport.at("value").get<std::string>() + "  "}});
    Check(spaced_transport.at("ok") == true && spaced_transport.at("path") == Json(transport_path));
    const auto decoded_string = Execute({{"operation", "decode_string"}, {"value", "\"\\U0001F680 café\\nline\""}});
    Check(decoded_string.at("ok") == true && decoded_string.at("value") == emoji + " café\nline");
    Check(Execute({{"operation", "decode_string"}, {"value", "true"}}).at("error").at("code") == "InvalidValue");
    Check(Execute({{"operation", "decode_string"}, {"value", "'x'\nsecret=2"}}).at("error").at("code")
          == "InvalidValue");
    const auto emoji_canonical = CanonicalPath(emoji_path);
    Check(Execute({{"operation", "parse_path"}, {"value", emoji_canonical}}).at("path") == Json(emoji_path));
    const auto emoji_document =
        "[" + EncodeKey(emoji_path[0]) + "]\r\n" + EncodeKey(emoji_path[1]) + "='none' # keep\r\n";
    auto emoji_expected = emoji_document;
    emoji_expected.replace(emoji_expected.find("'none'"), 6, "\"warp\"");
    Check(Prepare(emoji_document, "set", emoji_path, "'warp'") == emoji_expected);
    const std::string nested_aot = "[[unknown.items]]\nname='one'\n[unknown.items.detail]\nx=1\n[ui]\nenabled=false\n";
    Check(Prepare(nested_aot, "set", {"ui", "enabled"}, "true")
          == nested_aot.substr(0, nested_aot.find("false")) + "true\n");
    const std::string comment_bom = "\xef\xbb\xbf# keep\r\n";
    const auto        created_bom = Prepare(comment_bom, "set", {"ui", "enabled"}, "true");
    Check(created_bom.starts_with("\xef\xbb\xbf") && created_bom.find("# keep\r\n") != std::string::npos);
    Check(Prepare(comment_bom, "remove", {"missing"}) == comment_bom);
    const auto snapshot = Read(unusual);
    Check(snapshot.overrides[0].path == Path({"elsewhere", "x"}));
    bool found = false;
    for (const auto& row : snapshot.overrides)
      if (row.path == Path({"ui.é", "é.mode"})) {
        Check(row.value == "'''none\nmode'''" && row.semantic_value == "\"none\\nmode\"");
        found = true;
      }
    Check(found);
    for (const std::string text :
         {"", "# comment", "[ui]", "ui = {} # inline\n", "ui.other = 9\n", "[ui.child]\nx=9\n"}) {
      const auto result = Prepare(text, "set", {"ui", "enabled"}, "true");
      Check(toml::parse(result)["ui"]["enabled"].value<bool>() == true);
    }
    Check(Prepare("ui={theme.color='blue'}\n", "set", {"ui", "theme", "font"}, "'mono'")
          == "ui={theme.color='blue', theme.font = \"mono\" }\n");
    Check(Prepare("ui={theme={color='blue'}}\n", "set", {"ui", "theme", "font"}, "'mono'")
          == "ui={theme={color='blue', font = \"mono\" }}\n");
    Check(Prepare("ui.enabled=true # remove\nother=2\n", "remove", {"ui", "enabled"}) == "other=2\n");
    Check(Prepare("[ui]\nenabled=true # remove\n[other]\nx=2\n", "remove", {"ui", "enabled"})
          == "[ui]\n[other]\nx=2\n");
    Check(Prepare("ui={a=1,b=2,c=3}\n", "remove", {"ui", "a"}) == "ui={b=2,c=3}\n");
    Check(Prepare("ui={a=1,b=2,c=3}\n", "remove", {"ui", "b"}) == "ui={a=1,c=3}\n");
    Check(Prepare("ui={a=1,b=2,c=3}\n", "remove", {"ui", "c"}) == "ui={a=1,b=2}\n");
    Check(Prepare("ui={theme.a=1,theme.b=2,other=3}\n", "remove_table", {"ui", "theme"}) == "ui={other=3}\n");
    Check(Prepare("ui={theme={a=1},other=3}\n", "remove_table", {"ui", "theme"}) == "ui={other=3}\n");
    const std::string scattered = "# keep\n[sync.targets.old] # header\na=1 # body\n[other]\nx=2 # unrelated\n"
                                  "[sync.targets.old.child]\nb='x'\n[sync.targets.kept]\na=3\n";
    Check(Prepare(scattered, "remove_table", {"sync", "targets", "old"})
          == "# keep\n[other]\nx=2 # unrelated\n[sync.targets.kept]\na=3\n");
    const auto renamed = Prepare(scattered, "rename_table", {"sync", "targets", "old"}, {}, {"sync", "targets", "new"});
    Check(renamed
          == "# keep\n[sync.targets.new] # header\na=1 # body\n[other]\nx=2 # unrelated\n"
             "[sync.targets.new.child]\nb='x'\n[sync.targets.kept]\na=3\n");
    Check(Prepare("sync.targets.old.a=1\nsync.targets.old.b=2\nother=3\n", "rename_table", {"sync", "targets", "old"},
                  {}, {"sync", "targets", "a.b"})
          == "sync.targets.\"a.b\".a=1\nsync.targets.\"a.b\".b=2\nother=3\n");
    Check(Prepare("sync={targets={old={a=1},kept={a=2}}}\n", "rename_table", {"sync", "targets", "old"}, {},
                  {"sync", "targets", "new"})
          == "sync={targets={new={a=1},kept={a=2}}}\n");
    const auto cross =
        Prepare("[left]\nold.x=1 # selected\nkept=2\n", "rename_table", {"left", "old"}, {}, {"right", "new"});
    Check(cross.find("kept=2\n") != std::string::npos);
    Check(toml::parse(cross)["right"]["new"]["x"].value<int>() == 1);
    Fails("DuplicateTarget", [] { Validate("x=1\nx=2\n"); });
    Fails("InvalidDocument", [] { Validate("x=[\n"); });
    Fails("InvalidUtf8", [] { Validate(std::string("x='\xff'", 5)); });
    Fails("InvalidValue", [] { Prepare("", "set", {"x"}, "1\ny=2"); });
    Fails("UnsupportedTarget", [&] { Prepare(unusual, "set", {"unknown", "items", "name"}, "'x'"); });
    Fails("DuplicateTarget", [] { Prepare("[a]\nx=1\n[b]\nx=2\n", "rename_table", {"a"}, {}, {"b"}); });
    Check(NormalizeValue("'''hello\nworld'''") == "\"hello\\nworld\"");
    Check(NormalizeValue("0x7fff_ffff_ffff_ffff") == "9223372036854775807");
    Check(NormalizeValue("1979-05-27T07:32:00Z") == "1979-05-27T07:32:00Z");
    Check(toml::parse(Prepare("a=[[1],[2]]\n", "set", {"a"}, "[[3]]"))["a"].is_array());
    const auto collection =
        Prepare("[ui]\nkeys=['one',\n # in array\n 'two'] # tail\n", "set", {"ui", "keys"}, "['three']");
    Check(collection == "[ui]\nkeys=[ 'three' ] # tail\n" || collection == "[ui]\nkeys=[ \"three\" ] # tail\n");
    const auto path = Execute({{"operation", "parse_path"}, {"value", "sync.targets.\"a.b\""}});
    Check(path.at("ok") == true && path.at("path") == Json::array({"sync", "targets", "a.b"})
          && path.at("value") == "sync.targets.\"a.b\"");
    Check(Execute({{"operation", "parse_path"}, {"value", "x = true # injected"}}).at("error").at("code")
          == "InvalidPath");
    Check(Execute({{"operation", "parse_path"}, {"value", "x\ny"}}).at("ok") == false);
    Check(Execute({{"operation", "parse_path"}, {"path", Json::array({"sync", "", "a.b"})}}).at("value")
          == "sync.\"\".\"a.b\"");
    const auto read = Execute({{"operation", "read"}, {"text", "[\"a.b\"]\nx='v'\n"}});
    Check(read.at("overrides")[0].at("canonicalPath") == "\"a.b\".x");
    const auto bad =
        Execute({{"operation", "set"}, {"text", "x=1"}, {"path", Json::array({"x"})}, {"value", "1\nSECRET=2"}});
    Check(bad.at("error").at("code") == "InvalidValue" && bad.dump().find("SECRET") == std::string::npos
          && !bad.contains("text"));
    char*       response = reinterpret_cast<char*>(1);
    std::size_t length   = 999;
    Check(stfc_toml_execute(nullptr, 0, &response, &length) != 0 && response == nullptr && length == 0);
    std::cout << "Shared TOML core and ABI source/semantic fixtures passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
