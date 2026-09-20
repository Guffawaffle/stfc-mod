#include "patches/galaxy_selection_pins.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

using Edge = std::pair<int, std::string>;
struct Reader {
  std::map<Edge, int> fields, properties;
  std::map<int, std::vector<int>> lists;
  int failing_list = 0;
  bool Read(const std::map<Edge, int>& values, int object, const char* name, int& out)
  {
    const auto it = values.find({object, name});
    if (it == values.end()) return false;
    out = it->second;
    return true;
  }
  bool Field(int object, const char* name, int& out) { return Read(fields, object, name, out); }
  bool Property(int object, const char* name, int& out) { return Read(properties, object, name, out); }
  bool List(int list, std::vector<int>& pins)
  {
    if (!list) return true;
    if (list == failing_list || !lists.contains(list)) return false;
    for (int poi : lists.at(list)) if (poi) pins.push_back(poi);
    return true;
  }
};
void Check(bool ok, const char* label)
{
  if (!ok) { std::cerr << label << '\n'; std::exit(1); }
}
Reader Fixture()
{
  return {{{{1, "_navigationInteractionUIViewController"}, 2}, {{1, "_selectionPoiList"}, 10},
           {{2, "_objectViewerLoadAndShow"}, 3}, {{3, "_activePOI"}, 99},
           {{3, "_activeViewer"}, 4}, {{3, "_viewerToResetOnClose"}, 5},
           {{6, "_queryPoiList"}, 11}, {{7, "Poi"}, 99}, {{7, "PoiList"}, 12}},
          {{{2, "get_CanvasContext"}, 7}, {{3, "get_Context"}, 7},
           {{4, "get_CanvasContext"}, 7}, {{4, "get_Context"}, 7}, {{4, "get_Parent"}, 6},
           {{5, "get_CanvasContext"}, 7}, {{5, "get_Context"}, 7}, {{5, "get_Parent"}, 0},
           {{6, "get_CanvasContext"}, 7}},
          {{10, {99}}, {11, {99}}, {12, {99}}}};
}
int main()
{
  auto valid = Fixture();
  std::vector<int> pins;
  Check(galaxy_selection::CollectOwnerPins(1, pins, valid), "Complete owners must permit retirement checks");
  Check(std::find(pins.begin(), pins.end(), 99) != pins.end(), "Active preview POI must remain pinned");
  // Every field and getter on the owner graph is required when its owner exists.
  // A failed read must deny retirement even if other reads already collected pins.
  for (const auto& [edge, value] : valid.fields) {
    auto failed = valid;
    failed.fields.erase(edge);
    pins.clear();
    Check(!galaxy_selection::CollectOwnerPins(1, pins, failed), "Missing owner field authorized retirement");
  }
  for (const auto& [edge, value] : valid.properties) {
    auto failed = valid;
    failed.properties.erase(edge);
    pins.clear();
    Check(!galaxy_selection::CollectOwnerPins(1, pins, failed), "Failed context getter authorized retirement");
  }
  for (int list : {10, 11, 12}) {
    auto failed = valid;
    failed.failing_list = list;
    pins.clear();
    Check(!galaxy_selection::CollectOwnerPins(1, pins, failed), "Failed list item read authorized retirement");
  }
  // Real null values are different from failures: closed viewers and empty
  // selection/context lists are ordinary and must not permanently fill the pool.
  for (auto& [edge, value] : valid.fields)
    if (edge.second != "_navigationInteractionUIViewController" && edge.second != "_objectViewerLoadAndShow") value = 0;
  for (auto& [edge, value] : valid.properties) value = 0;
  pins.clear();
  Check(galaxy_selection::CollectOwnerPins(1, pins, valid) && pins.empty(), "Successful null owners must be accepted");
  std::cout << "Galaxy selection owner-read failure fixtures passed\n";
}
