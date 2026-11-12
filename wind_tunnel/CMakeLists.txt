# Wind Tunnel: an interactive 2D fluid demo after Ten Minute Physics #17. Air with smoke streaks flows past an obstacle
# (circle, square, plate, NACA airfoil, aircraft, car) that is dragged with the mouse or a finger. The fluid is drawn as
# one instanced quad per grid cell. The texture is generated (plain white).

moksha_add_game(Sample_WindTunnel
    SOURCES
        game_main.cpp
        game/game_wind_tunnel.cpp
        game/wind_tunnel_sim.cpp
        game/wind_tunnel_shapes.cpp
        scene/scene_wind_tunnel.cpp
        scene/scene_ui_wind_tunnel.cpp
    RES_LIST resources/res_list.json
    HEADLESS
    DISPLAY_NAME "Wind Tunnel"
    ORIENTATION LANDSCAPE)
