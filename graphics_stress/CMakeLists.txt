# Graphics Stress: a turning spiral galaxy that loads every render path, with buttons to raise each load until the frame
# rate drops. Textures are procedurally generated.

moksha_add_game(Sample_GraphicsStress
    SOURCES
        game_main.cpp
        game/game_graphics_stress.cpp
        scene/scene_graphics_stress.cpp
        scene/scene_ui_graphics_stress.cpp
    RES_LIST resources/res_list.json
    DISPLAY_NAME "Graphics Stress"
    ORIENTATION PORTRAIT)
