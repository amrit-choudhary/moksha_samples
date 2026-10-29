# Tower Climb: an endless portrait climber, tap left or right to leap between the walls. The proof of concept for the
# mobile port. Art: Kenney's Tiny Dungeon (CC0).

moksha_add_game(Sample_TowerClimb
    SOURCES
        game_main.cpp
        game/game_tower_climb.cpp
        scene/scene_tower_climb.cpp
        scene/scene_ui_tower_climb.cpp
    RES_LIST resources/res_list.json)
