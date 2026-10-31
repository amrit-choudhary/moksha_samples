# Net test: connects to Samples_Server; ping, synced scores, global high score and a timed quiz.
# HEADLESS adds Sample_NetTest_Headless, which plays as a bot (load tests, see tools/load_test.sh).

moksha_add_game(Sample_NetTest
    SOURCES
        game_main.cpp
        game/game_net_test.cpp
        scene/scene_ui_net_test.cpp
    RES_LIST resources/res_list.json
    LIBRARIES Samples_Shared
    DISPLAY_NAME "Net Test"
    ORIENTATION LANDSCAPE
    HEADLESS)
