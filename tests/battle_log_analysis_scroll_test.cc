#include "fixtures/battle_log_analysis_scroll_cases.h"
#include "patches/parts/battle_log_analysis_scroll.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
  using namespace battle_analysis;
  for (const auto& fixture : fixtures::scroll_cases) {
    const auto layout = scroll_layout(fixture.count, fixture.row, fixture.available, fixture.gutter);
    assert(layout.cues == fixture.cues && layout.visible == fixture.expected_visible);
    const auto first = scroll_window(fixture.count, fixture.row, layout.visible, 0);
    const auto last  = scroll_window(fixture.count, fixture.row, layout.visible, 1e9f);
    assert(!first.before && !last.after);
    assert(first.after == fixture.cues && last.before == fixture.cues);
    assert(scroll_step(fixture.count, fixture.row, layout.visible, 0, false) == 0);
    assert(scroll_step(fixture.count, fixture.row, layout.visible, last.offset, true) == last.offset);
    for (float requested : {-100.f, 0.f, 30.f, 400.f, 1234.5f, 1e6f, 2e7f}) {
      const auto window = scroll_window(fixture.count, fixture.row, layout.visible, requested);
      const auto budget = static_cast<std::size_t>(std::ceil(layout.visible / fixture.row)) + 3;
      assert(window.begin <= window.end && window.end <= fixture.count && window.end - window.begin <= budget);
      assert(window.offset >= 0 && window.offset <= window.maximum);
      const auto before = scroll_step(fixture.count, fixture.row, layout.visible, window.offset, false);
      const auto after  = scroll_step(fixture.count, fixture.row, layout.visible, window.offset, true);
      assert(before <= window.offset && after >= window.offset && before >= 0 && after <= window.maximum);
    }
    if (fixture.count) {
      const auto revealed = scroll_reveal(fixture.count, fixture.row, layout.visible, 0, fixture.count - 1);
      assert(revealed == last.offset);
    }
    std::cout << "PASS: " << fixture.name << '\n';
  }
  // Selection and bookmarks keep visible content stable instead of recentering.
  assert(scroll_reveal(20, 113.25f, 850, 400, 6) == 400);
  assert(scroll_reveal(20, 113.25f, 850, 0, 7) == 56);
  assert(scroll_step(20, 113.25f, 850, 400, false) == 339.75f);
  assert(scroll_step(20, 113.25f, 850, 400, true) == 453);
  assert(scroll_reveal(20, 113.25f, 850, 400, 100) == 400);
  // Resizing a long region until it fits clears both edge cues and the offset.
  const auto resized = scroll_window(20, 113.25f, 3000, 1000);
  assert(resized.offset == 0 && !resized.before && !resized.after);
  const auto invalid = scroll_window(100, 0, 432, 0);
  assert(invalid.begin == 0 && invalid.end == 0);
  const auto nan = scroll_window(100, 54, 432, std::numeric_limits<float>::quiet_NaN());
  assert(nan.offset == 0 && nan.begin == 0);
  assert(scroll_reveal(20, std::numeric_limits<float>::quiet_NaN(), 432, 0, 2) == 0);
  assert(scroll_layout(100, 54, 0, 20).visible == 0);
  std::cout << "PASS: selection stability, arrow steps, resize and invalid inputs\n";
}
