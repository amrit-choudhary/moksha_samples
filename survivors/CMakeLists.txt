# Survivors: a one-screen arena where enemies swarm the player, raising the spawn rate until the engine
# struggles. Art: Kenney's Tiny Dungeon (CC0).

moksha_add_game(Sample_Survivors
    SOURCES
        game_main.cpp
        game/game_survivors.cpp
        scene/scene_survivors.cpp
    RES_LIST resources/res_list.json)
