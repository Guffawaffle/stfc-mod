#include "patches/parts/battle_log_analysis_scroll.h"

#include <cassert>
#include <iostream>
#include <limits>

int main()
{
  using battle_analysis::scroll_window;
  auto empty = scroll_window(0, 54, 432, 100);
  assert(empty.begin == 0 && empty.end == 0 && empty.offset == 0 && empty.extent == 432);
  auto short_list = scroll_window(3, 54, 432, 800);
  assert(short_list.begin == 0 && short_list.end == 3 && short_list.offset == 0);
  auto start = scroll_window(227, 54, 432, -100);
  assert(start.begin == 0 && start.end == 9 && start.offset == 0);
  auto middle = scroll_window(227, 54, 432, 540);
  assert(middle.begin == 9 && middle.end == 19 && middle.offset == 540);
  auto end = scroll_window(227, 54, 432, 1e9f);
  assert(end.end == 227 && end.end - end.begin <= 10 && end.offset == 227 * 54 - 432);
  for (float offset : {0.f, 1234.5f, 1e6f, 2e7f}) {
    auto large = scroll_window(200000, 54, 432, offset);
    assert(large.begin <= large.end && large.end <= 200000 && large.end - large.begin <= 11);
    assert(large.offset >= 0 && large.offset <= large.extent - 432);
  }
  auto invalid = scroll_window(100, 0, 432, 0);
  assert(invalid.begin == 0 && invalid.end == 0);
  auto nan = scroll_window(100, 54, 432, std::numeric_limits<float>::quiet_NaN());
  assert(nan.offset == 0 && nan.begin == 0);
  std::cout << "PASS: empty, short, start, middle, end, invalid and 200k-event scroll windows remain bounded\n";
}
