# Colony: an ant colony simulation, the test bed for the engine's multithreading (design: colony_design.md). Ants
# follow fading marker trails between their nest and food; every system is a range function scheduled in colony_step.
# The cell texture is a plain white tile.

moksha_add_game(Sample_Colony
    SOURCES
        game_main.cpp
        game/game_colony.cpp
        game/colony_ants.cpp
        game/colony_requests.cpp
        game/colony_scenario.cpp
        game/colony_step.cpp
        game/colony_systems.cpp
        game/colony_timing.cpp
        game/colony_world.cpp
        scene/scene_colony.cpp
        scene/scene_ui_colony.cpp
    RES_LIST resources/res_list.json
    HEADLESS
    DISPLAY_NAME "Colony"
    ORIENTATION LANDSCAPE)
