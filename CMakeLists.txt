cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
cmake_policy(VERSION 3.18.0)

project (Timekeeper
    VERSION 0.0.1
    LANGUAGES C CXX
    )

# Options — default to ON when consumed as the top-level project,
# OFF when included via add_subdirectory().
set(TIMEKEEPER_ENABLE_EXAMPLES   ${PROJECT_IS_TOP_LEVEL} CACHE BOOL "Build Timekeeper examples")
set(TIMEKEEPER_ENABLE_TESTS      ${PROJECT_IS_TOP_LEVEL} CACHE BOOL "Build Timekeeper tests")
set(TIMEKEEPER_FETCH_JSON        OFF       CACHE BOOL "Fetch nlohmann/json via FetchContent for json.hh support")

if(NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR} )
    add_subdirectory(src)
else()

    if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
        message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.")
    endif()

    set(CMAKE_CXX_STANDARD 17)

    # Optionally fetch nlohmann/json so that json.hh works without a system install.
    if(${TIMEKEEPER_FETCH_JSON})
        message(STATUS "Fetching nlohmann/json via FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG        v3.11.3
        )
        # Avoid installing the fetched package globally.
        set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(nlohmann_json)
    endif()

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF) # since CMake 3.16

    add_subdirectory(src)

    if(${TIMEKEEPER_ENABLE_EXAMPLES})
        add_subdirectory(examples)
    endif()

    if(${TIMEKEEPER_ENABLE_TESTS})
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
