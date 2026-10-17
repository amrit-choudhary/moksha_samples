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
set(MOKSHA_PIN 2f5192c497e88b5532eeeab07743b5aff450ed03)

include(FetchContent)
FetchContent_Declare(moksha
    GIT_REPOSITORY https://github.com/amrit-choudhary/moksha.git
    GIT_TAG ${MOKSHA_PIN})
FetchContent_MakeAvailable(moksha)

add_subdirectory(ui_demo)
