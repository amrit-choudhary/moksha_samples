# Samples_Tests: unit tests for the samples' own code (score database, the samples' messages).

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Prism Drop's rules are plain code, compiled in directly.
add_executable(Samples_Tests ${SOURCES} "${PROJECT_SOURCE_DIR}/prism_drop/game/prism_board.cpp"
    "${PROJECT_SOURCE_DIR}/prism_drop/game/prism_autoplay.cpp")

target_include_directories(Samples_Tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(Samples_Tests PRIVATE Moksha_TestFramework Samples_ScoreDB Samples_Shared)
