# Evolution: an environment simulation. Perlin-noise terrain, thousands of creatures as instanced atlas sprites,
# physics colliders; WASD moves the camera. Ships its own tileset.

moksha_add_game(Sample_Evolution
    SOURCES
        game_main.cpp
        game/creature.cpp
        game/game_evolution.cpp
        game/physics_scene_evolution.cpp
        scene/scene_evolution.cpp
        scene/scene_ui_evolution.cpp
    RES_LIST resources/res_list.json)
