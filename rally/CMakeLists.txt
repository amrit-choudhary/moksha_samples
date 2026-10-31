# Rally: two-player paddle ball over the network; Samples_Server runs the match. Portrait, one paddle per phone.
# HEADLESS adds Sample_Rally_Headless, a bot that plays a seat until a second human joins.

moksha_add_game(Sample_Rally
    SOURCES
        game_main.cpp
        game/game_rally.cpp
        scene/scene_rally.cpp
        scene/scene_ui_rally.cpp
    RES_LIST resources/res_list.json
    LIBRARIES Samples_Shared
    DISPLAY_NAME "Rally"
    ORIENTATION PORTRAIT
    HEADLESS)
