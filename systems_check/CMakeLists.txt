# Systems check: the engine's non-graphics systems (config, time, timers, files, random, physics, network, input,
# audio), each logged as PASS/FAIL/SKIP. Draws nothing, so it runs first on a platform without a renderer yet.
# HEADLESS adds Sample_SystemsCheck_Headless, which runs the automatic checks and quits.

moksha_add_game(Sample_SystemsCheck
    SOURCES
        game_main.cpp
        game/game_systems_check.cpp
        scene/scene_systems_check.cpp
    RES_LIST resources/res_list.json
    HEADLESS)
