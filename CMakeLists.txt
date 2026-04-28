cmake_minimum_required(VERSION 3.24)

project(stfc-community-patch-pure-validation LANGUAGES CXX)

# Linux-native validation for pure logic only. Release/platform mod builds
# remain owned by xmake.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(doctest CONFIG QUIET)
if(NOT doctest_FOUND)
  FetchContent_Declare(
    doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
  )
  FetchContent_MakeAvailable(doctest)
endif()

set(STFC_MOD_PURE_SOURCES
  mods/src/testable_functions.cc
  mods/src/patches/live_debug_event_store.cc
  mods/src/patches/live_debug_recent_event_requests.cc
  mods/src/patches/live_debug_fleet_serializers.cc
  mods/src/patches/live_debug_ui_serializers.cc
  mods/src/patches/live_debug_viewer_serializers.cc
  mods/src/patches/battle_log_decoder.cc
  mods/src/patches/notification_queue.cc
  mods/src/patches/notification_text.cc
)

add_executable(stfc-mod-tests
  tests/src/test_pure_logic.cc
  ${STFC_MOD_PURE_SOURCES}
)
target_include_directories(stfc-mod-tests PRIVATE mods/src)
target_compile_definitions(stfc-mod-tests PRIVATE NOMINMAX)
target_link_libraries(stfc-mod-tests PRIVATE doctest::doctest nlohmann_json::nlohmann_json)

add_executable(battle-log-decode
  tests/tools/battle_log_decode_tool.cc
  mods/src/patches/battle_log_decoder.cc
)
target_include_directories(battle-log-decode PRIVATE mods/src)
target_compile_definitions(battle-log-decode PRIVATE NOMINMAX)
target_link_libraries(battle-log-decode PRIVATE nlohmann_json::nlohmann_json)

enable_testing()
add_test(NAME stfc-mod-tests COMMAND stfc-mod-tests)
