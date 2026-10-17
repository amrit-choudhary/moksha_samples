# Samples_Tests: unit tests for the samples' own code (score database, net test messages).

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(Samples_Tests ${SOURCES})

target_include_directories(Samples_Tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(Samples_Tests PRIVATE Moksha_TestFramework Samples_ScoreDB Samples_Shared)
