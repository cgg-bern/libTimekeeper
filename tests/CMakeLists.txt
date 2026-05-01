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

add_executable(timekeeper_tests
    test_duration.cc
    test_stopwatch.cc
    test_hierarchical.cc
    test_scopedstopwatch.cc
    test_printing.cc
)
target_link_libraries(timekeeper_tests PRIVATE Timekeeper::libTimekeeper GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(timekeeper_tests)
