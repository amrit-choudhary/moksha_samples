# Moksha samples: small games that exercise the engine, one folder and executable each. All samples share one
# engine pin; the `live` preset (CMakeUserPresets.json) builds the local engine clone instead.

cmake_minimum_required(VERSION 3.31)

project(MokshaSamples LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The engine builds without exceptions; drop MSVC's default /EHsc so it doesn't clash with /EHs-c- (D9025).
if(MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# The engine commit every sample builds against. Bump it after pushing the engine changes the samples need.
set(MOKSHA_PIN 3e15b4311e5432860b3a4cd76afd2469fa231ac1)

include(FetchContent)
FetchContent_Declare(moksha
    GIT_REPOSITORY https://github.com/amrit-choudhary/moksha.git
    GIT_TAG ${MOKSHA_PIN})
FetchContent_MakeAvailable(moksha)

# Samples' own unit tests (Samples_Tests).
option(SAMPLES_BUILD_TESTS "Build the samples' unit tests" ON)

add_subdirectory(shared)
add_subdirectory(server)
add_subdirectory(ui_demo)
add_subdirectory(net_test)
add_subdirectory(evolution)
add_subdirectory(survivors)
add_subdirectory(systems_check)

if(SAMPLES_BUILD_TESTS)
    add_subdirectory(tests)
endif()
