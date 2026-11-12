# Samples_Tests: unit tests for the samples' own code (score database, the samples' messages).

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Prism Drop's rules and Particle Life's and Wind Tunnel's simulations are plain code, compiled in directly.
add_executable(Samples_Tests ${SOURCES} "${PROJECT_SOURCE_DIR}/prism_drop/game/prism_board.cpp"
    "${PROJECT_SOURCE_DIR}/prism_drop/game/prism_autoplay.cpp"
    "${PROJECT_SOURCE_DIR}/particle_life/game/particle_life_sim.cpp"
    "${PROJECT_SOURCE_DIR}/wind_tunnel/game/wind_tunnel_sim.cpp"
    "${PROJECT_SOURCE_DIR}/wind_tunnel/game/wind_tunnel_shapes.cpp")

target_include_directories(Samples_Tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(Samples_Tests PRIVATE Moksha_TestFramework Samples_ScoreDB Samples_Shared)
