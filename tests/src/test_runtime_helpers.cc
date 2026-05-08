#include "test_pure_common.h"

// ===========================================================================
// runtime_helpers
// ===========================================================================

TEST_SUITE("str_utils")
{
  TEST_CASE("StripLeadingAsciiWhitespace")
  {
    CHECK(StripLeadingAsciiWhitespace("  hello") == "hello");
    CHECK(StripLeadingAsciiWhitespace("\thello") == "hello");
    CHECK(StripLeadingAsciiWhitespace("hello") == "hello");
    CHECK(StripLeadingAsciiWhitespace("") == "");
    CHECK(StripLeadingAsciiWhitespace("   ") == "");
  }

  TEST_CASE("StripTrailingAsciiWhitespace")
  {
    CHECK(StripTrailingAsciiWhitespace("hello  ") == "hello");
    CHECK(StripTrailingAsciiWhitespace("hello\t") == "hello");
    CHECK(StripTrailingAsciiWhitespace("hello") == "hello");
    CHECK(StripTrailingAsciiWhitespace("") == "");
    CHECK(StripTrailingAsciiWhitespace("   ") == "");
  }

  TEST_CASE("StripAsciiWhitespace")
  {
    CHECK(StripAsciiWhitespace("  hello  ") == "hello");
    CHECK(StripAsciiWhitespace("  hello world  ") == "hello world");
    CHECK(StripAsciiWhitespace("hello") == "hello");
    CHECK(StripAsciiWhitespace("") == "");
  }

  TEST_CASE("AsciiStrToUpper")
  {
    CHECK(AsciiStrToUpper("hello") == "HELLO");
    CHECK(AsciiStrToUpper("Hello World") == "HELLO WORLD");
    CHECK(AsciiStrToUpper("ALREADY") == "ALREADY");
    CHECK(AsciiStrToUpper("123abc") == "123ABC");
    CHECK(AsciiStrToUpper("") == "");
  }

  TEST_CASE("StrSplit")
  {
    SUBCASE("basic split")
    {
      auto result = StrSplit("a,b,c", ',');
      REQUIRE(result.size() == 3);
      CHECK(result[0] == "a");
      CHECK(result[1] == "b");
      CHECK(result[2] == "c");
    }

    SUBCASE("strips trailing whitespace on last element")
    {
      auto result = StrSplit("a,b,c  ", ',');
      REQUIRE(result.size() == 3);
      CHECK(result[2] == "c");
    }

    SUBCASE("empty segments skipped")
    {
      auto result = StrSplit("a,,b", ',');
      REQUIRE(result.size() == 2);
      CHECK(result[0] == "a");
      CHECK(result[1] == "b");
    }

    SUBCASE("single element")
    {
      auto result = StrSplit("hello", ',');
      REQUIRE(result.size() == 1);
      CHECK(result[0] == "hello");
    }

    SUBCASE("empty string")
    {
      auto result = StrSplit("", ',');
      CHECK(result.empty());
    }

    SUBCASE("pipe delimiter")
    {
      auto result = StrSplit("SPACE|MOUSE1", '|');
      REQUIRE(result.size() == 2);
      CHECK(result[0] == "SPACE");
      CHECK(result[1] == "MOUSE1");
    }
  }
}

// ===========================================================================
// hotkey decisions
// ===========================================================================


TEST_SUITE("async_work_queue")
{
  TEST_CASE("tracks enqueue drain and depth diagnostics")
  {
    AsyncWorkQueue<int> queue;

    CHECK(queue.enqueue(10));
    CHECK(queue.enqueue(20));

    auto diagnostics = queue.diagnostics();
    CHECK(diagnostics.depth == 2);
    CHECK(diagnostics.enqueued == 2);
    CHECK(diagnostics.dequeued == 0);

    auto batch = queue.drain();
    REQUIRE(batch.size() == 2);
    CHECK(batch[0] == 10);
    CHECK(batch[1] == 20);

    diagnostics = queue.diagnostics();
    CHECK(diagnostics.depth == 0);
    CHECK(diagnostics.dequeued == 2);
  }

  TEST_CASE("rejects new work after shutdown while preserving queued work")
  {
    AsyncWorkQueue<std::string> queue;

    CHECK(queue.enqueue("before"));
    queue.request_shutdown();

    CHECK_FALSE(queue.enqueue("after"));
    CHECK(queue.shutdown_requested());

    auto batch = queue.drain();
    REQUIRE(batch.size() == 1);
    CHECK(batch[0] == "before");
  }

  TEST_CASE("try_pop updates dequeue diagnostics")
  {
    AsyncWorkQueue<int> queue;
    int                 value = 0;

    CHECK_FALSE(queue.try_pop(value));
    CHECK(queue.enqueue(42));
    CHECK(queue.try_pop(value));
    CHECK(value == 42);

    auto diagnostics = queue.diagnostics();
    CHECK(diagnostics.depth == 0);
    CHECK(diagnostics.dequeued == 1);
  }

  TEST_CASE("tracks worker activity and errors")
  {
    AsyncWorkQueue<int> queue;

    queue.set_worker_active(true);
    queue.record_worker_error();
    queue.record_worker_error();

    auto diagnostics = queue.diagnostics();
    CHECK(diagnostics.worker_active);
    CHECK(diagnostics.worker_errors == 2);

    queue.set_worker_active(false);
    CHECK_FALSE(queue.diagnostics().worker_active);
  }
}


TEST_SUITE("object_tracker_core")
{
  TEST_CASE("tracks objects by class with oldest-to-newest ordering")
  {
    ObjectTrackerCore<int, int> tracker;

    tracker.add(1, 100);
    tracker.add(1, 200);
    tracker.add(2, 300);

    CHECK(tracker.class_count() == 2);
    CHECK(tracker.object_count() == 3);
    CHECK(tracker.latest_for_class(1) == 200);
    CHECK(tracker.latest_for_class(3) == 0);

    auto objects = tracker.objects_for_class(1);
    REQUIRE(objects.size() == 2);
    CHECK(objects[0] == 100);
    CHECK(objects[1] == 200);
  }

  TEST_CASE("removes a single object from a class bucket")
  {
    ObjectTrackerCore<int, int> tracker;
    tracker.add(1, 100);
    tracker.add(1, 200);

    CHECK(tracker.remove(1, 100));
    CHECK_FALSE(tracker.remove(1, 999));

    auto objects = tracker.objects_for_class(1);
    REQUIRE(objects.size() == 1);
    CHECK(objects[0] == 200);
    CHECK(tracker.latest_for_class(1) == 200);
  }

  TEST_CASE("removes an object from every class bucket")
  {
    ObjectTrackerCore<int, int> tracker;
    tracker.add(1, 100);
    tracker.add(2, 100);
    tracker.add(2, 200);

    CHECK(tracker.remove_object_from_all(100) == 2);
    CHECK(tracker.objects_for_class(1).empty());

    auto remaining = tracker.objects_for_class(2);
    REQUIRE(remaining.size() == 1);
    CHECK(remaining[0] == 200);
  }
}

// ===========================================================================
// incoming attack policy
// ===========================================================================


TEST_SUITE("strip_unity_rich_text")
{
  TEST_CASE("removes color tags")
  { CHECK(strip_unity_rich_text("<color=#FF0000>Red Text</color>") == "Red Text"); }

  TEST_CASE("removes bold/italic tags")
  { CHECK(strip_unity_rich_text("<b>Bold</b> and <i>Italic</i>") == "Bold and Italic"); }

  TEST_CASE("removes size tags")
  { CHECK(strip_unity_rich_text("<size=20>Big</size>") == "Big"); }

  TEST_CASE("handles nested tags")
  { CHECK(strip_unity_rich_text("<color=#FFF><b>Hello</b></color>") == "Hello"); }

  TEST_CASE("preserves plain text")
  { CHECK(strip_unity_rich_text("Hello World") == "Hello World"); }

  TEST_CASE("empty string")
  { CHECK(strip_unity_rich_text("") == ""); }

  TEST_CASE("unclosed angle bracket kept")
  { CHECK(strip_unity_rich_text("5 < 10 but no closing") == "5 < 10 but no closing"); }
}

// ===========================================================================
// parse_hull_key
// ===========================================================================
