# UI demo: the engine's UI widgets, text and input on an empty scene. Uses only engine assets.

moksha_add_game(Sample_UIDemo
    SOURCES
        game_main.cpp
        game/game_ui_demo.cpp
        scene/scene_ui_demo.cpp
    RES_LIST resources/res_list.json)
