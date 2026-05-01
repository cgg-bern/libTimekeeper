cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

project(TimekeeperTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to find GTest first; fall back to FetchContent if not available.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GTest not found via find_package — downloading from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    # Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Link against nlohmann/json if it was fetched via FetchContent.
if(TARGET nlohmann_json::nlohmann_json)
    set(JSON_LIB nlohmann_json::nlohmann_json)
else()
    find_package(nlohmann_json QUIET)
    if(NOT nlohmann_json_FOUND)
        message(FATAL_ERROR "nlohmann/json not found. Either install it system-wide or enable TIMEKEEPER_FETCH_JSON.")
    endif()
    set(JSON_LIB nlohmann_json::nlohmann_json)
endif()

add_executable(timekeeper_tests
    test_duration.cc
    test_stopwatch.cc
    test_hierarchical.cc
    test_scopedstopwatch.cc
    test_printing.cc
    test_json.cc
)
target_link_libraries(timekeeper_tests PRIVATE Timekeeper::libTimekeeper GTest::gtest_main ${JSON_LIB})

include(GoogleTest)
gtest_discover_tests(timekeeper_tests)
