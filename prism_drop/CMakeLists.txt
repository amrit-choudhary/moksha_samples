# Prism Drop: a falling block puzzle for a portrait phone, with particles, animation and music. Every texture, sound
# and the music are procedurally generated.

moksha_add_game(Sample_PrismDrop
    SOURCES
        game_main.cpp
        game/game_prism_drop.cpp
        game/prism_autoplay.cpp
        game/prism_board.cpp
        game/prism_fx.cpp
        scene/scene_prism_drop.cpp
        scene/scene_ui_prism_drop.cpp
    RES_LIST resources/res_list.json
    HEADLESS
    DISPLAY_NAME "Prism Drop"
    ORIENTATION PORTRAIT)
